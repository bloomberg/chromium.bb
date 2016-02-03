// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/cld_data_harness.h"
#include "chrome/browser/translate/cld_data_harness_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/notification_service.h"
#include "url/gurl.h"

class TranslateManagerBrowserTest : public InProcessBrowserTest {
 public:
  TranslateManagerBrowserTest() {}
  ~TranslateManagerBrowserTest() override {}

  std::string GetLanguageFor(content::WebContents* web_contents) {
    translate::LanguageDetectionDetails details;
    content::Source<content::WebContents> source(web_contents);
    language_detected_signal_->GetDetailsFor(source.map_key(), &details);
    return details.adopted_language;
  }

  void WaitUntilLanguageDetected() { language_detected_signal_->Wait(); }

  void ResetObserver() {
    language_detected_signal_.reset(new LangageDetectionObserver(
        chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
        content::NotificationService::AllSources()));
  }

 protected:
  // InProcessBrowserTest members.
  void SetUp() override {
    scoped_ptr<test::CldDataHarness> cld_data_harness =
        test::CldDataHarnessFactory::Get()->CreateCldDataHarness();
    ASSERT_NO_FATAL_FAILURE(cld_data_harness->Init());
    InProcessBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    ResetObserver();
    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  using LangageDetectionObserver =
      ui_test_utils::WindowedNotificationObserverWithDetails<
          translate::LanguageDetectionDetails>;

  scoped_ptr<LangageDetectionObserver> language_detected_signal_;
};

// Tests that the CLD (Compact Language Detection) works properly.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest, PageLanguageDetection) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // The InProcessBrowserTest opens a new tab, let's wait for that first.
  WaitUntilLanguageDetected();

  content::WebContents* current_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::string adopted_language = GetLanguageFor(current_web_contents);
  EXPECT_EQ("und", adopted_language);

  // Open a new tab with a page in English.
  AddTabAtIndex(0, GURL(embedded_test_server()->GetURL("/english_page.html")),
                ui::PAGE_TRANSITION_TYPED);

  ResetObserver();

  current_web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(current_web_contents);

  WaitUntilLanguageDetected();
  adopted_language = GetLanguageFor(current_web_contents);
  EXPECT_EQ("en", adopted_language);
  EXPECT_EQ("en",
            chrome_translate_client->GetLanguageState().original_language());

  ResetObserver();

  // Now navigate to a page in French.
  ui_test_utils::NavigateToURL(
      browser(), GURL(embedded_test_server()->GetURL("/french_page.html")));

  WaitUntilLanguageDetected();
  adopted_language = GetLanguageFor(current_web_contents);
  EXPECT_EQ("fr", adopted_language);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());
}

// Test is flaky on Win http://crbug.com/166334
#if defined(OS_WIN)
#define MAYBE_PRE_TranslateSessionRestore DISABLED_PRE_TranslateSessionRestore
#else
#define MAYBE_PRE_TranslateSessionRestore PRE_TranslateSessionRestore
#endif
// Test that session restore restores the translate infobar and other translate
// settings.
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       MAYBE_PRE_TranslateSessionRestore) {
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  WaitUntilLanguageDetected();

  content::WebContents* current_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(current_web_contents);

  std::string adopted_language = GetLanguageFor(current_web_contents);
  EXPECT_EQ("und", adopted_language);

  ResetObserver();

  GURL french_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("french_page.html")));
  ui_test_utils::NavigateToURL(browser(), french_url);

  WaitUntilLanguageDetected();
  adopted_language = GetLanguageFor(current_web_contents);
  EXPECT_EQ("fr", adopted_language);
  EXPECT_EQ("fr",
            chrome_translate_client->GetLanguageState().original_language());
}

#if defined (OS_WIN)
#define MAYBE_TranslateSessionRestore DISABLED_TranslateSessionRestore
#else
#define MAYBE_TranslateSessionRestore TranslateSessionRestore
#endif
IN_PROC_BROWSER_TEST_F(TranslateManagerBrowserTest,
                       MAYBE_TranslateSessionRestore) {
  WaitUntilLanguageDetected();

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string active_adopted_language = GetLanguageFor(active_web_contents);

  content::WebContents* restored_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  std::string restored_adopted_language = GetLanguageFor(restored_web_contents);

  // One of the tabs could be still loading, let's check on that and wait,
  // if necessary.
  if (active_adopted_language.empty()) {
    ResetObserver();
    WaitUntilLanguageDetected();
    active_adopted_language = GetLanguageFor(active_web_contents);
  } else if (restored_adopted_language.empty()) {
    ResetObserver();
    WaitUntilLanguageDetected();
    restored_adopted_language = GetLanguageFor(restored_web_contents);
  }

  EXPECT_EQ("fr", restored_adopted_language);
  EXPECT_EQ("und", active_adopted_language);
}
