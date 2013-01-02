// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/mock_host_resolver.h"

namespace {

class ClipboardApiTest : public ExtensionApiTest {
 public:
  bool LoadHostedApp(const std::string& app_name,
                     const std::string& launch_page);
  bool ExecuteCopyInSelectedTab(bool* result);
  bool ExecutePasteInSelectedTab(bool* result);

 private:
  bool ExecuteScriptInSelectedTab(const std::string& script, bool* result);
};

bool ClipboardApiTest::LoadHostedApp(const std::string& app_name,
                                     const std::string& launch_page) {
  host_resolver()->AddRule("*", "127.0.0.1");

  if (!StartTestServer()) {
    message_ = "Failed to start test server.";
    return false;
  }

  if (!LoadExtension(test_data_dir_.AppendASCII("clipboard")
                                   .AppendASCII(app_name))) {
    message_ = "Failed to load hosted app.";
    return false;
  }

  GURL base_url = test_server()->GetURL("files/extensions/api_test/clipboard/");
  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);
  base_url = base_url.ReplaceComponents(replace_host);

  std::string launch_page_path =
      base::StringPrintf("%s/%s", app_name.c_str(), launch_page.c_str());
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve(launch_page_path));

  return true;
}

bool ClipboardApiTest::ExecuteCopyInSelectedTab(bool* result) {
  const char kScript[] =
      "window.domAutomationController.send(document.execCommand('copy'))";
  return ExecuteScriptInSelectedTab(kScript, result);
}

bool ClipboardApiTest::ExecutePasteInSelectedTab(bool* result) {
  const char kScript[] =
      "window.domAutomationController.send(document.execCommand('paste'))";
  return ExecuteScriptInSelectedTab(kScript, result);
}

bool ClipboardApiTest::ExecuteScriptInSelectedTab(const std::string& script,
                                                  bool* result) {
  if (!content::ExecuteJavaScriptAndExtractBool(
          chrome::GetActiveWebContents(browser())->GetRenderViewHost(),
          "",
          script,
          result)) {
    message_ = "Failed to execute script in selected tab.";
    return false;
  }
  return true;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ClipboardApiTest, Extension) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("clipboard/extension")) << message_;
}

IN_PROC_BROWSER_TEST_F(ClipboardApiTest, ExtensionNoPermission) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("clipboard/extension_no_permission"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ClipboardApiTest, HostedApp) {
  ASSERT_TRUE(LoadHostedApp("hosted_app", "main.html")) << message_;

  bool result = false;
  ASSERT_TRUE(ExecuteCopyInSelectedTab(&result)) << message_;
  EXPECT_TRUE(result);
  ASSERT_TRUE(ExecutePasteInSelectedTab(&result)) << message_;
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(ClipboardApiTest, HostedAppNoPermission) {
  ASSERT_TRUE(LoadHostedApp("hosted_app_no_permission", "main.html"))
      << message_;

  bool result = false;
  ASSERT_TRUE(ExecuteCopyInSelectedTab(&result)) << message_;
  EXPECT_FALSE(result);
  ASSERT_TRUE(ExecutePasteInSelectedTab(&result)) << message_;
  EXPECT_FALSE(result);
}

