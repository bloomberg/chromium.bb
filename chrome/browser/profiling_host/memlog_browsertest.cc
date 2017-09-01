// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/features.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

// Some builds don't support the allocator shim in which case the memory long
// won't function.
#if BUILDFLAG(USE_ALLOCATOR_SHIM)

namespace {

class MemlogBrowserTest : public InProcessBrowserTest,
                          public testing::WithParamInterface<const char*> {
 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    if (GetParam())
      command_line->AppendSwitchASCII(switches::kMemlog, GetParam());
  }
};

IN_PROC_BROWSER_TEST_P(MemlogBrowserTest, EndToEnd) {
  if (!GetParam()) {
    // Test that nothing has been started if the flag is not passed.
    ASSERT_FALSE(profiling::ProfilingProcessHost::has_started());
  } else {
    ASSERT_TRUE(profiling::ProfilingProcessHost::has_started());
  }
}

INSTANTIATE_TEST_CASE_P(NoMemlog,
                        MemlogBrowserTest,
                        ::testing::Values(static_cast<const char*>(nullptr)));
INSTANTIATE_TEST_CASE_P(BrowserOnly,
                        MemlogBrowserTest,
                        ::testing::Values(switches::kMemlogModeBrowser));
INSTANTIATE_TEST_CASE_P(AllProcesses,
                        MemlogBrowserTest,
                        ::testing::Values(switches::kMemlogModeAll));

}  // namespace

#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
