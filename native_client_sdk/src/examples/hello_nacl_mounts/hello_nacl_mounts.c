/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "nacl_mounts/kernel_intercept.h"


#define MAX_OPEN_FILES 10
#define MAX_PARAMS 4
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#if defined(WIN32)
#define va_copy(d, s) ((d) = (s))
#endif


typedef int (*HandleFunc)(int num_params, char** params, char** output);
typedef struct {
  const char* name;
  HandleFunc function;
} FuncNameMapping;


int HandleFopen(int num_params, char** params, char** output);
int HandleFwrite(int num_params, char** params, char** output);
int HandleFread(int num_params, char** params, char** output);
int HandleFseek(int num_params, char** params, char** output);
int HandleFclose(int num_params, char** params, char** output);


static PPB_Messaging* ppb_messaging_interface = NULL;
static PPB_Var* ppb_var_interface = NULL;
static FILE* g_OpenFiles[MAX_OPEN_FILES];
static FuncNameMapping g_FunctionMap[] = {
  { "fopen", HandleFopen },
  { "fwrite", HandleFwrite },
  { "fread", HandleFread },
  { "fseek", HandleFseek },
  { "fclose", HandleFclose },
  { NULL, NULL },
};


static struct PP_Var CStrToVar(const char* str) {
  if (ppb_var_interface != NULL) {
    return ppb_var_interface->VarFromUtf8(str, strlen(str));
  }
  return PP_MakeUndefined();
}

