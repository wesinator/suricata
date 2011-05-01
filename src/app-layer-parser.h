/* Copyright (C) 2007-2010 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * \file
 *
 * \author Victor Julien <victor@inliniac.net>
 */

#ifndef __APP_LAYER_PARSER_H__
#define __APP_LAYER_PARSER_H__

/** Mapping between local parser id's (e.g. HTTP_FIELD_REQUEST_URI) and
  * the dynamically assigned (at registration) global parser id. */
typedef struct AppLayerLocalMap_ {
    uint16_t parser_id;
} AppLayerLocalMap;

/** \brief Mapping between ALPROTO_* and L7Parsers
 *
 * Map the proto to the parsers for the to_client and to_server directions.
 */
typedef struct AppLayerProto_ {
    char *name; /**< name of the registered proto */

    uint16_t to_server;
    uint16_t to_client;
    uint8_t storage_id;

    AppLayerLocalMap **map;
    uint16_t map_size;

    void *(*StateAlloc)(void);
    void (*StateFree)(void *);
    void (*StateUpdateTransactionId)(void *, uint16_t *);
    void (*StateTransactionFree)(void *, uint16_t);

    char logger; /**< does this proto have a logger enabled? */
} AppLayerProto;

/** flags for the result elmts */
#define ALP_RESULT_ELMT_ALLOC 0x01

/** \brief Result elements for the parser */
typedef struct AppLayerParserResultElmt_ {
    uint16_t flags; /* flags. E.g. local alloc */
    uint16_t name_idx; /* idx for names like "http.request_line.uri" */

    uint32_t data_len; /* length of the data from the ptr */
    uint8_t  *data_ptr; /* point to the position in the "input" data
                          * or ptr to new mem if local alloc flag set */
    struct AppLayerParserResultElmt_ *next;
} AppLayerParserResultElmt;

/** \brief List head for parser result elmts */
typedef struct AppLayerParserResult_ {
    AppLayerParserResultElmt *head;
    AppLayerParserResultElmt *tail;
    uint32_t cnt;
} AppLayerParserResult;

#define APP_LAYER_PARSER_USE            0x01
#define APP_LAYER_PARSER_EOF            0x02
#define APP_LAYER_PARSER_DONE           0x04    /**< parser is done, ignore more
                                                     msgs */
#define APP_LAYER_PARSER_NO_INSPECTION  0x08    /**< Flag to indicate no more
                                                     packets payload inspection */
#define APP_LAYER_PARSER_NO_REASSEMBLY  0x10    /**< Flag to indicate no more
                                                     packets reassembly for this
                                                     session */

#define APP_LAYER_TRANSACTION_EOF       0x01    /**< Session done, last transaction
                                                     as well */
#define APP_LAYER_TRANSACTION_TOSERVER  0x02    /**< transaction has been inspected
                                                     in to server direction. */
#define APP_LAYER_TRANSACTION_TOCLIENT  0x04    /**< transaction has been inspected
                                                     in to server direction. */

typedef struct AppLayerParserState_ {
    uint8_t flags;
    uint16_t cur_parser; /**< idx of currently active parser */
    uint8_t *store;
    uint32_t store_len;
    uint16_t parse_field;
} AppLayerParserState;

typedef struct AppLayerParserStateStore_ {
    AppLayerParserState to_client;
    AppLayerParserState to_server;

    /** flags related to the id's */
    uint8_t id_flags;

    /** the highest id of inspected state's (i.e. http transactions), updated by
     *  the stateful detection engine code */
    uint16_t inspect_id;
    /** the highest id of logged state's (i.e. http transactions), updated by
     *  a logging module throught the app layer API */
    uint16_t logged_id;
    /** the higest id of available state's, updated by the app layer parser */
    uint16_t avail_id;
    /** the base id signifies the id number of the oldest id we have in our
     *  state. As transactions may be cleaned up before the entire state is
     *  freed, id's may "disappear". */
    uint16_t base_id;
} AppLayerParserStateStore;

typedef struct AppLayerParserTableElement_ {
    char *name;
    uint16_t proto;
    uint16_t parser_local_id; /** local id of the parser in the parser itself. */
    uint8_t flags;
    int (*AppLayerParser)(Flow *f, void *protocol_state, AppLayerParserState
                          *parser_state, uint8_t *input, uint32_t input_len,
                          AppLayerParserResult *output);
    uint16_t max_outputs; /* rationele is that if we know the max outputs of all
                             parsers, we can statically define our output array
                             to be a certain size */
} AppLayerParserTableElement;

