// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

namespace extensions {

class SystemInfoStorageApiTest: public ExtensionApiTest {
 public:
  SystemInfoStorageApiTest() {}
  ~SystemInfoStorageApiTest() {}
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

IN_PROC_BROWSER_TEST_F(SystemInfoStorageApiTest, Storage) {
  ASSERT_TRUE(RunExtensionTest("systeminfo/storage")) << message_;
}

} // namespace extensions
