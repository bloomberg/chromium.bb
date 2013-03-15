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
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/events/event_constants.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

#if defined(OS_MACOSX)
const int kCtrlOrCmdMask = ui::EF_COMMAND_DOWN;
#else
const int kCtrlOrCmdMask = ui::EF_CONTROL_DOWN;
#endif

void SendKeyForBrowser(const Browser* browser,
                       ui::KeyboardCode key,
                       int modifiers) {
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser, key,
      (modifiers & ui::EF_CONTROL_DOWN) != 0,
      (modifiers & ui::EF_SHIFT_DOWN) != 0,
      (modifiers & ui::EF_ALT_DOWN) != 0,
      (modifiers & ui::EF_COMMAND_DOWN) != 0));
}

}  // namespace

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
// http://crbug.com/196592
#if defined(OS_WIN)
#define MAYBE_CopyTextInIncognito DISABLED_CopyTextInIncognito
#else
#define MAYBE_CopyTextInIncognito CopyTextInIncognito
#endif
IN_PROC_BROWSER_TEST_F(FindDialogTest, MAYBE_CopyTextInIncognito) {
  EXPECT_FALSE(browser()->profile()->IsOffTheRecord());
  Browser* browser_incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser_incognito));
  chrome::ShowFindBar(browser_incognito);
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser_incognito,
                                           VIEW_ID_FIND_IN_PAGE_TEXT_FIELD));
  string16 text(ASCIIToUTF16("foo"));
  browser_incognito->GetFindBarController()->find_bar()->SetFindText(text);

  // Select and copy.
  ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(browser_incognito,
                                            ui::VKEY_A,
                                            kCtrlOrCmdMask));
  EXPECT_TRUE(chrome::ExecuteCommand(browser_incognito, IDC_COPY));
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  EXPECT_TRUE(clipboard->IsFormatAvailable(
      ui::Clipboard::GetPlainTextFormatType(), ui::Clipboard::BUFFER_STANDARD));

  CloseBrowser(browser_incognito);
  EXPECT_FALSE(clipboard->IsFormatAvailable(
      ui::Clipboard::GetPlainTextFormatType(), ui::Clipboard::BUFFER_STANDARD));
}
