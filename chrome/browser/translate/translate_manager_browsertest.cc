// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_manager.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/cld_data_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/translate/core/common/language_detection_details.h"
#include "url/gurl.h"

class TranslateManagerBrowserTest : public InProcessBrowserTest {};

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
  scoped_ptr<test::CldDataHarness> cld_data_harness =
      test::CreateCldDataHarness();
  ASSERT_NO_FATAL_FAILURE(cld_data_harness->Init());
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  content::WebContents* current_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(current_web_contents);
  content::Source<content::WebContents> source(current_web_contents);

  ui_test_utils::WindowedNotificationObserverWithDetails<
      translate::LanguageDetectionDetails>
      fr_language_detected_signal(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                                  source);

  GURL french_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("french_page.html")));
  ui_test_utils::NavigateToURL(browser(), french_url);
  fr_language_detected_signal.Wait();
  translate::LanguageDetectionDetails details;
  EXPECT_TRUE(fr_language_detected_signal.GetDetailsFor(
        source.map_key(), &details));
  EXPECT_EQ("fr", details.adopted_language);
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
  scoped_ptr<test::CldDataHarness> cld_data_harness =
      test::CreateCldDataHarness();
  ASSERT_NO_FATAL_FAILURE(cld_data_harness->Init());
  content::WebContents* current_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::Source<content::WebContents> source(current_web_contents);

  ui_test_utils::WindowedNotificationObserverWithDetails<
      translate::LanguageDetectionDetails>
      fr_language_detected_signal(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                                  source);
  fr_language_detected_signal.Wait();
}
