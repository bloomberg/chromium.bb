// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"

class ExtensionResourceRequestPolicyTest : public ExtensionApiTest {
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
    browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(document.title)",
    &result));
  EXPECT_EQ(result, "Loaded");

  // A web host that does not have permission.
  ui_test_utils::NavigateToURL(
      browser(), web_resource.ReplaceComponents(make_host_b_com));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
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
      browser()->GetSelectedTabContents()->render_view_host(), L"",
      L"window.domAutomationController.send(document.title)",
      &result));
  EXPECT_EQ(result, "Loaded");

  // A different extension. Extensions should always be able to load each
  // other's resources.
  ASSERT_TRUE(LoadExtension(test_data_dir_
      .AppendASCII("extension_resource_request_policy")
      .AppendASCII("extension2")));
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://pbkkcbgdkliohhfaeefcijaghglkahja/index.html"));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
      browser()->GetSelectedTabContents()->render_view_host(), L"",
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

IN_PROC_BROWSER_TEST_F(ExtensionResourceRequestPolicyTest, Video) {
  EXPECT_TRUE(RunExtensionSubtest(
      "extension_resource_request_policy/extension2",
      "video.html"));
}
