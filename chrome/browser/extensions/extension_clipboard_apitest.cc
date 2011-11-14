// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

class ExtensionClipboardApiTest : public ExtensionApiTest {
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalExtensionApis);

    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionClipboardApiTest, WriteAndReadTest) {
#if defined(OS_WIN) || defined(USE_AURA)
  ASSERT_TRUE(RunExtensionTest("clipboard_api/extension_win")) << message_;
#elif defined(OS_MACOSX)
  ASSERT_TRUE(RunExtensionTest("clipboard_api/extension_mac")) << message_;
#else  // !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(USE_AURA)
  ASSERT_TRUE(RunExtensionTest("clipboard_api/extension"))  << message_;
#endif
};

IN_PROC_BROWSER_TEST_F(ExtensionClipboardApiTest, NoPermission) {
  ASSERT_FALSE(RunExtensionTest("clipboard_api/extension_no_permission"))
      << message_;
};
