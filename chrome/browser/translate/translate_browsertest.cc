// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/infobar.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_script.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace {

const base::FilePath::CharType kTranslateRoot[] =
    FILE_PATH_LITERAL("chrome/test/data/translate");
const char kNonSecurePrefix[] = "/translate/";
const char kSecurePrefix[] = "files/";
const char kFrenchTestPath[] = "fr_test.html";
const char kRefreshMetaTagTestPath[] = "refresh_meta_tag.html";
const char kRefreshMetaTagCaseInsensitiveTestPath[] =
    "refresh_meta_tag_casei.html";
const char kRefreshMetaTagAtOnloadTestPath[] =
    "refresh_meta_tag_at_onload.html";
const char kUpdateLocationTestPath[] = "update_location.html";
const char kUpdateLocationAtOnloadTestPath[] = "update_location_at_onload.html";
const char kMainScriptPath[] = "pseudo_main.js";
const char kElementMainScriptPath[] = "pseudo_element_main.js";

};  // namespace

class TranslateBrowserTest : public InProcessBrowserTest {
 public:
  TranslateBrowserTest()
      : https_server_(net::SpawnedTestServer::TYPE_HTTPS,
                      SSLOptions(SSLOptions::CERT_OK),
                      base::FilePath(kTranslateRoot)),
        infobar_service_(NULL) {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ASSERT_TRUE(https_server_.Start());
    // Setup alternate security origin for testing in order to allow XHR against
    // local test server. Note that this flag shows a confirm infobar in tests.
    GURL base_url = GetSecureURL("");
    command_line->AppendSwitchASCII(
        translate::switches::kTranslateSecurityOrigin,
        base_url.GetOrigin().spec());
  }

 protected:
  GURL GetNonSecureURL(const std::string& path) const {
    std::string prefix(kNonSecurePrefix);
    return embedded_test_server()->GetURL(prefix + path);
  }

  GURL GetSecureURL(const std::string& path) const {
    std::string prefix(kSecurePrefix);
    return https_server_.GetURL(prefix + path);
  }

  translate::TranslateInfoBarDelegate* GetExistingTranslateInfoBarDelegate() {
    if (!infobar_service_) {
      content::WebContents* web_contents =
          browser()->tab_strip_model()->GetActiveWebContents();
      if (web_contents)
        infobar_service_ = InfoBarService::FromWebContents(web_contents);
    }
    if (!infobar_service_) {
      ADD_FAILURE() << "infobar service is not available";
      return NULL;
    }

    translate::TranslateInfoBarDelegate* delegate = NULL;
    for (size_t i = 0; i < infobar_service_->infobar_count(); ++i) {
      // Check if the shown infobar is a confirm infobar coming from the
      // |kTranslateSecurityOrigin| flag specified in SetUpCommandLine().
      // This infobar appears in all tests of TranslateBrowserTest and can be
      // ignored here.
      if (infobar_service_->infobar_at(i)->delegate()->
              AsConfirmInfoBarDelegate()) {
        continue;
      }

      translate::TranslateInfoBarDelegate* translate =
          infobar_service_->infobar_at(i)
              ->delegate()
              ->AsTranslateInfoBarDelegate();
      if (translate) {
        EXPECT_FALSE(delegate) << "multiple infobars are shown unexpectedly";
        delegate = translate;
        continue;
      }

      // Other infobar should not be shown.
      EXPECT_TRUE(delegate);
    }
    return delegate;
  }

 private:
  net::SpawnedTestServer https_server_;
  InfoBarService* infobar_service_;

  typedef net::SpawnedTestServer::SSLOptions SSLOptions;

  DISALLOW_COPY_AND_ASSIGN(TranslateBrowserTest);
};

IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, TranslateInIsolatedWorld) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  net::TestURLFetcherFactory factory;
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Check if there is no Translate infobar.
  translate::TranslateInfoBarDelegate* translate =
      GetExistingTranslateInfoBarDelegate();
  EXPECT_FALSE(translate);

  // Setup infobar observer.
  content::WindowedNotificationObserver infobar(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::NotificationService::AllSources());

  // Setup page title observer.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::TitleWatcher watcher(web_contents, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit non-secure page which is going to be translated.
  ui_test_utils::NavigateToURL(browser(), GetNonSecureURL(kFrenchTestPath));

  // Wait for Chrome Translate infobar.
  infobar.Wait();

  // Perform Chrome Translate.
  translate = GetExistingTranslateInfoBarDelegate();
  ASSERT_TRUE(translate);
  translate->Translate();

  // Hook URLFetcher for element.js.
  GURL script1_url = GetSecureURL(kMainScriptPath);
  GURL script2_url = GetSecureURL(kElementMainScriptPath);
  std::string element_js = "main_script_url = '" + script1_url.spec() + "';\n";
  element_js += "element_main_script_url = '" + script2_url.spec() + "';\n";
  element_js +=
    "google = { 'translate' : { 'TranslateService' : function() { return {\n"
    "  isAvailable: function() {\n"
    "    cr.googleTranslate.onLoadJavascript(main_script_url);\n"
    "    return true;\n"
    "  },\n"
    "  translatePage: function(sl, tl, cb) {\n"
    "    cb(1, true);\n"
    "  }\n"
    "} } } };\n"
    "cr.googleTranslate.onTranslateElementLoad();\n";
  net::TestURLFetcher* fetcher =
      factory.GetFetcherByID(translate::TranslateScript::kFetcherId);
  ASSERT_TRUE(fetcher);
  net::URLRequestStatus status;
  status.set_status(net::URLRequestStatus::SUCCESS);
  fetcher->set_status(status);
  fetcher->set_url(fetcher->GetOriginalURL());
  fetcher->set_response_code(net::HTTP_OK);
  fetcher->SetResponseString(element_js);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  // Wait for the page title is changed after the test finished.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", base::UTF16ToASCII(result));
}

IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, IgnoreRefreshMetaTag) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Check if there is no Translate infobar.
  translate::TranslateInfoBarDelegate* translate =
      GetExistingTranslateInfoBarDelegate();
  EXPECT_FALSE(translate);

  // Setup page title observer.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::TitleWatcher watcher(web_contents, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit a test page.
  ui_test_utils::NavigateToURL(
      browser(),
      GetNonSecureURL(kRefreshMetaTagTestPath));

  // Wait for the page title is changed after the test finished.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", base::UTF16ToASCII(result));

  // Check if there is no Translate infobar.
  translate = GetExistingTranslateInfoBarDelegate();
  EXPECT_FALSE(translate);
}

IN_PROC_BROWSER_TEST_F(TranslateBrowserTest,
                       IgnoreRefreshMetaTagInCaseInsensitive) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Check if there is no Translate infobar.
  translate::TranslateInfoBarDelegate* translate =
      GetExistingTranslateInfoBarDelegate();
  EXPECT_FALSE(translate);

  // Setup page title observer.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::TitleWatcher watcher(web_contents, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit a test page.
  ui_test_utils::NavigateToURL(
      browser(),
      GetNonSecureURL(kRefreshMetaTagCaseInsensitiveTestPath));

  // Wait for the page title is changed after the test finished.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", base::UTF16ToASCII(result));

  // Check if there is no Translate infobar.
  translate = GetExistingTranslateInfoBarDelegate();
  EXPECT_FALSE(translate);
}

IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, IgnoreRefreshMetaTagAtOnload) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Check if there is no Translate infobar.
  translate::TranslateInfoBarDelegate* translate =
      GetExistingTranslateInfoBarDelegate();
  EXPECT_FALSE(translate);

  // Setup page title observer.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::TitleWatcher watcher(web_contents, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit a test page.
  ui_test_utils::NavigateToURL(
      browser(),
      GetNonSecureURL(kRefreshMetaTagAtOnloadTestPath));

  // Wait for the page title is changed after the test finished.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", base::UTF16ToASCII(result));

  // Check if there is no Translate infobar.
  translate = GetExistingTranslateInfoBarDelegate();
  EXPECT_FALSE(translate);
}

IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, UpdateLocation) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Check if there is no Translate infobar.
  translate::TranslateInfoBarDelegate* translate =
      GetExistingTranslateInfoBarDelegate();
  EXPECT_FALSE(translate);

  // Setup page title observer.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::TitleWatcher watcher(web_contents, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit a test page.
  ui_test_utils::NavigateToURL(
      browser(),
      GetNonSecureURL(kUpdateLocationTestPath));

  // Wait for the page title is changed after the test finished.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", base::UTF16ToASCII(result));

  // Check if there is no Translate infobar.
  translate = GetExistingTranslateInfoBarDelegate();
  EXPECT_FALSE(translate);
}

IN_PROC_BROWSER_TEST_F(TranslateBrowserTest, UpdateLocationAtOnload) {
  // TODO(port): Test corresponding bubble translate UX: http://crbug.com/383235
  if (TranslateService::IsTranslateBubbleEnabled())
    return;

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // Check if there is no Translate infobar.
  translate::TranslateInfoBarDelegate* translate =
      GetExistingTranslateInfoBarDelegate();
  EXPECT_FALSE(translate);

  // Setup page title observer.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  content::TitleWatcher watcher(web_contents, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit a test page.
  ui_test_utils::NavigateToURL(
      browser(),
      GetNonSecureURL(kUpdateLocationAtOnloadTestPath));

  // Wait for the page title is changed after the test finished.
  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_EQ("PASS", base::UTF16ToASCII(result));

  // Check if there is no Translate infobar.
  translate = GetExistingTranslateInfoBarDelegate();
  EXPECT_FALSE(translate);
}
