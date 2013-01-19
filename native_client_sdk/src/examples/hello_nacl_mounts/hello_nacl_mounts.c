/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hello_nacl_mounts.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "nacl_mounts/nacl_mounts.h"

#include "handlers.h"
#include "queue.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#if defined(WIN32)
#define va_copy(d, s) ((d) = (s))
#endif

typedef struct {
  const char* name;
  HandleFunc function;
} FuncNameMapping;


static PP_Instance g_instance = 0;
static PPB_GetInterface get_browser_interface = NULL;
static PPB_Messaging* ppb_messaging_interface = NULL;
static PPB_Var* ppb_var_interface = NULL;

static FuncNameMapping g_function_map[] = {
  { "fopen", HandleFopen },
  { "fwrite", HandleFwrite },
  { "fread", HandleFread },
  { "fseek", HandleFseek },
  { "fclose", HandleFclose },
  { NULL, NULL },
};

/** A handle to the thread the handles messages. */
static pthread_t g_handle_message_thread;

/**
 * Create a new PP_Var from a C string.
 * @param[in] str The string to convert.
 * @return A new PP_Var with the contents of |str|. */
struct PP_Var CStrToVar(const char* str) {
  if (ppb_var_interface != NULL) {
    return ppb_var_interface->VarFromUtf8(str, strlen(str));
  }
  return PP_MakeUndefined();
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
  buffer = (char*)malloc(length + 1);  /* +1 for NULL-terminator. */
  result = vsnprintf(&buffer[0], length + 1, format, args_copy);
  assert(result == length);
  return buffer;
}

/**
 * Printf to a newly allocated C string.
 * @param[in] format A print format string.
 * @param[in] ... The printf arguments.
 * @return The newly constructed string. Caller takes ownership. */
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
 * @return A new PP_Var. */
struct PP_Var PrintfToVar(const char* format, ...) {
  if (ppb_var_interface != NULL) {
    char* string;
    va_list args;
    struct PP_Var var;

    va_start(args, format);
    string = VprintfToNewString(format, args);
    va_end(args);

    var = ppb_var_interface->VarFromUtf8(string, strlen(string));
    free(string);

    return var;
  }

  return PP_MakeUndefined();
}

/**
 * Convert a PP_Var to a C string, given a buffer.
 * @param[in] var The PP_Var to convert.
 * @param[out] buffer The buffer to write to.
 * @param[in] length The length of |buffer|.
 * @return The number of characters written. */
uint32_t VarToCStr(struct PP_Var var, char* buffer, uint32_t length) {
  if (ppb_var_interface != NULL) {
    uint32_t var_length;
    const char* str = ppb_var_interface->VarToUtf8(var, &var_length);
    /* str is NOT NULL-terminated. Copy using memcpy. */
    uint32_t min_length = MIN(var_length, length - 1);
    memcpy(buffer, str, min_length);
    buffer[min_length] = 0;

    return min_length;
  }

  return 0;
}

/**
 * Given a message from JavaScript, parse it for functions and parameters.
 *
 * The format of the message is:
 *   function, param1, param2, param3, etc.
 * where each element is separated by the \1 character.
 *
 * e.g.
 *   "function\1first parameter\1second parameter"
 *
 * How to use:
 *   char* function;
 *   char* params[4];
 *   int num_params = ParseMessage(msg, &function, &params, 4);
 *
 * @param[in, out] message The message to parse. This string is modified
 *     in-place.
 * @param[out] out_function The function name.
 * @param[out] out_params An array of strings, one for each parameter parsed.
 * @param[in] max_params The maximum number of parameters to parse.
 * @return The number of parameters parsed. */
static size_t ParseMessage(char* message,
                           char** out_function,
                           char** out_params,
                           size_t max_params) {
  char* separator;
  char* param_start;
  size_t num_params = 0;

  /* Parse the message: function\1param1\1param2\1param3,... */
  *out_function = &message[0];

  separator = strchr(message, 1);
  if (!separator) {
    return num_params;
  }

  *separator = 0;  /* NULL-terminate function. */

  while (separator && num_params < max_params) {
    param_start = separator + 1;
    separator = strchr(param_start, 1);
    if (separator) {
      *separator = 0;
      out_params[num_params++] = param_start;
    }
  }

  out_params[num_params++] = param_start;

  return num_params;
}

