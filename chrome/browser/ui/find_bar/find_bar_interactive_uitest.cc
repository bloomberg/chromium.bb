// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/find_bar/find_bar.h"

#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_state.h"
#include "chrome/browser/ui/find_bar/find_bar_state_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/base/clipboard/clipboard.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

int FindInPage(Browser* browser,
               const string16& search_str16,
               bool forward,
               bool case_sensitive,
               int* ordinal) {
  browser->GetFindBarController()->find_bar()->SetFindText(search_str16);
  return ui_test_utils::FindInPage(
      browser->tab_strip_model()->GetActiveWebContents(),
      search_str16,
      forward,
      case_sensitive,
      ordinal,
      NULL);
}

}

class FindDialogTest : public InProcessBrowserTest {
 protected:
  void CloseBrowser(Browser* browser) {
    content::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::Source<Browser>(browser));
    chrome::CloseWindow(browser);

#if defined(OS_MACOSX)
    // BrowserWindowController depends on the auto release pool being recycled
    // in the message loop to delete itself, which frees the Browser object
    // which fires this event.
    AutoreleasePool()->Recycle();
#endif

    signal.Wait();
  }
};

// Copy text in Incognito window, close window, check the test disappeared from
// the clipboard.
IN_PROC_BROWSER_TEST_F(FindDialogTest, CopyTextInIncognito) {
  EXPECT_FALSE(browser()->profile()->IsOffTheRecord());
  Browser* browser_incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser_incognito));
  const string16 find_text(ASCIIToUTF16("foo"));

#if !defined(OS_MACOSX)
  // Set prepopulated text, find bar will appear with this text being selected.
  // Mac ignores this and reads Find pasteboard instead.
  FindBarState* find_bar_state =
      FindBarStateFactory::GetForProfile(browser_incognito->profile());
  find_bar_state->set_last_prepopulate_text(find_text);
#endif

  chrome::ShowFindBar(browser_incognito);

#if defined(OS_MACOSX)
  FindInPage(browser_incognito, find_text, true, true, NULL);
#endif

  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser_incognito,
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  EXPECT_TRUE(chrome::ExecuteCommand(browser_incognito, IDC_COPY));
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  string16 clipboard_str;
  clipboard->ReadText(ui::Clipboard::BUFFER_STANDARD, &clipboard_str);
  // Make sure the text is copied successfully.
  EXPECT_EQ(find_text, clipboard_str);

  CloseBrowser(browser_incognito);
  EXPECT_FALSE(clipboard->IsFormatAvailable(
      ui::Clipboard::GetPlainTextFormatType(), ui::Clipboard::BUFFER_STANDARD));
}
