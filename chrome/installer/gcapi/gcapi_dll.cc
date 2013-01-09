// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"

// Visual Studio needs at least one C++ file in project http://goo.gl/roro9

namespace {
base::AtExitManager* g_exit_manager = NULL;
}

// DLL Entry Point - This is necessary to initialize basic things like the
// CommandLine and Logging components needed by functions in the DLL.
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,
                               DWORD reason,
                               LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_exit_manager = new base::AtExitManager();
    CommandLine::Init(0, NULL);
    logging::InitLogging(
        NULL,
        logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
        logging::LOCK_LOG_FILE,
        logging::DELETE_OLD_LOG_FILE,
        logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  } else if (reason == DLL_PROCESS_DETACH) {
    CommandLine::Reset();
    delete g_exit_manager;
    g_exit_manager = NULL;
  }

  return TRUE;
}
