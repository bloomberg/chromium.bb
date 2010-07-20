// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/keyboard_codes.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"

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
  // This test depends on the fact that there are two menus and that
  // New Tab is the first item in the app menu. If the menus change,
  // this test will need to be changed to reflect that.
  //
  // If alternate_key_sequence is true, use "Alt" instead of "F10" to
  // open the menu bar, and "Down" instead of "Enter" to open a menu.
  void TestMenuKeyboardAccess(bool alternate_key_sequence);

  DISALLOW_COPY_AND_ASSIGN(KeyboardAccessTest);
};

void KeyboardAccessTest::TestMenuKeyboardAccess(bool alternate_key_sequence) {
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

  base::KeyboardCode menu_key =
      alternate_key_sequence ? base::VKEY_MENU : base::VKEY_F10;
#if defined(OS_CHROMEOS)
  // Chrome OS has different function key accelerators, so we always use the
  // menu key there.
  menu_key = base::VKEY_MENU;
#endif
  ASSERT_TRUE(window->SimulateOSKeyPress(menu_key, 0));

  int new_view_id = -1;
  ASSERT_TRUE(window->WaitForFocusedViewIDToChange(
      original_view_id, &new_view_id));

  ASSERT_TRUE(browser->StartTrackingPopupMenus());

  if (alternate_key_sequence)
    ASSERT_TRUE(window->SimulateOSKeyPress(base::VKEY_DOWN, 0));
  else
    ASSERT_TRUE(window->SimulateOSKeyPress(base::VKEY_RETURN, 0));

  // Wait until the popup menu actually opens.
  ASSERT_TRUE(browser->WaitForPopupMenuToOpen());

  // Press DOWN to select the first item, then RETURN to select it.
  ASSERT_TRUE(window->SimulateOSKeyPress(base::VKEY_DOWN, 0));
  ASSERT_TRUE(window->SimulateOSKeyPress(base::VKEY_RETURN, 0));

  // Wait for the new tab to appear.
  ASSERT_TRUE(browser->WaitForTabCountToBecome(2, action_timeout_ms()));

  // Make sure that the new tab index is 1.
  ASSERT_TRUE(browser->GetActiveTabIndex(&tab_index));
  ASSERT_EQ(1, tab_index);
}

TEST_F(KeyboardAccessTest, TestMenuKeyboardAccess) {
  TestMenuKeyboardAccess(false);
}

TEST_F(KeyboardAccessTest, TestAltMenuKeyboardAccess) {
  TestMenuKeyboardAccess(true);
}

}  // namespace

#endif  // defined(TOOLKIT_VIEWS)
