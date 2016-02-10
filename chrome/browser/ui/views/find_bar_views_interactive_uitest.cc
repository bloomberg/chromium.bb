// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;
using content::WebContents;

namespace {

static const char kSimplePage[] = "/find_in_page/simple.html";

class FindInPageTest : public InProcessBrowserTest {
 public:
  FindInPageTest() {
    FindBarHost::disable_animations_during_testing_ = true;
  }

  base::string16 GetFindBarText() {
    FindBar* find_bar = browser()->GetFindBarController()->find_bar();
    return find_bar->GetFindText();
  }

  base::string16 GetFindBarSelectedText() {
    FindBarTesting* find_bar =
        browser()->GetFindBarController()->find_bar()->GetFindBarTesting();
    return find_bar->GetFindSelectedText();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FindInPageTest);
};

}  // namespace

// Flaky because the test server fails to start? See: http://crbug.com/96594.
IN_PROC_BROWSER_TEST_F(FindInPageTest, CrashEscHandlers) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our test page (tab A).
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::Find(browser());

  // Open another tab (tab B).
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);

  chrome::Find(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Select tab A.
  browser()->tab_strip_model()->ActivateTabAt(0, true);

  // Close tab B.
  browser()->tab_strip_model()->CloseWebContentsAt(1,
                                                   TabStripModel::CLOSE_NONE);

  // Click on the location bar so that Find box loses focus.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::ClickOnView(browser(),
                                                     VIEW_ID_OMNIBOX));
  // Check the location bar is focused.
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // This used to crash until bug 1303709 was fixed.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));
}

IN_PROC_BROWSER_TEST_F(FindInPageTest, NavigationByKeyEvent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // First we navigate to any page.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kSimplePage));
  // Show the Find bar.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(
      ui_test_utils::IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), ASCIIToUTF16("a"),
      true, false, NULL, NULL);

  // The previous button should still be focused after pressing [Enter] on it.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  EXPECT_TRUE(
      ui_test_utils::IsViewFocused(browser(),
      VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON));

  // The next button should still be focused after pressing [Enter] on it.
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), ASCIIToUTF16("b"),
      true, false, NULL, NULL);
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB, false,
                                              false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_RETURN, false,
                                              false, false, false));
  EXPECT_TRUE(
      ui_test_utils::IsViewFocused(browser(),
      VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON));
}

// TODO(mpistrich): Enable again when ui_test_utils::ClickOnView works with find
// bar view IDs.
// http://crbug.com/584043
IN_PROC_BROWSER_TEST_F(FindInPageTest, DISABLED_NavigationByMouse) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  // First we navigate to any page.
  ui_test_utils::NavigateToURL(browser(),
                               embedded_test_server()->GetURL(kSimplePage));
  // Show the Find bar.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(
      ui_test_utils::IsViewFocused(browser(), VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(), ASCIIToUTF16("a"),
      true, false, NULL, NULL);

  // The textfield should be focused after clicking on any button.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_FIND_IN_PAGE_PREVIOUS_BUTTON);
  EXPECT_TRUE(
      ui_test_utils::IsViewFocused(browser(),
      VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // The textfield should be focused after clicking on any button.
  ui_test_utils::ClickOnView(browser(), VIEW_ID_FIND_IN_PAGE_NEXT_BUTTON);
  EXPECT_TRUE(
      ui_test_utils::IsViewFocused(browser(),
      VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#define MAYBE_FocusRestore DISABLED_FocusRestore
#else
#define MAYBE_FocusRestore FocusRestore
#endif

// Flaky because the test server fails to start? See: http://crbug.com/96594.
IN_PROC_BROWSER_TEST_F(FindInPageTest, MAYBE_FocusRestore) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url = embedded_test_server()->GetURL("/title1.html");
  ui_test_utils::NavigateToURL(browser(), url);

  // Focus the location bar, open and close the find-in-page, focus should
  // return to the location bar.
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  // Ensure the creation of the find bar controller.
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelectionOnPage,
      FindBarController::kKeepResultsInFindBox);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Focus the location bar, find something on the page, close the find box,
  // focus should go to the page.
  chrome::FocusLocationBar(browser());
  chrome::Find(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      ASCIIToUTF16("a"), true, false, NULL, NULL);
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelectionOnPage,
      FindBarController::kKeepResultsInFindBox);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_TAB_CONTAINER));

  // Focus the location bar, open and close the find box, focus should return to
  // the location bar (same as before, just checking that http://crbug.com/23599
  // is fixed).
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
  browser()->GetFindBarController()->Show();
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  browser()->GetFindBarController()->EndFindSession(
      FindBarController::kKeepSelectionOnPage,
      FindBarController::kKeepResultsInFindBox);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
}


