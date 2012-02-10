// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

class ExtensionChromePrivateApiTest : public ExtensionApiTest {
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kWhitelistedExtensionID, "oflbaaikkabfdfkimeclgkackhdkpnip");
  }
};

// Needs a new baseline.
// http://code.google.com/p/chromium/issues/detail?id=113657
IN_PROC_BROWSER_TEST_F(ExtensionChromePrivateApiTest, DISABLED_DecodeJPEG) {
  ASSERT_TRUE(RunExtensionTest("decode_jpeg")) << message_;
}
