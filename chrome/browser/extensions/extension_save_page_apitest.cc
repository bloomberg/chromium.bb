// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_save_page_api.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

class ExtensionSavePageApiTest : public ExtensionApiTest {
 public:
  // TODO(jcivelli): remove this once save-page APIs are no longer experimental.
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");
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

class SavePageAsMHTMLDelegate : public SavePageAsMHTMLFunction::TestDelegate {
 public:
  SavePageAsMHTMLDelegate() {
    SavePageAsMHTMLFunction::SetTestDelegate(this);
  }

  virtual ~SavePageAsMHTMLDelegate() {
    SavePageAsMHTMLFunction::SetTestDelegate(NULL);
  }

  virtual void OnTemporaryFileCreated(const FilePath& temp_file) OVERRIDE {
    temp_file_ = temp_file;
  }

  FilePath temp_file_;
};

IN_PROC_BROWSER_TEST_F(ExtensionSavePageApiTest, MAYBE_SavePageAsMHTML) {
  SavePageAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunExtensionTest("save_page")) << message_;
  ASSERT_FALSE(delegate.temp_file_.empty());
  // Flush the file message loop to make sure the delete happens.
  ui_test_utils::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
  ASSERT_FALSE(file_util::PathExists(delegate.temp_file_));

}
