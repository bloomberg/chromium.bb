// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This functionality currently works on Windows and on Linux when
// toolkit_views is defined (i.e. for Chrome OS). It's not needed
// on the Mac, and it's not yet implemented on Linux.

#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/events/event_constants.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/ui_controls/ui_controls.h"
#include "ui/views/controls/menu/menu_listener.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// An async version of SendKeyPressSync since we don't get notified when a
// menu is showing.
void SendKeyPress(Browser* browser, ui::KeyboardCode key) {
  ASSERT_TRUE(ui_controls::SendKeyPress(
      browser->window()->GetNativeWindow(), key, false, false, false, false));
}

// Helper class that waits until the focus has changed to a view other
// than the one with the provided view id.
class ViewFocusChangeWaiter : public views::FocusChangeListener {
 public:
  ViewFocusChangeWaiter(views::FocusManager* focus_manager,
                        int previous_view_id)
      : focus_manager_(focus_manager),
        previous_view_id_(previous_view_id),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
    focus_manager_->AddFocusChangeListener(this);
    // Call the focus change notification once in case the focus has
    // already changed.
    OnWillChangeFocus(NULL, focus_manager_->GetFocusedView());
  }

  virtual ~ViewFocusChangeWaiter() {
    focus_manager_->RemoveFocusChangeListener(this);
  }

  void Wait() {
    content::RunMessageLoop();
  }

 private:
  // Inherited from FocusChangeListener
  virtual void OnWillChangeFocus(views::View* focused_before,
                                 views::View* focused_now) {
  }

  virtual void OnDidChangeFocus(views::View* focused_before,
                                views::View* focused_now) {
    if (focused_now && focused_now->id() != previous_view_id_) {
      MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    }
  }

  views::FocusManager* focus_manager_;
  int previous_view_id_;
  base::WeakPtrFactory<ViewFocusChangeWaiter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ViewFocusChangeWaiter);
};

class SendKeysMenuListener : public views::MenuListener {
 public:
  SendKeysMenuListener(ToolbarView* toolbar_view, Browser* browser)
      : toolbar_view_(toolbar_view), browser_(browser) {
    toolbar_view_->AddMenuListener(this);
  }

  virtual ~SendKeysMenuListener() {}

 private:
  // Overridden from views::MenuListener:
  virtual void OnMenuOpened() OVERRIDE {
    toolbar_view_->RemoveMenuListener(this);
    // Press DOWN to select the first item, then RETURN to select it.
    SendKeyPress(browser_, ui::VKEY_DOWN);
    SendKeyPress(browser_, ui::VKEY_RETURN);
  }

  ToolbarView* toolbar_view_;
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(SendKeysMenuListener);
};

class KeyboardAccessTest : public InProcessBrowserTest {
 public:
  KeyboardAccessTest() {}

  // Use the keyboard to select "New Tab" from the app menu.
  // This test depends on the fact that there is one menu and that
  // New Tab is the first item in the menu. If the menus change,
  // this test will need to be changed to reflect that.
  //
  // If alternate_key_sequence is true, use "Alt" instead of "F10" to
  // open the menu bar, and "Down" instead of "Enter" to open a menu.
  void TestMenuKeyboardAccess(bool alternate_key_sequence, bool shift);

  int GetFocusedViewID() {
    gfx::NativeWindow window = browser()->window()->GetNativeWindow();
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    const views::FocusManager* focus_manager = widget->GetFocusManager();
    const views::View* focused_view = focus_manager->GetFocusedView();
    return focused_view ? focused_view->id() : -1;
  }

  void WaitForFocusedViewIDToChange(int original_view_id) {
    if (GetFocusedViewID() != original_view_id)
      return;
    gfx::NativeWindow window = browser()->window()->GetNativeWindow();
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    views::FocusManager* focus_manager = widget->GetFocusManager();
    ViewFocusChangeWaiter waiter(focus_manager, original_view_id);
    waiter.Wait();
  }

  DISALLOW_COPY_AND_ASSIGN(KeyboardAccessTest);
};