// TODO(phajdan.jr): Disabling due to possible timing issues on XP
// interactive_ui_tests.
// http://crbug.com/311363
IN_PROC_BROWSER_TEST_F(FindInPageTest, DISABLED_SelectionRestoreOnTabSwitch) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // First we navigate to any page in the current tab (tab A).
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // Search for "abc".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_B, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_C, false, false, false, false));
  EXPECT_EQ(ASCIIToUTF16("abc"), GetFindBarText());

  // Select "bc".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_LEFT, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_LEFT, false, true, false, false));
  EXPECT_EQ(ASCIIToUTF16("bc"), GetFindBarSelectedText());

  // Open another tab (tab B).
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // Search for "def".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_D, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_E, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, false, false, false));
  EXPECT_EQ(ASCIIToUTF16("def"), GetFindBarText());

  // Select "de".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_HOME, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RIGHT, false, true, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RIGHT, false, true, false, false));
  EXPECT_EQ(ASCIIToUTF16("de"), GetFindBarSelectedText());

  // Select tab A. Find bar should select "bc".
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  EXPECT_EQ(ASCIIToUTF16("bc"), GetFindBarSelectedText());

  // Select tab B. Find bar should select "de".
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  EXPECT_EQ(ASCIIToUTF16("de"), GetFindBarSelectedText());
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(USE_AURA)
// TODO(erg): linux_aura bringup: http://crbug.com/163931
#define MAYBE_FocusRestoreOnTabSwitch DISABLED_FocusRestoreOnTabSwitch
#else
#define MAYBE_FocusRestoreOnTabSwitch FocusRestoreOnTabSwitch
#endif

// Flaky because the test server fails to start? See: http://crbug.com/96594.
IN_PROC_BROWSER_TEST_F(FindInPageTest, MAYBE_FocusRestoreOnTabSwitch) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // First we navigate to our test page (tab A).
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  chrome::Find(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Search for 'a'.
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      ASCIIToUTF16("a"), true, false, NULL, NULL);
  EXPECT_EQ(ASCIIToUTF16("a"), GetFindBarSelectedText());

  // Open another tab (tab B).
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Make sure Find box is open.
  chrome::Find(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Search for 'b'.
  ui_test_utils::FindInPage(
      browser()->tab_strip_model()->GetActiveWebContents(),
      ASCIIToUTF16("b"), true, false, NULL, NULL);
  EXPECT_EQ(ASCIIToUTF16("b"), GetFindBarSelectedText());

  // Set focus away from the Find bar (to the Location bar).
  chrome::FocusLocationBar(browser());
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));

  // Select tab A. Find bar should get focus.
  browser()->tab_strip_model()->ActivateTabAt(0, true);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  EXPECT_EQ(ASCIIToUTF16("a"), GetFindBarSelectedText());

  // Select tab B. Location bar should get focus.
  browser()->tab_strip_model()->ActivateTabAt(1, true);
  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(), VIEW_ID_OMNIBOX));
}