/**
 * Given a function name, look up its handler function.
 * @param[in] function_name The function name to look up.
 * @return The handler function mapped to |function_name|. */
static HandleFunc GetFunctionByName(const char* function_name) {
  FuncNameMapping* map_iter = g_function_map;
  for (; map_iter->name; ++map_iter) {
    if (strcmp(map_iter->name, function_name) == 0) {
      return map_iter->function;
    }
  }

  return NULL;
}

/** Handle as message from JavaScript on the worker thread.
 *
 * @param[in] message The message to parse and handle. */
static void HandleMessage(char* message) {
  char* function_name;
  char* params[MAX_PARAMS];
  size_t num_params;
  char* output = NULL;
  int result;
  HandleFunc function;

  num_params = ParseMessage(message, &function_name, &params[0], MAX_PARAMS);

  function = GetFunctionByName(function_name);
  if (!function) {
    /* Function name wasn't found. Error. */
    ppb_messaging_interface->PostMessage(
        g_instance, PrintfToVar("Error: Unknown function \"%s\"", function));
  }

  /* Function name was found, call it. */
  result = (*function)(num_params, &params[0], &output);
  if (result != 0) {
    /* Error. */
    struct PP_Var var;
    if (output != NULL) {
      var = PrintfToVar(
          "Error: Function \"%s\" returned error %d. "
          "Additional output: %s", function_name, result, output);
      free(output);
    } else {
      var = PrintfToVar("Error: Function \"%s\" returned error %d.",
                        function_name, result);
    }

    /* Post the error to JavaScript, so the user can see it. */
    ppb_messaging_interface->PostMessage(g_instance, var);
    return;
  }

  if (output != NULL) {
    /* Function returned an output string. Send it to JavaScript. */
    ppb_messaging_interface->PostMessage(g_instance, CStrToVar(output));
    free(output);
  }
}

/** A worker thread that handles messages from JavaScript.
 * @param[in] user_data Unused.
 * @return unused. */
void* HandleMessageThread(void* user_data) {
  while (1) {
    char* message = DequeueMessage();
    HandleMessage(message);
    free(message);
  }
}


static PP_Bool Instance_DidCreate(PP_Instance instance,
                                  uint32_t argc,
                                  const char* argn[],
                                  const char* argv[]) {
  g_instance = instance;
  nacl_mounts_init_ppapi(instance, get_browser_interface);
  mount(
      "",  /* source */
      "/persistent",  /* target */
      "html5fs",  /* filesystemtype */
      0,  /* mountflags */
      "type=PERSISTENT,expected_size=1048576");  /* data */

  mount(
      "",  /* source. Use relative URL */
      "/http",  /* target */
      "httpfs",  /* filesystemtype */
      0,  /* mountflags */
      "");  /* data */

  pthread_create(&g_handle_message_thread, NULL, &HandleMessageThread, NULL);
  InitializeMessageQueue();

  return PP_TRUE;
}


static void Instance_DidDestroy(PP_Instance instance) {
}

static void Instance_DidChangeView(PP_Instance instance,
                                   PP_Resource view_resource) {
}

static void Instance_DidChangeFocus(PP_Instance instance,
                                    PP_Bool has_focus) {
}

static PP_Bool Instance_HandleDocumentLoad(PP_Instance instance,
                                           PP_Resource url_loader) {
  /* NaCl modules do not need to handle the document load function. */
  return PP_FALSE;
}

static void Messaging_HandleMessage(PP_Instance instance,
                                    struct PP_Var message) {
  char buffer[1024];
  VarToCStr(message, &buffer[0], 1024);
  if (!EnqueueMessage(strdup(buffer))) {
    struct PP_Var var;
    var = PrintfToVar(
        "Warning: dropped message \"%s\" because the queue was full.",
        message);
    ppb_messaging_interface->PostMessage(g_instance, var);
  }
}

PP_EXPORT int32_t PPP_InitializeModule(PP_Module a_module_id,
                                       PPB_GetInterface get_browser) {
  get_browser_interface = get_browser;
  ppb_messaging_interface =
      (PPB_Messaging*)(get_browser(PPB_MESSAGING_INTERFACE));
  ppb_var_interface = (PPB_Var*)(get_browser(PPB_VAR_INTERFACE));
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