void KeyboardAccessTest::TestMenuKeyboardAccess(bool alternate_key_sequence,
                                                bool shift) {
  // Navigate to a page in the first tab, which makes sure that focus is
  // set to the browser window.
  ui_test_utils::NavigateToURL(browser(), GURL("about:"));

  // The initial tab index should be 0.
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  // Get the focused view ID, then press a key to activate the
  // page menu, then wait until the focused view changes.
  int original_view_id = GetFocusedViewID();

  content::WindowedNotificationObserver new_tab_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::Source<content::WebContentsDelegate>(browser()));

  BrowserView* browser_view = reinterpret_cast<BrowserView*>(
      browser()->window());
  ToolbarView* toolbar_view = browser_view->GetToolbarView();
  SendKeysMenuListener menu_listener(toolbar_view, browser());

#if defined(OS_CHROMEOS)
  // Chrome OS doesn't have a way to just focus the wrench menu, so we use Alt+F
  // to bring up the menu.
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_F, false, shift, true, false));
#else
  ui::KeyboardCode menu_key =
      alternate_key_sequence ? ui::VKEY_MENU : ui::VKEY_F10;
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), menu_key, false, shift, false, false));
#endif

  if (shift) {
    // Verify Chrome does not move the view focus. We should not move the view
    // focus when typing a menu key with modifier keys, such as shift keys or
    // control keys.
    int new_view_id = GetFocusedViewID();
    ASSERT_EQ(original_view_id, new_view_id);
    return;
  }

  WaitForFocusedViewIDToChange(original_view_id);

  // See above comment. Since we already brought up the menu, no need to do this
  // on ChromeOS.
#if !defined(OS_CHROMEOS)
  if (alternate_key_sequence)
    SendKeyPress(browser(), ui::VKEY_DOWN);
  else
    SendKeyPress(browser(), ui::VKEY_RETURN);
#endif

  // Wait for the new tab to appear.
  new_tab_observer.Wait();

  // Make sure that the new tab index is 1.
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
}

// http://crbug.com/62310.
#if defined(OS_CHROMEOS)
#define MAYBE_TestMenuKeyboardAccess DISABLED_TestMenuKeyboardAccess
#else
#define MAYBE_TestMenuKeyboardAccess TestMenuKeyboardAccess
#endif

IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, MAYBE_TestMenuKeyboardAccess) {
  TestMenuKeyboardAccess(false, false);
}

// http://crbug.com/62310.
#if defined(OS_CHROMEOS)
#define MAYBE_TestAltMenuKeyboardAccess DISABLED_TestAltMenuKeyboardAccess
#else
#define MAYBE_TestAltMenuKeyboardAccess TestAltMenuKeyboardAccess
#endif

IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, MAYBE_TestAltMenuKeyboardAccess) {
  TestMenuKeyboardAccess(true, false);
}

// If this flakes, use http://crbug.com/62311.
#if defined(OS_WIN)
#define MAYBE_TestShiftAltMenuKeyboardAccess DISABLED_TestShiftAltMenuKeyboardAccess
#else
#define MAYBE_TestShiftAltMenuKeyboardAccess TestShiftAltMenuKeyboardAccess
#endif
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest,
                       MAYBE_TestShiftAltMenuKeyboardAccess) {
  TestMenuKeyboardAccess(true, true);
}

// Test that JavaScript cannot intercept reserved keyboard accelerators like
// ctrl-t to open a new tab or ctrl-f4 to close a tab.
// TODO(isherman): This test times out on ChromeOS.  We should merge it with
// BrowserKeyEventsTest.ReservedAccelerators, but just disable for now.
// If this flakes, use http://crbug.com/62311.
IN_PROC_BROWSER_TEST_F(KeyboardAccessTest, ReserveKeyboardAccelerators) {
  const std::string kBadPage =
      "<html><script>"
      "document.onkeydown = function() {"
      "  event.preventDefault();"
      "  return false;"
      "}"
      "</script></html>";
  GURL url("data:text/html," + kBadPage);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_TAB, true, false, false, false));
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  ASSERT_EQ(2, browser()->tab_strip_model()->active_index());

  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
      browser(), ui::VKEY_W, true, false, false, false));
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
}

}  // namespace