static char* VprintfToNewString(const char* format, va_list args) {
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

static char* PrintfToNewString(const char* format, ...) {
  va_list args;
  char* result;
  va_start(args, format);
  result = VprintfToNewString(format, args);
  va_end(args);
  return result;
}

static struct PP_Var PrintfToVar(const char* format, ...) {
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

static uint32_t VarToCStr(struct PP_Var var, char* buffer, uint32_t length) {
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

static int AddFileToMap(FILE* file) {
  int i;
  assert(file != NULL);
  for (i = 0; i < MAX_OPEN_FILES; ++i) {
    if (g_OpenFiles[i] == NULL) {
      g_OpenFiles[i] = file;
      return i;
    }
  }

  return -1;
}

static void RemoveFileFromMap(int i) {
  assert(i >= 0 && i < MAX_OPEN_FILES);
  g_OpenFiles[i] = NULL;
}

static FILE* GetFileFromMap(int i) {
  assert(i >= 0 && i < MAX_OPEN_FILES);
  return g_OpenFiles[i];
}

static FILE* GetFileFromIndexString(const char* s, int* file_index) {
  char* endptr;
  int result = strtol(s, &endptr, 10);
  if (endptr != s + strlen(s)) {
    /* Garbage at the end of the number...? */
    return NULL;
  }

  if (file_index)
    *file_index = result;

  return GetFileFromMap(result);
}

int HandleFopen(int num_params, char** params, char** output) {
  FILE* file;
  int file_index;
  const char* filename;
  const char* mode;

  if (num_params != 2) {
    *output = PrintfToNewString("Error: fopen takes 2 parameters.");
    return 1;
  }

  filename = params[0];
  mode = params[1];

  file = fopen(filename, mode);
  if (!file) {
    *output = PrintfToNewString("Error: fopen returned a NULL FILE*.");
    return 2;
  }

  file_index = AddFileToMap(file);
  if (file_index == -1) {
    *output = PrintfToNewString(
        "Error: Example only allows %d open file handles.", MAX_OPEN_FILES);
    return 3;
  }

  *output = PrintfToNewString("fopen\1%s\1%d", filename, file_index);
  return 0;
}

int HandleFwrite(int num_params, char** params, char** output) {
  FILE* file;
  const char* file_index_string;
  const char* data;
  size_t data_len;
  size_t bytes_written;

  if (num_params != 2) {
    *output = PrintfToNewString("Error: fwrite takes 2 parameters.");
    return 1;
  }

  file_index_string = params[0];
  file = GetFileFromIndexString(file_index_string, NULL);
  data = params[1];
  data_len = strlen(data);

  if (!file) {
    *output = PrintfToNewString("Error: Unknown file handle %s.",
                                file_index_string);
    return 2;
  }

  bytes_written = fwrite(data, 1, data_len, file);

  *output = PrintfToNewString("fwrite\1%s\1%d", file_index_string,
                              bytes_written);
  return 0;
}

int HandleFread(int num_params, char** params, char** output) {
  FILE* file;
  const char* file_index_string;
  char* buffer;
  size_t data_len;
  size_t bytes_read;

  if (num_params != 2) {
    *output = PrintfToNewString("Error: fread takes 2 parameters.");
    return 1;
  }

  file_index_string = params[0];
  file = GetFileFromIndexString(file_index_string, NULL);
  data_len = strtol(params[1], NULL, 10);

  if (!file) {
    *output = PrintfToNewString("Error: Unknown file handle %s.",
                                file_index_string);
    return 2;
  }

  buffer = (char*)malloc(data_len + 1);
  bytes_read = fread(buffer, 1, data_len, file);
  buffer[bytes_read] = 0;

  *output = PrintfToNewString("fread\1%s\1%s", file_index_string, buffer);
  free(buffer);
  return 0;
}

int HandleFseek(int num_params, char** params, char** output) {
  FILE* file;
  const char* file_index_string;
  long offset;
  int whence;
  int result;

  if (num_params != 3) {
    *output = PrintfToNewString("Error: fseek takes 3 parameters.");
    return 1;
  }

  file_index_string = params[0];
  file = GetFileFromIndexString(file_index_string, NULL);
  offset = strtol(params[1], NULL, 10);
  whence = strtol(params[2], NULL, 10);

  if (!file) {
    *output = PrintfToNewString("Error: Unknown file handle %s.",
                                file_index_string);
    return 2;
  }

  result = fseek(file, offset, whence);
  if (result) {
    *output = PrintfToNewString("Error: fseek returned error %d.", result);
    return 3;
  }

  *output = PrintfToNewString("fseek\1%d", file_index_string);
  return 0;
}

int HandleFclose(int num_params, char** params, char** output) {
  FILE* file;
  int file_index;
  const char* file_index_string;
  int result;

  if (num_params != 1) {
    *output = PrintfToNewString("Error: fclose takes 1 parameters.");
    return 1;
  }

  file_index_string = params[0];
  file = GetFileFromIndexString(file_index_string, &file_index);
  if (!file) {
    *output = PrintfToNewString("Error: Unknown file handle %s.",
                                file_index_string);
    return 2;
  }

  result = fclose(file);
  if (result) {
    *output = PrintfToNewString("Error: fclose returned error %d.", result);
    return 3;
  }

  RemoveFileFromMap(file_index);

  *output = PrintfToNewString("fclose\1%s", file_index_string);
  return 0;
}

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

static HandleFunc GetFunctionByName(const char* function_name) {
  FuncNameMapping* map_iter = g_FunctionMap;
  for (; map_iter->name; ++map_iter) {
    if (strcmp(map_iter->name, function_name) == 0) {
      return map_iter->function;
    }
  }

  return NULL;
}


static PP_Bool Instance_DidCreate(PP_Instance instance,
                                  uint32_t argc,
                                  const char* argn[],
                                  const char* argv[]) {

  ki_init(NULL);
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
  char* function_name;
  char* params[MAX_PARAMS];
  size_t num_params;
  char* output = NULL;
  int result;
  HandleFunc function;

  VarToCStr(message, &buffer[0], 1024);

  num_params = ParseMessage(buffer, &function_name, &params[0], MAX_PARAMS);

  function = GetFunctionByName(function_name);
  if (!function) {
    /* Function name wasn't found. Error. */
    ppb_messaging_interface->PostMessage(
        instance, PrintfToVar("Error: Unknown function \"%s\"", function));
  }

  result = (*function)(num_params, &params[0], &output);
  if (result != 0) {
    /* Error. */
    struct PP_Var var;
    if (output != NULL) {
      var = PrintfToVar(
          "Error: Function \"%s\" returned error %d. "
          "Additional output: %s", function_name, result, output);
    } else {
      var = PrintfToVar("Error: Function \"%s\" returned error %d.",
                        function_name, result);
    }

    ppb_messaging_interface->PostMessage(instance, var);
    return;
  }

  if (output != NULL) {
    /* Function returned an output string. Send it to JavaScript. */
    ppb_messaging_interface->PostMessage(instance, CStrToVar(output));
    free(output);
  }
}

PP_EXPORT int32_t PPP_InitializeModule(PP_Module a_module_id,
                                       PPB_GetInterface get_browser) {
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
