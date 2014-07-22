// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_view.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/translate/cld_data_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/notification_details.h"

class TranslateBubbleViewBrowserTest : public InProcessBrowserTest {
 public:
  TranslateBubbleViewBrowserTest()
      : cld_data_harness(test::CreateCldDataHarness()) {}
  virtual ~TranslateBubbleViewBrowserTest() {}
  virtual void SetUpOnMainThread() OVERRIDE {
    // We can't Init() until PathService has been initialized. This happens
    // very late in the test fixture setup process.
    cld_data_harness->Init();
    InProcessBrowserTest::SetUpOnMainThread();
  }

 private:
  scoped_ptr<test::CldDataHarness> cld_data_harness;
  DISALLOW_COPY_AND_ASSIGN(TranslateBubbleViewBrowserTest);
};

// Flaky: crbug.com/394066
IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest,
                       DISABLED_CloseBrowserWithoutTranslating) {
  EXPECT_FALSE(TranslateBubbleView::IsShowing());

  // Show a French page and wait until the bubble is shown.
  content::WebContents* current_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::Source<content::WebContents> source(current_web_contents);
  ui_test_utils::WindowedNotificationObserverWithDetails<
      translate::LanguageDetectionDetails>
      fr_language_detected_signal(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                                  source);
  GURL french_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("french_page.html")));
  ui_test_utils::NavigateToURL(browser(), french_url);
  fr_language_detected_signal.Wait();
  EXPECT_TRUE(TranslateBubbleView::IsShowing());

  // Close the window without translating.
  chrome::CloseWindow(browser());
  EXPECT_FALSE(TranslateBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest,
                       CloseLastTabWithoutTranslating) {
  EXPECT_FALSE(TranslateBubbleView::IsShowing());

  // Show a French page and wait until the bubble is shown.
  content::WebContents* current_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::Source<content::WebContents> source(current_web_contents);
  ui_test_utils::WindowedNotificationObserverWithDetails<
      translate::LanguageDetectionDetails>
      fr_language_detected_signal(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                                  source);
  GURL french_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("french_page.html")));
  ui_test_utils::NavigateToURL(browser(), french_url);
  fr_language_detected_signal.Wait();
  EXPECT_TRUE(TranslateBubbleView::IsShowing());

  // Close the tab without translating.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  chrome::CloseWebContents(browser(), current_web_contents, false);
  EXPECT_FALSE(TranslateBubbleView::IsShowing());
}

IN_PROC_BROWSER_TEST_F(TranslateBubbleViewBrowserTest,
                       CloseAnotherTabWithoutTranslating) {
  EXPECT_FALSE(TranslateBubbleView::IsShowing());

  int active_index = browser()->tab_strip_model()->active_index();

  // Open another tab to load a French page on background.
  int french_index = active_index + 1;
  GURL french_url = ui_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("french_page.html")));
  chrome::AddTabAt(browser(), french_url, french_index, false);
  EXPECT_EQ(active_index, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Wait until the language is detected.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(french_index);
  content::Source<content::WebContents> source(web_contents);
  ui_test_utils::WindowedNotificationObserverWithDetails<
      translate::LanguageDetectionDetails>
      fr_language_detected_signal(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                                  source);
  fr_language_detected_signal.Wait();

  // The bubble is not shown because the tab is not activated.
  EXPECT_FALSE(TranslateBubbleView::IsShowing());

  // Close the French page tab immediately.
  chrome::CloseWebContents(browser(), web_contents, false);
  EXPECT_EQ(active_index, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_FALSE(TranslateBubbleView::IsShowing());

  // Close the last tab.
  chrome::CloseWebContents(browser(),
                           browser()->tab_strip_model()->GetActiveWebContents(),
                           false);
}
