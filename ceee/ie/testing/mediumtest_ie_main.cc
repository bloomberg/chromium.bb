// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Main function for large IE tests.
#include <atlbase.h>
#include <atlcom.h>
#include "base/at_exit.h"
#include "base/command_line.h"
#include "ceee/testing/utils/gflag_utils.h"
#include "ceee/testing/utils/nt_internals.h"
#include "chrome/common/url_constants.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"


// Stub for unittesting.
namespace ceee_module_util {

LONG LockModule() {
  return 0;
}
LONG UnlockModule() {
  return 0;
}

void Lock() {}
void Unlock() {}
void RegisterHookForSafetyNet(HHOOK hook) {}
void UnregisterHookForSafetyNet(HHOOK hook) {}

}  // namespace ceee_module_util


// We're testing some ATL code that requires a module object.
class ObligatoryModule: public CAtlDllModuleT<ObligatoryModule> {
};

ObligatoryModule g_obligatory_atl_module;

const DWORD kTestGFlags = FLG_USER_STACK_TRACE_DB |
                          FLG_ENABLE_HANDLE_EXCEPTIONS |
                          FLG_ENABLE_CLOSE_EXCEPTIONS |
                          FLG_HEAP_VALIDATE_PARAMETERS;

int main(int argc, char **argv) {
  // Disabled, bb2560934.
  // if (!IsDebuggerPresent())
  //   testing::RelaunchWithGFlags(kTestGFlags);

  testing::InitGoogleMock(&argc, argv);
  testing::InitGoogleTest(&argc, argv);

  // Obligatory Chrome base initialization.
  CommandLine::Init(argc, argv);
  base::AtExitManager at_exit_manager;

  // Needs to be called before we can use GURL.
  chrome::RegisterChromeSchemes();

  return RUN_ALL_TESTS();
}
