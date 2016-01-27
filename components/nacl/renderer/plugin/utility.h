/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A collection of debugging related interfaces.

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_UTILITY_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_UTILITY_H_

#include <stdint.h>

#include "components/nacl/renderer/ppb_nacl_private.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/nacl_time.h"

#define SRPC_PLUGIN_DEBUG 1

namespace plugin {

const PPB_NaCl_Private* GetNaClInterface();
void SetNaClInterface(const PPB_NaCl_Private* nacl_interface);

// Debugging print utility
extern int gNaClPluginDebugPrintEnabled;
extern int NaClPluginPrintLog(const char *format, ...);
extern int NaClPluginDebugPrintCheckEnv();
#if SRPC_PLUGIN_DEBUG
#define INIT_PLUGIN_LOGGING() do {                                    \
    if (-1 == ::plugin::gNaClPluginDebugPrintEnabled) {               \
      ::plugin::gNaClPluginDebugPrintEnabled =                        \
          ::plugin::NaClPluginDebugPrintCheckEnv();                   \
    }                                                                 \
} while (0)

#define PLUGIN_PRINTF(args) do {                                      \
    INIT_PLUGIN_LOGGING();                                            \
    if (0 != ::plugin::gNaClPluginDebugPrintEnabled) {                \
      ::plugin::NaClPluginPrintLog("PLUGIN %" NACL_PRIu64 ": ",       \
                                   NaClGetTimeOfDayMicroseconds());   \
      ::plugin::NaClPluginPrintLog args;                              \
    }                                                                 \
  } while (0)

// MODULE_PRINTF is used in the module because PLUGIN_PRINTF uses a
// a timer that may not yet be initialized.
#define MODULE_PRINTF(args) do {                                      \
    INIT_PLUGIN_LOGGING();                                            \
    if (0 != ::plugin::gNaClPluginDebugPrintEnabled) {                \
      ::plugin::NaClPluginPrintLog("MODULE: ");                       \
      ::plugin::NaClPluginPrintLog args;                              \
    }                                                                 \
  } while (0)
#else
#  define PLUGIN_PRINTF(args) do { if (0) { printf args; } } while (0)
#  define MODULE_PRINTF(args) do { if (0) { printf args; } } while (0)
/* allows DCE but compiler can still do format string checks */
#endif

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_UTILITY_H_
