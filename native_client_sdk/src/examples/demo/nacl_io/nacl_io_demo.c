/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "nacl_io_demo.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <pthread.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array.h"
#include "ppapi/c/ppb_var_dictionary.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "nacl_io/nacl_io.h"

#include "handlers.h"
#include "queue.h"

#if defined(WIN32)
#define va_copy(d, s) ((d) = (s))
#endif

typedef struct {
  const char* name;
  HandleFunc function;
} FuncNameMapping;

PP_Instance g_instance = 0;
PPB_GetInterface g_get_browser_interface = NULL;
PPB_Messaging* g_ppb_messaging = NULL;
PPB_Var* g_ppb_var = NULL;
PPB_VarArray* g_ppb_var_array = NULL;
PPB_VarDictionary* g_ppb_var_dictionary = NULL;

static FuncNameMapping g_function_map[] = {
    {"fopen", HandleFopen},
    {"fwrite", HandleFwrite},
    {"fread", HandleFread},
    {"fseek", HandleFseek},
    {"fclose", HandleFclose},
    {"fflush", HandleFflush},
    {"stat", HandleStat},
    {"opendir", HandleOpendir},
    {"readdir", HandleReaddir},
    {"closedir", HandleClosedir},
    {"mkdir", HandleMkdir},
    {"rmdir", HandleRmdir},
    {"chdir", HandleChdir},
    {"getcwd", HandleGetcwd},
    {"getaddrinfo", HandleGetaddrinfo},
    {"gethostbyname", HandleGethostbyname},
    {"connect", HandleConnect},
    {"send", HandleSend},
    {"recv", HandleRecv},
    {"close", HandleClose},
    {NULL, NULL},
};

/** A handle to the thread the handles messages. */
static pthread_t g_handle_message_thread;

/**
 * Create a new PP_Var from a C string.
 * @param[in] str The string to convert.
 * @return A new PP_Var with the contents of |str|.
 */
struct PP_Var CStrToVar(const char* str) {
  return g_ppb_var->VarFromUtf8(str, strlen(str));
}

/**
 * Printf to a newly allocated C string.
 * @param[in] format A printf format string.
 * @param[in] args The printf arguments.
 * @return The newly constructed string. Caller takes ownership. */
char* VprintfToNewString(const char* format, va_list args) {
  va_list args_copy;
  int length;
  char* buffer;
  int result;

  va_copy(args_copy, args);
  length = vsnprintf(NULL, 0, format, args);
  buffer = (char*)malloc(length + 1); /* +1 for NULL-terminator. */
  result = vsnprintf(&buffer[0], length + 1, format, args_copy);
  if (result != length) {
    assert(0);
    return NULL;
  }
  return buffer;
}

/**
 * Printf to a newly allocated C string.
 * @param[in] format A print format string.
 * @param[in] ... The printf arguments.
 * @return The newly constructed string. Caller takes ownership.
 */
char* PrintfToNewString(const char* format, ...) {
  va_list args;
  char* result;
  va_start(args, format);
  result = VprintfToNewString(format, args);
  va_end(args);
  return result;
}

/**
 * Printf to a new PP_Var.
 * @param[in] format A print format string.
 * @param[in] ... The printf arguments.
 * @return A new PP_Var.
 */
struct PP_Var PrintfToVar(const char* format, ...) {
  char* string;
  va_list args;
  struct PP_Var var;

  va_start(args, format);
  string = VprintfToNewString(format, args);
  va_end(args);

  var = g_ppb_var->VarFromUtf8(string, strlen(string));
  free(string);

  return var;
}

/**
 * Convert a PP_Var to a C string, given a buffer.
 * @param[in] var The PP_Var to convert.
 * @param[out] buffer The buffer to write to.
 * @param[in] length The length of |buffer|.
 * @return The number of characters written.
 */
const char* VarToCStr(struct PP_Var var) {
  uint32_t length;
  const char* str = g_ppb_var->VarToUtf8(var, &length);
  if (str == NULL) {
    return NULL;
  }

  /* str is NOT NULL-terminated. Copy using memcpy. */
  char* new_str = (char*)malloc(length + 1);
  memcpy(new_str, str, length);
  new_str[length] = 0;
  return new_str;
}

/**
 * Get a value from a Dictionary, given a string key.
 * @param[in] dict The dictionary to look in.
 * @param[in] key The key to look up.
 * @return PP_Var The value at |key| in the |dict|. If the key doesn't exist,
 *     return a PP_Var with the undefined value.
 */
struct PP_Var GetDictVar(struct PP_Var dict, const char* key) {
  struct PP_Var key_var = CStrToVar(key);
  struct PP_Var value = g_ppb_var_dictionary->Get(dict, key_var);
  g_ppb_var->Release(key_var);
  return value;
}

/**
 * Send a newly-created PP_Var to JavaScript, then release it.
 * @param[in] var The PP_Var to send.
 */
static void PostMessageVar(struct PP_Var var) {
  g_ppb_messaging->PostMessage(g_instance, var);
  g_ppb_var->Release(var);
}

/**
 * Given a message from JavaScript, parse it for functions and parameters.
 *
 * The format of the message is:
 * {
 *  "cmd": <function name>,
 *  "args": [<arg0>, <arg1>, ...]
 * }
 *
 * @param[in] message The message to parse.
 * @param[out] out_function The function name.
 * @param[out] out_params A PP_Var array.
 * @return 0 if successful, otherwise 1.
 */
