// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/clipboard.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

#if defined(OS_MACOSX)
const int kCtrlOrCmdMask = ui::EF_COMMAND_DOWN;
#else
const int kCtrlOrCmdMask = ui::EF_CONTROL_DOWN;
#endif

bool SendKeyForBrowser(const Browser* browser,
                              ui::KeyboardCode key,
                              int modifiers) {
  return ui_test_utils::SendKeyPressSync(
      browser,
      key,
      (modifiers & ui::EF_CONTROL_DOWN) != 0,
      (modifiers & ui::EF_SHIFT_DOWN) != 0,
      (modifiers & ui::EF_ALT_DOWN) != 0,
      (modifiers & ui::EF_COMMAND_DOWN) != 0);
}

} // namespace

class JavascriptDialogTest : public InProcessBrowserTest {
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

// Open message box from Incognito window, copy its text and close Incognito.
IN_PROC_BROWSER_TEST_F(JavascriptDialogTest, CopyFromIncognito) {
  EXPECT_FALSE(browser()->profile()->IsOffTheRecord());
  Browser* browser_incognito = CreateIncognitoBrowser();
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  clipboard->Clear(ui::Clipboard::BUFFER_STANDARD);

  content::WebContents* web_contents =
    browser_incognito->tab_strip_model()->GetActiveWebContents();
  web_contents->GetRenderViewHost()->ExecuteJavascriptInWebFrame(string16(),
      ASCIIToUTF16("alert('This should not crash.');"));
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(alert);
  // Some platforms handle Ctrl-C and copy text to the clipboard.
  ASSERT_TRUE(SendKeyForBrowser(browser_incognito, ui::VKEY_C, kCtrlOrCmdMask));
  alert->CloseModalDialog();

  CloseBrowser(browser_incognito);
  EXPECT_FALSE(clipboard->IsFormatAvailable(
      ui::Clipboard::GetPlainTextFormatType(), ui::Clipboard::BUFFER_STANDARD));
}
