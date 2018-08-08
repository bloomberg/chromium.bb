// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_action_runner.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
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
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
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
    return FetchHelper(
        url, base::BindOnce(
                 &CrossOriginReadBlockingExtensionTest::ExecuteContentScript,
                 base::Unretained(this), base::Unretained(web_contents)));
  }

  // Performs a fetch of |url| from the background page of the test extension.
  // Returns the body of the response.
  std::string FetchViaBackgroundPage(GURL url) {
    return FetchHelper(
        url, base::BindOnce(
                 &browsertest_util::ExecuteScriptInBackgroundPageNoWait,
                 base::Unretained(browser()->profile()), extension_->id()));
  }

  void VerifyContentScriptHistogramIsPresent(
      const base::HistogramTester& histograms,
      content::ResourceType resource_type) {
    VerifyContentScriptHistogram(
        histograms, testing::ElementsAre(base::Bucket(resource_type, 1)));
  }

  void VerifyContentScriptHistogramIsMissing(
      const base::HistogramTester& histograms) {
    VerifyContentScriptHistogram(histograms, testing::IsEmpty());
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

  using FetchCallback =
      base::OnceCallback<bool(const std::string& fetch_script)>;
  std::string FetchHelper(GURL url, FetchCallback fetch_callback) {
    content::DOMMessageQueue message_queue;

    // Inject a content script that performs a cross-origin XHR to bar.com.
    const char kXhrScriptTemplate[] = R"(
      fetch($1)
        .then(response => response.text())
        .then(text => domAutomationController.send(text))
        .catch(err => domAutomationController.send("error: " + err));
    )";
    EXPECT_TRUE(std::move(fetch_callback)
                    .Run(content::JsReplace(kXhrScriptTemplate, url.spec())));

    // Wait until the message comes back and extract result from the message.
    std::string json;
    EXPECT_TRUE(message_queue.WaitForMessage(&json));
    base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
    std::unique_ptr<base::Value> value = reader.ReadToValue(json);
    std::string result;
    EXPECT_TRUE(value->GetAsString(&result));
    return result;
  }

  void VerifyContentScriptHistogram(
      const base::HistogramTester& histograms,
      testing::Matcher<std::vector<base::Bucket>> matcher) {
    // LogInitiatorSchemeBypassingDocumentBlocking is only implemented in the
    // pre-NetworkService CrossSiteDocumentResourceHandler, because we hope to
    // gather enough data before NetworkService ships.  Logging in
    // NetworkService world should be possible but would require an extra IPC
    // from NetworkService to the Browser process which seems like unnecessary
    // complexity, given that the metrics gathered won't be needed in the
    // long-term.
    if (base::FeatureList::IsEnabled(network::features::kNetworkService))
      return;

    // Verify that LogInitiatorSchemeBypassingDocumentBlocking returned early
    // for a request that wasn't from a content script.
    EXPECT_THAT(histograms.GetAllSamples(
                    "SiteIsolation.XSD.Browser.Allowed.ContentScript"),
                matcher);
  }

  TestExtensionDir dir_;
  const Extension* extension_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CrossOriginReadBlockingExtensionTest);
};

// Test that verifies the current, baked-in (but not necessarily desirable
// behavior) where an extension that has permission to inject a content script
// to any page can also XHR (without CORS!) any cross-origin resource.
// See also https://crbug.com/846346.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromContentScript_NoSniffXml) {
  // Load the test extension.
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
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("bar.com", "/nosniff.xml"));
  EXPECT_EQ("nosniff.xml - body\n",
            FetchViaContentScript(cross_site_resource, web_contents));

  // Verify that no blocking occurred.
  EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
              testing::IsEmpty());

  // Verify that LogInitiatorSchemeBypassingDocumentBlocking was called.
  VerifyContentScriptHistogramIsPresent(histograms, content::RESOURCE_TYPE_XHR);
}

// Test that responses that would have been allowed by CORB anyway are not
// reported to LogInitiatorSchemeBypassingDocumentBlocking.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromContentScript_AllowedTextResource) {
  // Load the test extension.
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
  //
  // StartsWith (rather than equality) is used in the verification step to
  // account for \n VS \r\n difference on Windows.
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("bar.com", "/save_page/text.txt"));
  EXPECT_THAT(FetchViaContentScript(cross_site_resource, web_contents),
              ::testing::StartsWith(
                  "text-object.txt: ae52dd09-9746-4b7e-86a6-6ada5e2680c2"));

  // Verify that no blocking occurred.
  EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
              testing::IsEmpty());

  // Verify that we didn't call LogInitiatorSchemeBypassingDocumentBlocking
  // for a response that would have been allowed by CORB anyway.
  VerifyContentScriptHistogramIsMissing(histograms);
}

// Test that responses are blocked by CORB, but have empty response body are not
// reported to LogInitiatorSchemeBypassingDocumentBlocking.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromContentScript_EmptyAndBlocked) {
  // Load the test extension.
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
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("bar.com", "/nosniff.empty"));
  EXPECT_EQ("", FetchViaContentScript(cross_site_resource, web_contents));

  // Verify that no blocking occurred.
  EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
              testing::IsEmpty());

  // Verify that we didn't call LogInitiatorSchemeBypassingDocumentBlocking
  // for a response that would have been blocked by CORB, but was empty.
  VerifyContentScriptHistogramIsMissing(histograms);
}

// Test that LogInitiatorSchemeBypassingDocumentBlocking exits early for
// requests that aren't from content scripts.
IN_PROC_BROWSER_TEST_F(CrossOriginReadBlockingExtensionTest,
                       FromBackgroundPage_NoSniffXml) {
  // Load the test extension.
  InstallExtension();

  // Performs a cross-origin XHR from the background page.
  base::HistogramTester histograms;
  GURL cross_site_resource(
      embedded_test_server()->GetURL("bar.com", "/nosniff.xml"));
  EXPECT_EQ("nosniff.xml - body\n",
            FetchViaBackgroundPage(cross_site_resource));

  // Verify that no blocking occurred.
  EXPECT_THAT(histograms.GetAllSamples("SiteIsolation.XSD.Browser.Blocked"),
              testing::IsEmpty());

  // Verify that LogInitiatorSchemeBypassingDocumentBlocking returned early
  // for a request that wasn't from a content script.
  VerifyContentScriptHistogramIsMissing(histograms);
}

}  // namespace extensions
