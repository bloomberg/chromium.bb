// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class ExtensionResourceRequestPolicyTest : public ExtensionApiTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        extensions::switches::kAllowLegacyExtensionManifests);
  }
};

// Note, this mostly tests the logic of chrome/renderer/extensions/
// extension_resource_request_policy.*, but we have it as a browser test so that
// can make sure it works end-to-end.
IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, OriginPrivileges) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(LoadExtensionWithFlags(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("extension"),
      // Tests manifest_version 1 behavior, so warnings are expected.
      ExtensionBrowserTest::kFlagIgnoreManifestWarnings));

  GURL web_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "index.html"));

  GURL::Replacements make_host_a_com;
  make_host_a_com.SetHostStr("a.com");

  GURL::Replacements make_host_b_com;
  make_host_b_com.SetHostStr("b.com");

  // A web host that has permission.
  ui_test_utils::NavigateToURL(
      browser(), web_resource.ReplaceComponents(make_host_a_com));
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Loaded");

  // A web host that loads a non-existent extension.
  GURL non_existent_extension(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "non_existent_extension.html"));
  ui_test_utils::NavigateToURL(browser(), non_existent_extension);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Image failed to load");

  // A data URL. Data URLs should always be able to load chrome-extension://
  // resources.
  std::string file_source;
  ASSERT_TRUE(base::ReadFileToString(
      test_data_dir_.AppendASCII("extension_resource_request_policy")
                    .AppendASCII("index.html"), &file_source));
  ui_test_utils::NavigateToURL(browser(),
      GURL(std::string("data:text/html;charset=utf-8,") + file_source));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Loaded");

  // A different extension. Legacy (manifest_version 1) extensions should always
  // be able to load each other's resources.
  ASSERT_TRUE(LoadExtensionWithFlags(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("extension2"),
      // Tests manifest_version 1 behavior, so warnings are expected.
      ExtensionBrowserTest::kFlagIgnoreManifestWarnings));
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://pbkkcbgdkliohhfaeefcijaghglkahja/index.html"));
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Loaded");
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       ExtensionCanLoadHostedAppIcons) {
  ASSERT_TRUE(LoadExtensionWithFlags(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("extension"),
      // Tests manifest_version 1 behavior, so warnings are expected.
      ExtensionBrowserTest::kFlagIgnoreManifestWarnings));

  ASSERT_TRUE(RunExtensionSubtest(
      "extension_resource_request_policy/extension2/",
      "can_load_icons_from_hosted_apps.html",
      // Tests manifest_version 1 behavior, so warnings are expected.
      ExtensionApiTest::kFlagIgnoreManifestWarnings)) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, Audio) {
  EXPECT_TRUE(RunExtensionSubtest(
      "extension_resource_request_policy/extension2",
      "audio.html",
      // Tests manifest_version 1 behavior, so warnings are expected.
      ExtensionApiTest::kFlagIgnoreManifestWarnings)) << message_;
}

#if defined(OS_MACOSX) || defined(OS_WIN)
// http://crbug.com/238733 - Video is flaky on Mac and Win.
#define MAYBE_Video DISABLED_Video
#else
#define MAYBE_Video Video
#endif

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, MAYBE_Video) {
  EXPECT_TRUE(RunExtensionSubtest(
      "extension_resource_request_policy/extension2",
      "video.html",
      // Tests manifest_version 1 behavior, so warnings are expected.
      ExtensionApiTest::kFlagIgnoreManifestWarnings)) << message_;
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
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("web_accessible")));

  GURL accessible_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/accessible_resource.html"));
  ui_test_utils::NavigateToURL(browser(), accessible_resource);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ("Loaded", result);

  GURL xhr_accessible_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/xhr_accessible_resource.html"));
  ui_test_utils::NavigateToURL(
      browser(), xhr_accessible_resource);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ("XHR completed with status: 200", result);

  GURL xhr_inaccessible_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/xhr_inaccessible_resource.html"));
  ui_test_utils::NavigateToURL(
      browser(), xhr_inaccessible_resource);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ("XHR failed to load resource", result);

  GURL nonaccessible_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/nonaccessible_resource.html"));
  ui_test_utils::NavigateToURL(browser(), nonaccessible_resource);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ("Image failed to load", result);

  GURL nonexistent_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/nonexistent_resource.html"));
  ui_test_utils::NavigateToURL(browser(), nonexistent_resource);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ("Image failed to load", result);

  GURL nonaccessible_cer_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/nonaccessible_chrome_resource_scheme.html"));
  ui_test_utils::NavigateToURL(browser(), nonaccessible_cer_resource);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ("Loading CER:// failed.", result);

  GURL newtab_page("chrome://newtab");
  GURL accessible_newtab_override(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/accessible_history_navigation.html"));
  ui_test_utils::NavigateToURL(browser(), newtab_page);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), accessible_newtab_override, 2);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ("New Tab Page Loaded Successfully", result);
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       LinkToWebAccessibleResources) {
  std::string result;
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("web_accessible")));

  GURL accessible_linked_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/accessible_link_resource.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
      accessible_linked_resource, 2);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.URL)",
      &result));
  EXPECT_NE("about:blank", result);

  GURL nonaccessible_linked_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/nonaccessible_link_resource.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
      nonaccessible_linked_resource, 2);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.URL)",
      &result));
  EXPECT_EQ("about:blank", result);

  GURL accessible_client_redirect_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/accessible_redirect_resource.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
      accessible_client_redirect_resource, 2);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.URL)",
      &result));
  EXPECT_NE("about:blank", result);

  GURL nonaccessible_client_redirect_resource(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/nonaccessible_redirect_resource.html"));
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(browser(),
      nonaccessible_client_redirect_resource, 2);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.URL)",
      &result));
  EXPECT_EQ("about:blank", result);
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       WebAccessibleResourcesWithCSP) {
  std::string result;
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("web_accessible")));

  GURL accessible_resource_with_csp(embedded_test_server()->GetURL(
      "/extensions/api_test/extension_resource_request_policy/"
      "web_accessible/accessible_resource_with_csp.html"));
  ui_test_utils::NavigateToURL(browser(), accessible_resource_with_csp);
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      "window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ("Loaded", result);
}

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, Iframe) {
  // Load another extension, which the test one shouldn't be able to get
  // resources from.
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("inaccessible")));
  EXPECT_TRUE(RunExtensionSubtest(
      "extension_resource_request_policy/web_accessible",
      "iframe.html")) << message_;
}

#if defined(OS_MACOSX)
#define MAYBE_ExtensionAccessibleResources DISABLED_ExtensionAccessibleResources
#else
#define MAYBE_ExtensionAccessibleResources ExtensionAccessibleResources
#endif
IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest,
                       MAYBE_ExtensionAccessibleResources) {
  ASSERT_TRUE(RunExtensionSubtest("accessible_cer", "main.html")) << message_;
}
