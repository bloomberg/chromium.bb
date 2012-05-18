// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"

class ExtensionResourceRequestPolicyTest : public ExtensionApiTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowLegacyExtensionManifests);
  }
};

// Note, this mostly tests the logic of chrome/renderer/extensions/
// extension_resource_request_policy.*, but we have it as a browser test so that
// can make sure it works end-to-end.
IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, OriginPrivileges) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("extension")));

  GURL web_resource(
      test_server()->GetURL(
          "files/extensions/api_test/extension_resource_request_policy/"
          "index.html"));

  std::string host_a("a.com");
  GURL::Replacements make_host_a_com;
  make_host_a_com.SetHostStr(host_a);

  std::string host_b("b.com");
  GURL::Replacements make_host_b_com;
  make_host_b_com.SetHostStr(host_b);

  // A web host that has permission.
  ui_test_utils::NavigateToURL(
      browser(), web_resource.ReplaceComponents(make_host_a_com));
  std::string result;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
    browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(document.title)",
    &result));
  EXPECT_EQ(result, "Loaded");

  // A web host that loads a non-existent extension.
  GURL non_existent_extension(
      test_server()->GetURL(
          "files/extensions/api_test/extension_resource_request_policy/"
          "non_existent_extension.html"));
  ui_test_utils::NavigateToURL(browser(), non_existent_extension);
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
    browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(document.title)",
    &result));
  EXPECT_EQ(result, "Image failed to load");

  // A data URL. Data URLs should always be able to load chrome-extension://
  // resources.
  std::string file_source;
  ASSERT_TRUE(file_util::ReadFileToString(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
                    .AppendASCII("index.html"), &file_source));
  ui_test_utils::NavigateToURL(browser(),
      GURL(std::string("data:text/html;charset=utf-8,") + file_source));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Loaded");

  // A different extension. Legacy (manifest_version 1) extensions should always
  // be able to load each other's resources.
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("extension2")));
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://pbkkcbgdkliohhfaeefcijaghglkahja/index.html"));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Loaded");
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       ExtensionCanLoadHostedAppIcons) {
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("extension")));

  ASSERT_TRUE(RunExtensionSubtest(
      "extension_resource_request_policy/extension2/",
      "can_load_icons_from_hosted_apps.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, Audio) {
  EXPECT_TRUE(RunExtensionSubtest(
      "extension_resource_request_policy/extension2",
      "audio.html"));
}

#if defined(OS_MACOSX)
// http://crbug.com/95274 - Video is flaky on Mac.
#define MAYBE_Video DISABLED_Video
#else
#define MAYBE_Video Video
#endif

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, MAYBE_Video) {
  EXPECT_TRUE(RunExtensionSubtest(
      "extension_resource_request_policy/extension2",
      "video.html"));
}

// This test times out regularly on win_rel trybots. See http://crbug.com/122154
#if defined(OS_WIN)
#define MAYBE_WebAccessibleResources DISABLED_WebAccessibleResources
#else
#define MAYBE_WebAccessibleResources WebAccessibleResources
#endif
IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       MAYBE_WebAccessibleResources) {
  std::string result;
  ASSERT_TRUE(test_server()->Start());
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("web_accessible")));

  GURL accessible_resource(
      test_server()->GetURL(
          "files/extensions/api_test/extension_resource_request_policy/"
          "web_accessible/accessible_resource.html"));
  ui_test_utils::NavigateToURL(browser(), accessible_resource);
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
    browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(document.title)",
    &result));
  EXPECT_EQ("Loaded", result);

  GURL xhr_accessible_resource(
      test_server()->GetURL(
          "files/extensions/api_test/extension_resource_request_policy/"
          "web_accessible/xhr_accessible_resource.html"));
  ui_test_utils::NavigateToURL(
      browser(), xhr_accessible_resource);
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
    browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(document.title)",
    &result));
  EXPECT_EQ("XHR completed with status: 200", result);

  GURL xhr_inaccessible_resource(
      test_server()->GetURL(
          "files/extensions/api_test/extension_resource_request_policy/"
          "web_accessible/xhr_inaccessible_resource.html"));
  ui_test_utils::NavigateToURL(
      browser(), xhr_inaccessible_resource);
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
    browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(document.title)",
    &result));
  EXPECT_EQ("XHR failed to load resource", result);

  GURL nonaccessible_resource(
      test_server()->GetURL(
          "files/extensions/api_test/extension_resource_request_policy/"
          "web_accessible/nonaccessible_resource.html"));
  ui_test_utils::NavigateToURL(browser(), nonaccessible_resource);
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
    browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(document.title)",
    &result));
  EXPECT_EQ("Image failed to load", result);

  GURL nonexistent_resource(
      test_server()->GetURL(
          "files/extensions/api_test/extension_resource_request_policy/"
          "web_accessible/nonexistent_resource.html"));
  ui_test_utils::NavigateToURL(browser(), nonexistent_resource);
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
    browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(document.title)",
    &result));
  EXPECT_EQ("Image failed to load", result);

  GURL nonaccessible_cer_resource(
      test_server()->GetURL(
          "files/extensions/api_test/extension_resource_request_policy/"
          "web_accessible/nonaccessible_chrome_resource_scheme.html"));
  ui_test_utils::NavigateToURL(browser(), nonaccessible_cer_resource);
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
    browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(document.title)",
    &result));
  EXPECT_EQ("Loading CER:// failed.", result);
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, Iframe) {
  // Load another extension, which the test one shouldn't be able to get
  // resources from.
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("inaccessible")));
  EXPECT_TRUE(RunExtensionSubtest(
      "extension_resource_request_policy/web_accessible",
      "iframe.html"));
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       ExtensionAccessibleResources) {
  ASSERT_TRUE(RunExtensionSubtest("accessible_cer", "main.html")) << message_;
}
