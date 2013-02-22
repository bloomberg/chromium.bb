// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/events/event_constants.h"

class IncognitoSelectionClipboardTest : public InProcessBrowserTest {
 protected:
  static ui::Clipboard* clipboard() {
    static ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
    return clipboard;
  }

  static void ExpectStringInClipboard(const string16& pattern,
                               ui::Clipboard::Buffer buffer) {
    string16 content;
    clipboard()->ReadText(buffer, &content);
    EXPECT_EQ(pattern, content);
  }

  void CloseBrowser(Browser* browser) {
    content::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::Source<Browser>(browser));
    chrome::CloseWindow(browser);

    signal.Wait();
  }

  static void SendKeyForBrowser(const Browser* browser,
                                ui::KeyboardCode key,
                                int modifiers) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser, key,
        (modifiers & ui::EF_CONTROL_DOWN) != 0,
        (modifiers & ui::EF_SHIFT_DOWN) != 0,
        (modifiers & ui::EF_ALT_DOWN) != 0,
        (modifiers & ui::EF_COMMAND_DOWN) != 0));
  }

  static void WriteClipboardCallback(ui::Clipboard::Buffer expected,
                                     const base::Closure& closure,
                                     ui::Clipboard::Buffer actual) {
    if (expected == actual)
      closure.Run();
  }
};

// Tests that data selected in content area of incognito window is destroyed in
// the selection clipboard after profile destruction.
// The test is executed on Linux only because it's the only OS with the
// selection clipboard.
#if defined(USE_X11) && !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(IncognitoSelectionClipboardTest,
                       ClearContentDataOnSelect) {
  EXPECT_FALSE(browser()->profile()->IsOffTheRecord());
  Browser* browser_incognito = CreateIncognitoBrowser();
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser_incognito));
  ui_test_utils::NavigateToURL(browser_incognito, GURL("data:text/plain,foo"));
  ui_test_utils::ClickOnView(browser_incognito, VIEW_ID_TAB_CONTAINER);
  ASSERT_TRUE(ui_test_utils::IsViewFocused(browser_incognito,
                                           VIEW_ID_TAB_CONTAINER));
  // Select web-page content.
  ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(browser_incognito,
                                            ui::VKEY_A,
                                            ui::EF_CONTROL_DOWN));
  // Run message loop until data have been written to the selection clipboard.
  // This happens on UI thread.
  scoped_refptr<content::MessageLoopRunner> message_loop_runner(
      new content::MessageLoopRunner);
  clipboard()->set_write_objects_callback_for_testing(
      base::Bind(&WriteClipboardCallback,
                 ui::Clipboard::BUFFER_SELECTION,
                 message_loop_runner->QuitClosure()));
  message_loop_runner->Run();
  ExpectStringInClipboard(ASCIIToUTF16("foo"), ui::Clipboard::BUFFER_SELECTION);

  CloseBrowser(browser_incognito);
  ExpectStringInClipboard(string16(), ui::Clipboard::BUFFER_SELECTION);
}
#endif

// Tests that data copied in content area of incognito window is destroyed in
// the selection clipboard after profile destruction.
// The test is executed on Linux only because it's the only OS with the
// selection clipboard.
#if defined(USE_X11) && !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(IncognitoSelectionClipboardTest,
                       ClearContentDataOnCopy) {
  EXPECT_FALSE(browser()->profile()->IsOffTheRecord());
  Browser* browser_incognito = CreateIncognitoBrowser();
  ui_test_utils::NavigateToURL(browser_incognito, GURL("data:text/plain,foo"));
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser_incognito));
  // Select and copy web-page content.
  ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(browser_incognito,
                                            ui::VKEY_A,
                                            ui::EF_CONTROL_DOWN));
  ASSERT_NO_FATAL_FAILURE(SendKeyForBrowser(browser_incognito,
                                            ui::VKEY_C,
                                            ui::EF_CONTROL_DOWN));
  // Run message loop until data have been written to the primary clipboard and
  // automatically propagated to the selection clipboard by ui::Clipboard.
  // This happens on UI thread.
  scoped_refptr<content::MessageLoopRunner> message_loop_runner(
      new content::MessageLoopRunner);
  clipboard()->set_write_objects_callback_for_testing(
      base::Bind(&WriteClipboardCallback,
                 ui::Clipboard::BUFFER_STANDARD,
                 message_loop_runner->QuitClosure()));
  message_loop_runner->Run();
  ExpectStringInClipboard(ASCIIToUTF16("foo"), ui::Clipboard::BUFFER_SELECTION);
  ExpectStringInClipboard(ASCIIToUTF16("foo"), ui::Clipboard::BUFFER_STANDARD);

  CloseBrowser(browser_incognito);
  ExpectStringInClipboard(string16(), ui::Clipboard::BUFFER_SELECTION);
  ExpectStringInClipboard(string16(), ui::Clipboard::BUFFER_STANDARD);
}
#endif