// FindInPage on Mac doesn't use prepopulated values. Search there is global.
#if !defined(OS_MACOSX) && !defined(USE_AURA)
// Flaky because the test server fails to start? See: http://crbug.com/96594.
// This tests that whenever you clear values from the Find box and close it that
// it respects that and doesn't show you the last search, as reported in bug:
// http://crbug.com/40121. For Aura see bug http://crbug.com/292299.
IN_PROC_BROWSER_TEST_F(FindInPageTest, PrepopulateRespectBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // First we navigate to any page.
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // Search for "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));

  // We should find "a" here.
  EXPECT_EQ(ASCIIToUTF16("a"), GetFindBarText());

  // Delete "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_BACK, false, false, false, false));

  // Validate we have cleared the text.
  EXPECT_EQ(base::string16(), GetFindBarText());

  // Close the Find box.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  // After the Find box has been reopened, it should not have been prepopulated
  // with "a" again.
  EXPECT_EQ(base::string16(), GetFindBarText());

  // Close the Find box.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_ESCAPE, false, false, false, false));

  // Press F3 to trigger FindNext.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F3, false, false, false, false));

  // After the Find box has been reopened, it should still have no prepopulate
  // value.
  EXPECT_EQ(base::string16(), GetFindBarText());
}
#endif

// Flaky on Win. http://crbug.com/92467
// Flaky on ChromeOS. http://crbug.com/118216
// Flaky on linux aura. http://crbug.com/163931
#if defined(TOOLKIT_VIEWS)
#define MAYBE_PasteWithoutTextChange DISABLED_PasteWithoutTextChange
#else
#define MAYBE_PasteWithoutTextChange PasteWithoutTextChange
#endif

IN_PROC_BROWSER_TEST_F(FindInPageTest, MAYBE_PasteWithoutTextChange) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Make sure Chrome is in the foreground, otherwise sending input
  // won't do anything and the test will hang.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // First we navigate to any page.
  GURL url = embedded_test_server()->GetURL(kSimplePage);
  ui_test_utils::NavigateToURL(browser(), url);

  // Show the Find bar.
  browser()->GetFindBarController()->Show();

  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // Search for "a".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_A, false, false, false, false));

  // We should find "a" here.
  EXPECT_EQ(ASCIIToUTF16("a"), GetFindBarText());

  // Reload the page to clear the matching result.
  chrome::Reload(browser(), CURRENT_TAB);

  // Focus the Find bar again to make sure the text is selected.
  browser()->GetFindBarController()->Show();

  EXPECT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));

  // "a" should be selected.
  EXPECT_EQ(ASCIIToUTF16("a"), GetFindBarSelectedText());

  // Press Ctrl-C to copy the content.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_C, true, false, false, false));

  base::string16 str;
  ui::Clipboard::GetForCurrentThread()->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE,
                                                 &str);

  // Make sure the text is copied successfully.
  EXPECT_EQ(ASCIIToUTF16("a"), str);

  // Press Ctrl-V to paste the content back, it should start finding even if the
  // content is not changed.
  content::Source<WebContents> notification_source(
      browser()->tab_strip_model()->GetActiveWebContents());
  ui_test_utils::WindowedNotificationObserverWithDetails
      <FindNotificationDetails> observer(
          chrome::NOTIFICATION_FIND_RESULT_AVAILABLE, notification_source);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_V, true, false, false, false));

  ASSERT_NO_FATAL_FAILURE(observer.Wait());
  FindNotificationDetails details;
  ASSERT_TRUE(observer.GetDetailsFor(notification_source.map_key(), &details));
  EXPECT_TRUE(details.number_of_matches() > 0);
}

#if defined(OS_WIN)
// TODO(phajdan.jr): Disabling due to possible timing issues on XP
// interactive_ui_tests.
// http://crbug.com/311363
IN_PROC_BROWSER_TEST_F(FindInPageTest, DISABLED_CtrlEnter) {
  ui_test_utils::NavigateToURL(browser(),
                               GURL("data:text/html,This is some text with a "
                                    "<a href=\"about:blank\">link</a>."));

  browser()->GetFindBarController()->Show();

  // Search for "link".
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_L, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_I, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_N, false, false, false, false));
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_K, false, false, false, false));
  EXPECT_EQ(ASCIIToUTF16("link"), GetFindBarText());

  ui_test_utils::UrlLoadObserver observer(
      GURL("about:blank"), content::NotificationService::AllSources());

  // Send Ctrl-Enter, should cause navigation to about:blank.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_RETURN, true, false, false, false));

  observer.Wait();
}
#endif
