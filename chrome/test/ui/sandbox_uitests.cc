// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui/ui_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class SandboxTest : public UITest {
 protected:
  // Launches chrome with the --test-sandbox=security_tests.dll flag.
  SandboxTest() : UITest() {
    launch_arguments_.AppendSwitchWithValue(switches::kTestSandbox,
                                            "security_tests.dll");
  }
};

#if !defined(OS_WIN)
// Need a cross-platform test library: http://crbug.com/45771
#define ExecuteDll DISABLED_ExecuteDll
#endif
// Verifies that chrome is running properly.
TEST_F(SandboxTest, ExecuteDll) {
  EXPECT_EQ(1, GetTabCount());
}
