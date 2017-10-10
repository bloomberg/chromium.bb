// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains holistic tests of the bindings infrastructure

#include "base/run_loop.h"
#include "chrome/browser/extensions/api/permissions/permissions_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {
namespace {

enum BindingsType { NATIVE_BINDINGS, JAVASCRIPT_BINDINGS };

class ExtensionBindingsApiTest
    : public ExtensionApiTest,
      public ::testing::WithParamInterface<BindingsType> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kNativeCrxBindings,
                                    GetParam() == NATIVE_BINDINGS ? "1" : "0");
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest,
                       UnavailableBindingsNeverRegistered) {
  // Test will request the 'storage' permission.
  PermissionsRequestFunction::SetIgnoreUserGestureForTests(true);
  ASSERT_TRUE(RunExtensionTest(
      "bindings/unavailable_bindings_never_registered")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest,
                       ExceptionInHandlerShouldNotCrash) {
  ASSERT_TRUE(RunExtensionSubtest(
      "bindings/exception_in_handler_should_not_crash",
      "page.html")) << message_;
}

// Tests that an error raised during an async function still fires
// the callback, but sets chrome.runtime.lastError.
IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, LastError) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bindings").AppendASCII("last_error")));

  // Get the ExtensionHost that is hosting our background page.
  extensions::ProcessManager* manager =
      extensions::ProcessManager::Get(browser()->profile());
  extensions::ExtensionHost* host = FindHostWithPath(manager, "/bg.html", 1);

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      host->render_view_host(), "testLastError()", &result));
  EXPECT_TRUE(result);
}

// Regression test that we don't delete our own bindings with about:blank
// iframes.
IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, AboutBlankIframe) {
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

IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest,
                       InternalAPIsNotOnChromeObject) {
  ASSERT_TRUE(RunExtensionSubtest(
      "bindings/internal_apis_not_on_chrome_object",
      "page.html")) << message_;
}

// Tests that we don't override events when bindings are re-injected.
// Regression test for http://crbug.com/269149.
// Regression test for http://crbug.com/436593.
// Flaky on Mac. http://crbug.com/733064.
#if defined(OS_MACOSX)
#define MAYBE_EventOverriding DISABLED_EventOverriding
#else
#define MAYBE_EventOverriding EventOverriding
#endif
IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, MAYBE_EventOverriding) {
  ASSERT_TRUE(RunExtensionTest("bindings/event_overriding")) << message_;
  // The extension test removes a window and, during window removal, sends the
  // success message. Make sure we flush all pending tasks.
  base::RunLoop().RunUntilIdle();
}

// Tests the effectiveness of the 'nocompile' feature file property.
// Regression test for http://crbug.com/356133.
IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, Nocompile) {
  ASSERT_TRUE(RunExtensionSubtest("bindings/nocompile", "page.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, ApiEnums) {
  ASSERT_TRUE(RunExtensionTest("bindings/api_enums")) << message_;
};

// Regression test for http://crbug.com/504011 - proper access checks on
// getModuleSystem().
IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, ModuleSystem) {
  ASSERT_TRUE(RunExtensionTest("bindings/module_system")) << message_;
}

IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, NoExportOverriding) {
  // We need to create runtime bindings in the web page. An extension that's
  // externally connectable will do that for us.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bindings")
                    .AppendASCII("externally_connectable_everywhere")));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/override_exports.html"));

  // See chrome/test/data/extensions/api_test/bindings/override_exports.html.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
          "document.getElementById('status').textContent.trim());",
      &result));
  EXPECT_EQ("success", result);
}

IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, NoGinDefineOverriding) {
  // We need to create runtime bindings in the web page. An extension that's
  // externally connectable will do that for us.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("bindings")
                    .AppendASCII("externally_connectable_everywhere")));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/override_gin_define.html"));
  ASSERT_FALSE(
      browser()->tab_strip_model()->GetActiveWebContents()->IsCrashed());

  // See chrome/test/data/extensions/api_test/bindings/override_gin_define.html.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send("
          "document.getElementById('status').textContent.trim());",
      &result));
  EXPECT_EQ("success", result);
}

IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, HandlerFunctionTypeChecking) {
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/handler_function_type_checking.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(web_contents->IsCrashed());
  // See handler_function_type_checking.html.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send("
          "document.getElementById('status').textContent.trim());",
      &result));
  EXPECT_EQ("success", result);
}

IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest,
                       MoreNativeFunctionInterceptionTests) {
  // We need to create runtime bindings in the web page. An extension that's
  // externally connectable will do that for us.
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bindings")
                        .AppendASCII("externally_connectable_everywhere")));

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/function_interceptions.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(web_contents->IsCrashed());
  // See function_interceptions.html.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "window.domAutomationController.send(window.testStatus);",
      &result));
  EXPECT_EQ("success", result);
}

class FramesExtensionBindingsApiTest : public ExtensionBindingsApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBindingsApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kDisablePopupBlocking);
  }
};

// This tests that web pages with iframes or child windows pointing at
// chrome-extenison:// urls, both web_accessible and nonexistent pages, don't
// get improper extensions bindings injected while they briefly still point at
// about:blank and are still scriptable by their parent.
//
// The general idea is to load up 2 extensions, one which listens for external
// messages ("receiver") and one which we'll try first faking messages from in
// the web page's iframe, as well as actually send a message from later
// ("sender").
IN_PROC_BROWSER_TEST_P(FramesExtensionBindingsApiTest, FramesBeforeNavigation) {
  // Load the sender and receiver extensions, and make sure they are ready.
  ExtensionTestMessageListener sender_ready("sender_ready", true);
  const Extension* sender = LoadExtension(
      test_data_dir_.AppendASCII("bindings").AppendASCII("message_sender"));
  ASSERT_NE(nullptr, sender);
  ASSERT_TRUE(sender_ready.WaitUntilSatisfied());

  ExtensionTestMessageListener receiver_ready("receiver_ready", false);
  const Extension* receiver =
      LoadExtension(test_data_dir_.AppendASCII("bindings")
                        .AppendASCII("external_message_listener"));
  ASSERT_NE(nullptr, receiver);
  ASSERT_TRUE(receiver_ready.WaitUntilSatisfied());

  // Load the web page which tries to impersonate the sender extension via
  // scripting iframes/child windows before they finish navigating to pages
  // within the sender extension.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/frames_before_navigation.html"));

  bool page_success = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      browser()->tab_strip_model()->GetWebContentsAt(0), "getResult()",
      &page_success));
  EXPECT_TRUE(page_success);

  // Reply to |sender|, causing it to send a message over to |receiver|, and
  // then ask |receiver| for the total message count. It should be 1 since
  // |receiver| should not have received any impersonated messages.
  sender_ready.Reply(receiver->id());
  int message_count = 0;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      ProcessManager::Get(profile())
          ->GetBackgroundHostForExtension(receiver->id())
          ->host_contents(),
      "getMessageCountAfterReceivingRealSenderMessage()", &message_count));
  EXPECT_EQ(1, message_count);
}

IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, TestFreezingChrome) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/extensions/api_test/bindings/freeze.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_FALSE(web_contents->IsCrashed());
}

// Tests interaction with event filter parsing.
IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, TestEventFilterParsing) {
  ExtensionTestMessageListener listener("ready", false);
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bindings/event_filter")));
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  ResultCatcher catcher;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/title1.html"));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// crbug.com/733337
IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, ValidationInterception) {
  // We need to create runtime bindings in the web page. An extension that's
  // externally connectable will do that for us.
  ASSERT_TRUE(
      LoadExtension(test_data_dir_.AppendASCII("bindings")
                        .AppendASCII("externally_connectable_everywhere")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL(
          "/extensions/api_test/bindings/validation_interception.html"));
  content::WaitForLoadStop(web_contents);
  ASSERT_FALSE(web_contents->IsCrashed());
  bool caught = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "domAutomationController.send(caught)", &caught));
  EXPECT_TRUE(caught);
}

IN_PROC_BROWSER_TEST_P(ExtensionBindingsApiTest, UncaughtExceptionLogging) {
  ASSERT_TRUE(RunExtensionTest("bindings/uncaught_exception_logging"))
      << message_;
}

// Run core bindings API tests with both native and JS-based bindings. This
// ensures we have some minimum level of coverage while in the experimental
// phase, when native bindings may be enabled on trunk but not at 100% stable.
INSTANTIATE_TEST_CASE_P(Native,
                        ExtensionBindingsApiTest,
                        ::testing::Values(NATIVE_BINDINGS));
INSTANTIATE_TEST_CASE_P(JavaScript,
                        ExtensionBindingsApiTest,
                        ::testing::Values(JAVASCRIPT_BINDINGS));

INSTANTIATE_TEST_CASE_P(Native,
                        FramesExtensionBindingsApiTest,
                        ::testing::Values(NATIVE_BINDINGS));
INSTANTIATE_TEST_CASE_P(JavaScript,
                        FramesExtensionBindingsApiTest,
                        ::testing::Values(JAVASCRIPT_BINDINGS));

}  // namespace
}  // namespace extensions
