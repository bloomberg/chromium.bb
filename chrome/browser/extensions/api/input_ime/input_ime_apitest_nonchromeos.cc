// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

namespace extensions {

class InputImeApiTest : public ExtensionApiTest {
 public:
  InputImeApiTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableInputImeAPI);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputImeApiTest);
};

IN_PROC_BROWSER_TEST_F(InputImeApiTest, CreateWindowTest) {
  ASSERT_TRUE(RunExtensionTest("input_ime_nonchromeos")) << message_;
}

}  // namespace extensions
