// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains holistic tests of the bindings infrastructure

#include "chrome/browser/extensions/extension_apitest.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ExceptionInHandlerShouldNotCrash) {
  ASSERT_TRUE(RunExtensionSubtest(
      "bindings/exception_in_handler_should_not_crash",
      "page.html")) << message_;
}

// Tests that an error raised during an async function still fires
// the callback, but sets chrome.runtime.lastError.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, LastError) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browsertest").AppendASCII("last_error")));

  // Get the ExtensionHost that is hosting our background page.
  ExtensionProcessManager* manager =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  extensions::ExtensionHost* host = FindHostWithPath(manager, "/bg.html", 1);

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      host->render_view_host(), "testLastError()", &result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, InternalAPIsNotOnChromeObject) {
  ASSERT_TRUE(RunExtensionSubtest(
      "bindings/internal_apis_not_on_chrome_object",
      "page.html")) << message_;
}
