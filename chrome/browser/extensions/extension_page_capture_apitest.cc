// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_page_capture_api.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

class ExtensionPageCaptureApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
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
#define MAYBE_SaveAsMHTML DISABLED_SaveAsMHTML
#else
#define MAYBE_SaveAsMHTML SaveAsMHTML
#endif  // defined(OS_LINUX)

class PageCaptureSaveAsMHTMLDelegate
    : public PageCaptureSaveAsMHTMLFunction::TestDelegate {
 public:
  PageCaptureSaveAsMHTMLDelegate() {
    PageCaptureSaveAsMHTMLFunction::SetTestDelegate(this);
  }

  virtual ~PageCaptureSaveAsMHTMLDelegate() {
    PageCaptureSaveAsMHTMLFunction::SetTestDelegate(NULL);
  }

  virtual void OnTemporaryFileCreated(const FilePath& temp_file) OVERRIDE {
    temp_file_ = temp_file;
  }

  FilePath temp_file_;
};

IN_PROC_BROWSER_TEST_F(ExtensionPageCaptureApiTest, MAYBE_SaveAsMHTML) {
  PageCaptureSaveAsMHTMLDelegate delegate;
  ASSERT_TRUE(RunExtensionTest("page_capture")) << message_;
  ASSERT_FALSE(delegate.temp_file_.empty());
  // Flush the file message loop to make sure the delete happens.
  ui_test_utils::RunAllPendingInMessageLoop(content::BrowserThread::FILE);
  ASSERT_FALSE(file_util::PathExists(delegate.temp_file_));
}
