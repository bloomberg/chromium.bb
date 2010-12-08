// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Main function for common unittests.
#include <atlbase.h>
#include <atlcom.h>
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "ceee/testing/utils/gflag_utils.h"
#include "chrome/common/url_constants.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "toolband.h"  // NOLINT

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


// We're testing ATL code that requires a module object.
class ObligatoryModule: public CAtlDllModuleT<ObligatoryModule> {
 public:
  DECLARE_LIBID(LIBID_ToolbandLib);
};

ObligatoryModule g_obligatory_atl_module;

// Run these tests under page heap.
const DWORD kGFlags = FLG_USER_STACK_TRACE_DB | FLG_HEAP_PAGE_ALLOCS;

int main(int argc, char **argv) {
  // Disabled, bb2560934.
  // if (!IsDebuggerPresent())
  //   testing::RelaunchWithGFlags(kTestGFlags);

  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);

  // Needs to be called before we can use GURL.
  chrome::RegisterChromeSchemes();

  testing::InitGoogleMock(&argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
