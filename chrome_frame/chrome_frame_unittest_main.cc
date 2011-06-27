// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/process.h"
#include "chrome_frame/crash_server_init.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "gtest/gtest.h"

class ObligatoryModule: public CAtlExeModuleT<ObligatoryModule> {
};

ObligatoryModule g_obligatory_atl_module;

static base::AtExitManager* g_at_exit_manager = NULL;

void DeleteAllSingletons() {
  if (g_at_exit_manager) {
    g_at_exit_manager->ProcessCallbacksNow();
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  base::AtExitManager at_exit_manager;
  g_at_exit_manager = &at_exit_manager;

  base::ProcessHandle crash_service = chrome_frame_test::StartCrashService();

  google_breakpad::scoped_ptr<google_breakpad::ExceptionHandler> breakpad(
      InitializeCrashReporting(HEADLESS));

  CommandLine::Init(argc, argv);

  RUN_ALL_TESTS();

  g_at_exit_manager = NULL;

  if (crash_service)
    base::KillProcess(crash_service, 0, false);
}
