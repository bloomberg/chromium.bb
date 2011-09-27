// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/mock_host_resolver.h"

class ExtensionSavePageApiTest : public ExtensionApiTest {
 public:
  // TODO(jcivelli): remove this once save-page APIs are no longer experimental.
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    host_resolver()->AddRule("www.a.com", "127.0.0.1");

    ASSERT_TRUE(StartTestServer());
  }
};

// Disabled on Linux http://crbug.com/98194
#if defined(OS_LINUX)
#define MAYBE_SavePageAsMHTML DISABLED_SavePageAsMHTML
#else
#define MAYBE_SavePageAsMHTML SavePageAsMHTML
#endif  // defined(OS_LINUX)

IN_PROC_BROWSER_TEST_F(ExtensionSavePageApiTest, MAYBE_SavePageAsMHTML) {
  ASSERT_TRUE(RunExtensionTest("save_page")) << message_;
}
