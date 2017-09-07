// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#if defined(USE_AURA)

#include <stddef.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace {

const base::FilePath::CharType kEnglishTestPath[] =
    FILE_PATH_LITERAL("english_page.html");
const base::FilePath::CharType kFrenchTestPath[] =
    FILE_PATH_LITERAL("french_page.html");

using LanguageInfo = language::UrlLanguageHistogram::LanguageInfo;

};  // namespace

class TranslateLanguageBrowserTest : public InProcessBrowserTest {
 public:
  TranslateLanguageBrowserTest() : browser_(nullptr) {}

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    translate::TranslateManager::SetIgnoreMissingKeyForTesting(true);
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override { InProcessBrowserTest::TearDown(); }

 protected:
  void SwitchToIncognitoMode() { browser_ = CreateIncognitoBrowser(); }

  void CheckForTranslateUI(const base::FilePath& path, bool expect_translate) {
    // Set browser_ here because |browser_| is not available during the
    // InProcessBrowserTest parameter initialization phase.
    if (!browser_) {
      browser_ = browser();
    }
    const content::WebContents* const current_web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    const content::Source<content::WebContents> source(current_web_contents);
    ui_test_utils::WindowedNotificationObserverWithDetails<
        translate::LanguageDetectionDetails>
        language_detected_signal(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                                 source);

    const GURL url = ui_test_utils::GetTestUrl(base::FilePath(), path);
    ui_test_utils::NavigateToURL(browser_, url);
    language_detected_signal.Wait();
    TranslateBubbleView* const bubble = TranslateBubbleView::GetCurrentBubble();
    EXPECT_NE(expect_translate, bubble == nullptr);
  }

  language::UrlLanguageHistogram* GetUrlLanguageHistogram() {
    const content::WebContents* const web_contents =
        browser_->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(web_contents);
    content::BrowserContext* const browser_context =
        web_contents->GetBrowserContext();
    EXPECT_TRUE(browser_context);
    return UrlLanguageHistogramFactory::GetForBrowserContext(browser_context);
  }

 private:
  Browser* browser_;
  DISALLOW_COPY_AND_ASSIGN(TranslateLanguageBrowserTest);
};

IN_PROC_BROWSER_TEST_F(TranslateLanguageBrowserTest, LanguageModelLogSucceed) {
  for (int i = 0; i < 10; ++i) {
    ASSERT_NO_FATAL_FAILURE(
        CheckForTranslateUI(base::FilePath(kFrenchTestPath), true));
    ASSERT_NO_FATAL_FAILURE(
        CheckForTranslateUI(base::FilePath(kEnglishTestPath), false));
  }
  // Intentionally visit the french page one more time.
  ASSERT_NO_FATAL_FAILURE(
      CheckForTranslateUI(base::FilePath(kFrenchTestPath), true));

  // We should expect fr and en. fr should be 11 / (11 + 10) = 0.5238.
  const language::UrlLanguageHistogram* const histograms =
      GetUrlLanguageHistogram();
  ASSERT_TRUE(histograms);
  const std::vector<LanguageInfo>& langs = histograms->GetTopLanguages();
  EXPECT_EQ(2u, langs.size());
  EXPECT_EQ("fr", langs[0].language_code);
  EXPECT_EQ("en", langs[1].language_code);
  EXPECT_NEAR(11.0 / (11.0 + 10.0), langs[0].frequency, 0.001f);
  EXPECT_NEAR(10.0 / (11.0 + 10.0), langs[1].frequency, 0.001f);
}

IN_PROC_BROWSER_TEST_F(TranslateLanguageBrowserTest, DontLogInIncognito) {
  SwitchToIncognitoMode();
  for (int i = 0; i < 10; ++i) {
    ASSERT_NO_FATAL_FAILURE(
        CheckForTranslateUI(base::FilePath(kEnglishTestPath), false));
    ASSERT_NO_FATAL_FAILURE(
        CheckForTranslateUI(base::FilePath(kFrenchTestPath), true));
  }
  // We should expect no url language histograms.
  const language::UrlLanguageHistogram* const histograms =
      GetUrlLanguageHistogram();
  EXPECT_FALSE(histograms);
}

#endif  // defined(USE_AURA)
