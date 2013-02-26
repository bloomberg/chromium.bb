// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

class ExtensionApiTestWithSwitch : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kSilentDebuggerExtensionAPI);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithSwitch, ExtensionDebugger) {
  ASSERT_TRUE(RunExtensionTest("debugger_extension")) << message_;
}
