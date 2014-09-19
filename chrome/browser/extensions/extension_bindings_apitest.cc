// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains holistic tests of the bindings infrastructure

#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace extensions {
namespace {

class ExtensionBindingsApiTest : public ExtensionApiTest {};

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       UnavailableBindingsNeverRegistered) {
  // Test will request the 'storage' permission.
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(RunExtensionTest(
      "bindings/unavailable_bindings_never_registered")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       ExceptionInHandlerShouldNotCrash) {
  ASSERT_TRUE(RunExtensionSubtest(
      "bindings/exception_in_handler_should_not_crash",
      "page.html")) << message_;
}

// Tests that an error raised during an async function still fires
// the callback, but sets chrome.runtime.lastError.
// FIXME should be in ExtensionBindingsApiTest.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, LastError) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("browsertest").AppendASCII("last_error")));

  // Get the ExtensionHost that is hosting our background page.
  extensions::ProcessManager* manager =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  extensions::ExtensionHost* host = FindHostWithPath(manager, "/bg.html", 1);

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      host->render_view_host(), "testLastError()", &result));
  EXPECT_TRUE(result);
}

// Regression test that we don't delete our own bindings with about:blank
// iframes.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, AboutBlankIframe) {
  ResultCatcher catcher;
  ExtensionTestMessageListener listener("load", true);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("bindings")
                                          .AppendASCII("about_blank_iframe")));

  ASSERT_TRUE(listener.WaitUntilSatisfied());

  const Extension* extension = LoadExtension(
        test_data_dir_.AppendASCII("bindings")
                      .AppendASCII("internal_apis_not_on_chrome_object"));
  ASSERT_TRUE(extension);
  listener.Reply(extension->id());

  ASSERT_TRUE(catcher.GetNextResult()) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest,
                       InternalAPIsNotOnChromeObject) {
  ASSERT_TRUE(RunExtensionSubtest(
      "bindings/internal_apis_not_on_chrome_object",
      "page.html")) << message_;
}

// Tests that we don't override events when bindings are re-injected.
// Regression test for http://crbug.com/269149.
IN_PROC_BROWSER_TEST_F(ExtensionBindingsApiTest, EventOverriding) {
  ASSERT_TRUE(RunExtensionTest("bindings/event_overriding")) << message_;
}

}  // namespace
}  // namespace extensions
