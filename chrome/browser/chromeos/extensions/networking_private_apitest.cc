// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"

namespace chromeos {

class ExtensionNetworkingPrivateApiTest : public ExtensionApiTest {
 public:
  // Whitelist the extension ID of the test extension.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kWhitelistedExtensionID, "epcifkihnkjgphfkloaaleeakhpmgdmn");
 }
};

IN_PROC_BROWSER_TEST_F(ExtensionNetworkingPrivateApiTest, BasicFunctions) {
  ASSERT_TRUE(RunComponentExtensionTest("networking")) << message_;
}

}  // namespace chromeos
