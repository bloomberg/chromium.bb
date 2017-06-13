// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/notification_service.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class TranslateManagerBrowserTest : public InProcessBrowserTest {
 public:
  TranslateManagerBrowserTest() {}
  ~TranslateManagerBrowserTest() override {}

  void WaitUntilLanguageDetected() { language_detected_signal_->Wait(); }
  void WaitUntilPageTranslated() { page_translated_signal_->Wait(); }

  void ResetObserver() {
    language_detected_signal_.reset(new LangageDetectionObserver(
        chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
        content::NotificationService::AllSources()));
    page_translated_signal_.reset(new content::WindowedNotificationObserver(
        chrome::NOTIFICATION_PAGE_TRANSLATED,
        content::NotificationService::AllSources()));
  }

  void SimulateURLFetch(bool success) {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    net::Error error = success ? net::OK : net::ERR_FAILED;

    std::string script =
        " var google = {};"
        "google.translate = (function() {"
        "  return {"
        "    TranslateService: function() {"
        "      return {"
        "        isAvailable : function() {"
        "          return true;"
        "        },"
        "        restore : function() {"
        "          return;"
        "        },"
        "        getDetectedLanguage : function() {"
        "          return \"fr\";"
        "        },"
        "        translatePage : function(originalLang, targetLang,"
        "                                 onTranslateProgress) {"
        "          var error = (originalLang == 'auto') ? true : false;"
        "          onTranslateProgress(100, true, error);"
        "        }"
        "      };"
        "    }"
        "  };"
        "})();"
        "cr.googleTranslate.onTranslateElementLoad();";

    fetcher->set_url(fetcher->GetOriginalURL());
    fetcher->set_status(net::URLRequestStatus::FromError(error));
    fetcher->set_response_code(success ? 200 : 500);
    fetcher->SetResponseString(script);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

 protected:
  // InProcessBrowserTest members.
  void SetUp() override {
    InProcessBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    ResetObserver();
    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  net::TestURLFetcherFactory url_fetcher_factory_;

  using LangageDetectionObserver =
      ui_test_utils::WindowedNotificationObserverWithDetails<
          translate::LanguageDetectionDetails>;

  std::unique_ptr<LangageDetectionObserver> language_detected_signal_;
  std::unique_ptr<content::WindowedNotificationObserver>
      page_translated_signal_;
};

// Tests that the CLD (Compact Language Detection) works properly.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, PageLanguageDetection) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* current_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(current_web_contents);
  // The InProcessBrowserTest opens a new tab, let's wait for that first.
  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetected();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in English.
  ResetObserver();
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/english_page.html")),
                ui::PAGE_TRANSITION_TYPED);
  current_web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  chrome_translate_client =
      ChromeTranslateClient::FromWebContents(current_web_contents);
  WaitUntilLanguageDetected();

  EXPECT_EQ("en",
            chrome_translate_client->GetLanguageState().original_language());

  ResetObserver();
  // Now navigate to a page in French.
  ui_test_utils::NavigateToURL(
      browser(), GURL(embedded_test_server()->GetURL("/french_page.html")));
  WaitUntilLanguageDetected();

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());
}

// Test that the translation was successful.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, PageTranslationSuccess) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* current_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(current_web_contents);

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetected();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with a page in French.
  ResetObserver();
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/french_page.html")),
                ui::PAGE_TRANSITION_TYPED);
  current_web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  chrome_translate_client =
      ChromeTranslateClient::FromWebContents(current_web_contents);
  WaitUntilLanguageDetected();

  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  translate::TranslateManager* manager =
      chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().original_language(), "en",
      true);

  SimulateURLFetch(true);

  // Wait for NOTIFICATION_PAGE_TRANSLATED notification.
  WaitUntilPageTranslated();

  EXPECT_FALSE(chrome_translate_client->GetLanguageState().translation_error());
}

// Test if there was an error during translation.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, PageTranslationError) {
  ASSERT_TRUE(embedded_test_server()->Start());

  content::WebContents* current_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(current_web_contents);

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetected();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Open a new tab with about:blank page.
  ResetObserver();
  AddTabAtIndex(0, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED);
  current_web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  chrome_translate_client =
      ChromeTranslateClient::FromWebContents(current_web_contents);
  WaitUntilLanguageDetected();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  // Translate the page through TranslateManager.
  translate::TranslateManager* manager =
      chrome_translate_client->GetTranslateManager();
  manager->TranslatePage(
      chrome_translate_client->GetLanguageState().original_language(), "en",
      true);

  SimulateURLFetch(true);

  // Wait for NOTIFICATION_PAGE_TRANSLATED notification.
  WaitUntilPageTranslated();

  EXPECT_TRUE(chrome_translate_client->GetLanguageState().translation_error());
}

// Test that session restore restores the translate infobar and other translate
// settings.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       PRE_TranslateSessionRestore) {
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  content::WebContents* current_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(current_web_contents);

  // There is a possible race condition, when the language is not yet detected,
  // so we check for that and wait if necessary.
  if (chrome_translate_client->GetLanguageState().original_language().empty())
    WaitUntilLanguageDetected();

  EXPECT_EQ("und",
            chrome_translate_client->GetLanguageState().original_language());

  ResetObserver();

  GURL french_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("french_page.html")));
  ui_test_utils::NavigateToURL(browser(), french_url);

  WaitUntilLanguageDetected();
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());
}

IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       TranslateSessionRestore) {
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ChromeTranslateClient* active_translate_client =
      ChromeTranslateClient::FromWebContents(active_web_contents);
  if (active_translate_client->GetLanguageState().current_language().empty())
    WaitUntilLanguageDetected();
  EXPECT_EQ("und",
            active_translate_client->GetLanguageState().current_language());

  // Make restored tab active to (on some platforms) initiate language
  // detection.
  browser()->tab_strip_model()->ActivateTabAt(0, true);

  content::WebContents* restored_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ChromeTranslateClient* restored_translate_client =
      ChromeTranslateClient::FromWebContents(restored_web_contents);
  if (restored_translate_client->GetLanguageState()
          .current_language()
          .empty()) {
    ResetObserver();
    WaitUntilLanguageDetected();
  }
  EXPECT_EQ("fr",
            restored_translate_client->GetLanguageState().current_language());
}