typedef struct AppLayerProbingParserElement_ {
    const char *al_proto_name;
    uint16_t al_proto;
    uint16_t port;
    uint8_t priority;
    uint8_t top;
    /* the min length of data that has to be supplied to invoke the parser */
    uint32_t min_depth;
    /* the max length of data after which this parser won't be invoked */
    uint32_t max_depth;
    /* the probing parser function */
    uint16_t (*ProbingParser)(uint8_t *input, uint32_t input_len);

    struct AppLayerProbingParserElement_ *next;
} AppLayerProbingParserElement;

typedef struct AppLayerProbingParser_ {
    /* the port no for which probing parser(s) are invoked */
    uint16_t port;
    /* the max depth for all the probing parsers registered for this port */
    uint16_t toserver_max_depth;
    uint16_t toclient_max_depth;

    AppLayerProbingParserElement *toserver;
    AppLayerProbingParserElement *toclient;

    struct AppLayerProbingParser_ *next;
} AppLayerProbingParser;

#define APP_LAYER_PROBING_PARSER_PRIORITY_HIGH   1
#define APP_LAYER_PROBING_PARSER_PRIORITY_MEDIUM 2
#define APP_LAYER_PROBING_PARSER_PRIORITY_LOW    3

extern AppLayerProbingParser *probing_parsers;

static inline
AppLayerProbingParser *AppLayerGetProbingParsers(AppLayerProbingParser *probing_parsers,
                                                 uint16_t ip_proto,
                                                 uint16_t port)
{
    if (probing_parsers == NULL)
        return NULL;

    AppLayerProbingParser *pp = probing_parsers;
    while (pp != NULL) {
        if (pp->port == port) {
            break;
        }
        pp = pp->next;
    }

    return pp;
}

/* prototypes */
void AppLayerParsersInitPostProcess(void);
void RegisterAppLayerParsers(void);

int AppLayerRegisterProto(char *name, uint8_t proto, uint8_t flags,
                          int (*AppLayerParser)(Flow *f, void *protocol_state,
                          AppLayerParserState *parser_state, uint8_t *input,
                          uint32_t input_len, AppLayerParserResult *output));
int AppLayerRegisterParser(char *name, uint16_t proto, uint16_t parser_id,
                           int (*AppLayerParser)(Flow *f, void *protocol_state,
                           AppLayerParserState *parser_state, uint8_t *input,
                           uint32_t input_len, AppLayerParserResult *output),
                           char *dependency);
void AppLayerRegisterProbingParser(AppLayerProbingParser **, uint16_t, uint16_t,
                                   const char *, uint16_t,
                                   uint16_t, uint16_t, uint8_t, uint8_t,
                                   uint8_t, uint16_t (*ProbingParser)
                                   (uint8_t *, uint32_t));
void AppLayerRegisterStateFuncs(uint16_t proto, void *(*StateAlloc)(void),
                                void (*StateFree)(void *));
void AppLayerRegisterTransactionIdFuncs(uint16_t proto,
        void (*StateTransactionId)(void *, uint16_t *),
        void (*StateTransactionFree)(void *, uint16_t id));
void AppLayerRegisterLogger(uint16_t proto);

int AppLayerParse(Flow *, uint8_t proto, uint8_t flags, uint8_t *input,
                  uint32_t input_len);

int AlpParseFieldBySize(AppLayerParserResult *, AppLayerParserState *, uint16_t,
                        uint32_t, uint8_t *, uint32_t, uint32_t *);
int AlpParseFieldByEOF(AppLayerParserResult *, AppLayerParserState *, uint16_t,
                       uint8_t *, uint32_t);
int AlpParseFieldByDelimiter(AppLayerParserResult *, AppLayerParserState *,
                             uint16_t, const uint8_t *, uint8_t, uint8_t *,
                             uint32_t, uint32_t *);
uint16_t AlpGetStateIdx(uint16_t);

uint16_t AppLayerGetProtoByName(const char *);

int AppLayerTransactionUpdateInspectId(Flow *, char);
void AppLayerTransactionUpdateLoggedId(Flow *);

int AppLayerTransactionGetLoggableId(Flow *f);
int AppLayerTransactionGetLoggedId(Flow *f);
int AppLayerTransactionGetBaseId(Flow *f);
int AppLayerTransactionGetInspectId(Flow *f);

void AppLayerParserRegisterTests(void);

void AppLayerParserCleanupState(Flow *);

uint8_t AppLayerRegisterModule(void);
uint8_t AppLayerGetStorageSize(void);
void AppLayerFreeProbingParsers(AppLayerProbingParser *);


#endif /* __APP_LAYER_PARSER_H__ */

