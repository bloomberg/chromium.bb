// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/serial/serial_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

using namespace extension_function_test_utils;

namespace {

class SerialApiTest : public ExtensionApiTest {
 public:
  SerialApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
    command_line->AppendSwitch(switches::kEnablePlatformApps);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialExtension) {
  ASSERT_TRUE(RunExtensionTest("serial/api")) << message_;
}
