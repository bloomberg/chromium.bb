/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "components/nacl/renderer/plugin/utility.h"
#include "ppapi/cpp/module.h"

namespace plugin {

int gNaClPluginDebugPrintEnabled = -1;

/*
 * Prints formatted message to the log.
 */
int NaClPluginPrintLog(const char *format, ...) {
  va_list arg;
  int out_size;

  static const int kStackBufferSize = 512;
  char stack_buffer[kStackBufferSize];

  // Just log locally to stderr if we can't use the nacl interface.
  if (!GetNaClInterface()) {
    va_start(arg, format);
    int rc = vfprintf(stderr, format, arg);
    va_end(arg);
    return rc;
  }

  va_start(arg, format);
  out_size = vsnprintf(stack_buffer, kStackBufferSize, format, arg);
  va_end(arg);
  if (out_size < kStackBufferSize) {
    GetNaClInterface()->Vlog(stack_buffer);
  } else {
    // Message too large for our stack buffer; we have to allocate memory
    // instead.
    char *buffer = static_cast<char*>(malloc(out_size + 1));
    va_start(arg, format);
    vsnprintf(buffer, out_size + 1, format, arg);
    va_end(arg);
    GetNaClInterface()->Vlog(buffer);
    free(buffer);
  }
  return out_size;
}

/*
 * Currently looks for presence of NACL_PLUGIN_DEBUG and returns
 * 0 if absent and 1 if present.  In the future we may include notions
 * of verbosity level.
 */
int NaClPluginDebugPrintCheckEnv() {
  char* env = getenv("NACL_PLUGIN_DEBUG");
  return (NULL != env);
}

// We cache the NaCl interface pointer and ensure that its set early on, on the
// main thread. This allows GetNaClInterface() to be used from non-main threads.
static const PPB_NaCl_Private* g_nacl_interface = NULL;

const PPB_NaCl_Private* GetNaClInterface() {
  return g_nacl_interface;
}

void SetNaClInterface(const PPB_NaCl_Private* nacl_interface) {
  g_nacl_interface = nacl_interface;
}

}  // namespace plugin
