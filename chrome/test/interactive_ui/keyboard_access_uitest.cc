// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_timeouts.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "views/event.h"

// This functionality currently works on Windows and on Linux when
// toolkit_views is defined (i.e. for Chrome OS). It's not needed
// on the Mac, and it's not yet implemented on Linux.
#if defined(TOOLKIT_VIEWS)

namespace {

class KeyboardAccessTest : public UITest {
 public:
  KeyboardAccessTest() {
    dom_automation_enabled_ = true;
    show_window_ = true;
  }

  // Use the keyboard to select "New Tab" from the app menu.
  // This test depends on the fact that there is one menu and that
  // New Tab is the first item in the menu. If the menus change,
  // this test will need to be changed to reflect that.
  //
  // If alternate_key_sequence is true, use "Alt" instead of "F10" to
  // open the menu bar, and "Down" instead of "Enter" to open a menu.
  void TestMenuKeyboardAccess(bool alternate_key_sequence, int modifiers);

  DISALLOW_COPY_AND_ASSIGN(KeyboardAccessTest);
};

void KeyboardAccessTest::TestMenuKeyboardAccess(bool alternate_key_sequence,
                                                int modifiers) {
  scoped_refptr<BrowserProxy> browser = automation()->GetBrowserWindow(0);
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window = browser->GetWindow();
  ASSERT_TRUE(window.get());

  // Navigate to a page in the first tab, which makes sure that focus is
  // set to the browser window.
  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());

  ASSERT_TRUE(tab->NavigateToURL(GURL("about:")));

  // The initial tab index should be 0.
  int tab_index = -1;
  ASSERT_TRUE(browser->GetActiveTabIndex(&tab_index));
  ASSERT_EQ(0, tab_index);

  // Get the focused view ID, then press a key to activate the
  // page menu, then wait until the focused view changes.
  int original_view_id = -1;
  ASSERT_TRUE(window->GetFocusedViewID(&original_view_id));

  ui::KeyboardCode menu_key =
      alternate_key_sequence ? ui::VKEY_MENU : ui::VKEY_F10;
#if defined(OS_CHROMEOS)
  // Chrome OS has different function key accelerators, so we always use the
  // menu key there.
  menu_key = ui::VKEY_MENU;
#endif
  ASSERT_TRUE(window->SimulateOSKeyPress(menu_key, modifiers));

  if (modifiers) {
    // Verify Chrome does not move the view focus. We should not move the view
    // focus when typing a menu key with modifier keys, such as shift keys or
    // control keys.
    int new_view_id = -1;
    ASSERT_TRUE(window->GetFocusedViewID(&new_view_id));
    ASSERT_EQ(original_view_id, new_view_id);
    return;
  }

  int new_view_id = -1;
  ASSERT_TRUE(window->WaitForFocusedViewIDToChange(
      original_view_id, &new_view_id));

  ASSERT_TRUE(browser->StartTrackingPopupMenus());

  if (alternate_key_sequence)
    ASSERT_TRUE(window->SimulateOSKeyPress(ui::VKEY_DOWN, 0));
  else
    ASSERT_TRUE(window->SimulateOSKeyPress(ui::VKEY_RETURN, 0));

  // Wait until the popup menu actually opens.
  ASSERT_TRUE(browser->WaitForPopupMenuToOpen());

  // Press DOWN to select the first item, then RETURN to select it.
  ASSERT_TRUE(window->SimulateOSKeyPress(ui::VKEY_DOWN, 0));
  ASSERT_TRUE(window->SimulateOSKeyPress(ui::VKEY_RETURN, 0));

  // Wait for the new tab to appear.
  ASSERT_TRUE(browser->WaitForTabCountToBecome(2));

  // Make sure that the new tab index is 1.
  ASSERT_TRUE(browser->GetActiveTabIndex(&tab_index));
  ASSERT_EQ(1, tab_index);
}

// Disabled, http://crbug.com/62310.
TEST_F(KeyboardAccessTest, DISABLED_TestMenuKeyboardAccess) {
  TestMenuKeyboardAccess(false, 0);
}

// Disabled, http://crbug.com/62310.
TEST_F(KeyboardAccessTest, DISABLED_TestAltMenuKeyboardAccess) {
  TestMenuKeyboardAccess(true, 0);
}

// Flaky, http://crbug.com/62311.
TEST_F(KeyboardAccessTest, FLAKY_TestShiftAltMenuKeyboardAccess) {
  TestMenuKeyboardAccess(true, views::Event::EF_SHIFT_DOWN);
}

#if defined(OS_CHROMEOS)
// TODO(isherman): This test times out on ChromeOS.  We should merge it with
// BrowserKeyEventsTest.ReservedAccelerators, but just disable for now.
#define MAYBE_ReserveKeyboardAccelerators DISABLED_ReserveKeyboardAccelerators
#else
// Flaky, http://crbug.com/62311.
#define MAYBE_ReserveKeyboardAccelerators FLAKY_ReserveKeyboardAccelerators
#endif

// Test that JavaScript cannot intercept reserved keyboard accelerators like
// ctrl-t to open a new tab or ctrl-f4 to close a tab.
TEST_F(KeyboardAccessTest, MAYBE_ReserveKeyboardAccelerators) {
  const std::string kBadPage =
      "<html><script>"
      "document.onkeydown = function() {"
      "  event.preventDefault();"
      "  return false;"
      "}"
      "</script></html>";
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser);
  ASSERT_TRUE(browser->AppendTab(GURL("data:text/html," + kBadPage)));
  int tab_count = 0;
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(tab_count, 2);

  int active_tab = 0;
  ASSERT_TRUE(browser->GetActiveTabIndex(&active_tab));
  ASSERT_EQ(active_tab, 1);

  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window);
  ASSERT_TRUE(window->SimulateOSKeyPress(
      ui::VKEY_TAB, views::Event::EF_CONTROL_DOWN));
  ASSERT_TRUE(browser->WaitForTabToBecomeActive(
      0, TestTimeouts::action_max_timeout_ms()));

#if !defined(OS_MACOSX)  // see BrowserWindowCocoa::GetCommandId
  ASSERT_TRUE(browser->ActivateTab(1));
  ASSERT_TRUE(window->SimulateOSKeyPress(
      ui::VKEY_F4, views::Event::EF_CONTROL_DOWN));
  ASSERT_TRUE(browser->WaitForTabCountToBecome(1));
#endif
}

}  // namespace

#endif  // defined(TOOLKIT_VIEWS)