static int ParseMessage(struct PP_Var message,
                        const char** out_function,
                        struct PP_Var* out_params) {
  if (message.type != PP_VARTYPE_DICTIONARY) {
    return 1;
  }

  struct PP_Var cmd_value = GetDictVar(message, "cmd");
  *out_function = VarToCStr(cmd_value);
  g_ppb_var->Release(cmd_value);
  if (cmd_value.type != PP_VARTYPE_STRING) {
    return 1;
  }

  *out_params = GetDictVar(message, "args");
  if (out_params->type != PP_VARTYPE_ARRAY) {
    return 1;
  }

  return 0;
}

/**
 * Given a function name, look up its handler function.
 * @param[in] function_name The function name to look up.
 * @return The handler function mapped to |function_name|.
 */
static HandleFunc GetFunctionByName(const char* function_name) {
  FuncNameMapping* map_iter = g_function_map;
  for (; map_iter->name; ++map_iter) {
    if (strcmp(map_iter->name, function_name) == 0) {
      return map_iter->function;
    }
  }

  return NULL;
}

/**
 * Handle as message from JavaScript on the worker thread.
 *
 * @param[in] message The message to parse and handle.
 */
static void HandleMessage(struct PP_Var message) {
  const char* function_name;
  struct PP_Var params;
  if (ParseMessage(message, &function_name, &params)) {
    PostMessageVar(CStrToVar("Error: Unable to parse message"));
    return;
  }

  HandleFunc function = GetFunctionByName(function_name);
  if (!function) {
    /* Function name wasn't found. Error. */
    PostMessageVar(
        PrintfToVar("Error: Unknown function \"%s\"", function_name));
    return;
  }

  /* Function name was found, call it. */
  struct PP_Var result_var;
  const char* error;
  int result = (*function)(params, &result_var, &error);
  if (result != 0) {
    /* Error. */
    struct PP_Var var;
    if (error != NULL) {
      var = PrintfToVar("Error: \"%s\" failed: %s.", function_name, error);
      free((void*)error);
    } else {
      var = PrintfToVar("Error: \"%s\" failed.", function_name);
    }

    /* Post the error to JavaScript, so the user can see it. */
    PostMessageVar(var);
    return;
  }

  /* Function returned an output dictionary. Send it to JavaScript. */
  PostMessageVar(result_var);
  g_ppb_var->Release(result_var);
}

/**
 * A worker thread that handles messages from JavaScript.
 * @param[in] user_data Unused.
 * @return unused.
 */
void* HandleMessageThread(void* user_data) {
  while (1) {
    struct PP_Var message = DequeueMessage();
    HandleMessage(message);
    g_ppb_var->Release(message);
  }
}

static PP_Bool Instance_DidCreate(PP_Instance instance,
                                  uint32_t argc,
                                  const char* argn[],
                                  const char* argv[]) {
  g_instance = instance;
  nacl_io_init_ppapi(instance, g_get_browser_interface);

  // By default, nacl_io mounts / to pass through to the original NaCl
  // filesystem (which doesn't do much). Let's remount it to a memfs
  // filesystem.
  umount("/");
  mount("", "/", "memfs", 0, "");

  mount("",                                       /* source */
        "/persistent",                            /* target */
        "html5fs",                                /* filesystemtype */
        0,                                        /* mountflags */
        "type=PERSISTENT,expected_size=1048576"); /* data */

  mount("",       /* source. Use relative URL */
        "/http",  /* target */
        "httpfs", /* filesystemtype */
        0,        /* mountflags */
        "");      /* data */

  pthread_create(&g_handle_message_thread, NULL, &HandleMessageThread, NULL);
  InitializeMessageQueue();

  return PP_TRUE;
}

static void Instance_DidDestroy(PP_Instance instance) {
}

static void Instance_DidChangeView(PP_Instance instance,
                                   PP_Resource view_resource) {
}

static void Instance_DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {
}

static PP_Bool Instance_HandleDocumentLoad(PP_Instance instance,
                                           PP_Resource url_loader) {
  /* NaCl modules do not need to handle the document load function. */
  return PP_FALSE;
}

static void Messaging_HandleMessage(PP_Instance instance,
                                    struct PP_Var message) {
  g_ppb_var->AddRef(message);
  if (!EnqueueMessage(message)) {
    g_ppb_var->Release(message);
    PostMessageVar(
        PrintfToVar("Warning: dropped message because the queue was full."));
  }
}

#define GET_INTERFACE(var, type, name)            \
  var = (type*)(get_browser(name));               \
  if (!var) {                                     \
    printf("Unable to get interface " name "\n"); \
    return PP_ERROR_FAILED;                       \
  }

PP_EXPORT int32_t PPP_InitializeModule(PP_Module a_module_id,
                                       PPB_GetInterface get_browser) {
  g_get_browser_interface = get_browser;
  GET_INTERFACE(g_ppb_messaging, PPB_Messaging, PPB_MESSAGING_INTERFACE);
  GET_INTERFACE(g_ppb_var, PPB_Var, PPB_VAR_INTERFACE);
  GET_INTERFACE(g_ppb_var_array, PPB_VarArray, PPB_VAR_ARRAY_INTERFACE);
  GET_INTERFACE(
      g_ppb_var_dictionary, PPB_VarDictionary, PPB_VAR_DICTIONARY_INTERFACE);
  return PP_OK;
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0) {
    static PPP_Instance instance_interface = {
        &Instance_DidCreate,
        &Instance_DidDestroy,
        &Instance_DidChangeView,
        &Instance_DidChangeFocus,
        &Instance_HandleDocumentLoad,
    };
    return &instance_interface;
  } else if (strcmp(interface_name, PPP_MESSAGING_INTERFACE) == 0) {
    static PPP_Messaging messaging_interface = {
        &Messaging_HandleMessage,
    };
    return &messaging_interface;
  }
  return NULL;
}

PP_EXPORT void PPP_ShutdownModule() {
}
