// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_runner.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

class CrossOriginReadBlockingExtensionTest : public ExtensionBrowserTest {
 public:
  CrossOriginReadBlockingExtensionTest() = default;

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void InstallExtension() {
    const char kManifest[] = R"(
        {
          "name": "CrossOriginReadBlockingTest",
          "version": "1.0",
          "manifest_version": 2,
          "permissions": ["tabs", "*://*/*"],
          "background": {"scripts": ["script.js"]}
        } )";
    dir_.WriteManifest(kManifest);
    dir_.WriteFile(FILE_PATH_LITERAL("script.js"), "");
    extension_ = LoadExtension(dir_.UnpackedPath());
  }

  // Injects (into |web_contents|) a content_script that performs a fetch of
  // |url|. Returns the body of the response.
  //
  // The method below uses "programmatic" (rather than "declarative") way to
  // inject a content script, but the behavior and permissions of the conecnt
  // script should be the same in both cases.  See also
  // https://developer.chrome.com/extensions/content_scripts#programmatic.
  std::string FetchViaContentScript(const GURL& url,
                                    content::WebContents* web_contents) {
    content::DOMMessageQueue message_queue;

    // Inject a content script that performs a cross-origin XHR to bar.com.
    const char kXhrScriptTemplate[] = R"(
      fetch($1)
        .then(response => response.text())
        .then(text => domAutomationController.send(text))
        .catch(err => domAutomationController.send("error: " + err));
    )";
    std::string xhr_script = content::JsReplace(kXhrScriptTemplate, url.spec());
    EXPECT_TRUE(ExecuteContentScript(web_contents, xhr_script));

    // Wait until the message comes back and extract result from the message.
    std::string json;
    EXPECT_TRUE(message_queue.WaitForMessage(&json));
    base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
    std::unique_ptr<base::Value> value = reader.ReadToValue(json);
    std::string result;
    EXPECT_TRUE(value->GetAsString(&result));
    return result;
  }

 private:
  // Asks the test |extension_| to inject |content_script| into |web_contents|.
  // Returns true if the content script injection succeeded.
  bool ExecuteContentScript(content::WebContents* web_contents,
                            const std::string& content_script) {
    int tab_id = ExtensionTabUtil::GetTabId(web_contents);
    std::string background_script = content::JsReplace(
        "chrome.tabs.executeScript($1, { code: $2 });", tab_id, content_script);
    return browsertest_util::ExecuteScriptInBackgroundPageNoWait(
        browser()->profile(), extension_->id(), background_script);
  }

  TestExtensionDir dir_;
  const Extension* extension_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CrossOriginReadBlockingExtensionTest);
};

// Test that verifies the current, baked-in (but not necessarily desirable
// behavior) where an extension that has permission to inject a content script
// to any page can also XHR (without CORS!) any cross-origin resource.
// See also https://crbug.com/846346.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest, SimpleTest) {
  InstallExtension();

  // Navigate to a foo.com page.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL page_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  ui_test_utils::NavigateToURL(browser(), page_url);
  EXPECT_EQ(page_url, web_contents->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(page_url),
            web_contents->GetMainFrame()->GetLastCommittedOrigin());

  // Inject a content script that performs a cross-origin XHR to bar.com.
  // Verify that it returned the right response (one that wasn't blocked by
  // CORB).
  GURL cross_site_resource(
      embedded_test_server()->GetURL("bar.com", "/nosniff.xml"));
  EXPECT_EQ("nosniff.xml - body\n",
            FetchViaContentScript(cross_site_resource, web_contents));
}

}  // namespace extensions
