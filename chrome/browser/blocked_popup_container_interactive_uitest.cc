// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/l10n_util.h"
#include "base/file_path.h"
#include "base/gfx/rect.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/view_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/automation/automation_constants.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "views/event.h"

class BlockedPopupContainerInteractiveTest : public UITest {
 protected:
  BlockedPopupContainerInteractiveTest() {
    show_window_ = true;
  }

  virtual void SetUp() {
    UITest::SetUp();

    browser_ = automation()->GetBrowserWindow(0);
    ASSERT_TRUE(browser_.get());

    window_ = browser_->GetWindow();
    ASSERT_TRUE(window_.get());

    tab_ = browser_->GetTab(0);
    ASSERT_TRUE(tab_.get());
  }

  void NavigateMainTabTo(const std::string& file_name) {
    FilePath filename(test_data_directory_);
    filename = filename.AppendASCII("constrained_files");
    filename = filename.AppendASCII(file_name);
    ASSERT_TRUE(tab_->NavigateToURL(net::FilePathToFileURL(filename)));
  }

  void SimulateClickInCenterOf(const scoped_refptr<WindowProxy>& window) {
    gfx::Rect tab_view_bounds;
    ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER,
                                      &tab_view_bounds, true));

    // Simulate a click of the actual link to force user_gesture to be
    // true; if we don't, the resulting popup will be constrained, which
    // isn't what we want to test.
    ASSERT_TRUE(window->SimulateOSClick(tab_view_bounds.CenterPoint(),
                                        views::Event::EF_LEFT_BUTTON_DOWN));
  }

  scoped_refptr<BrowserProxy> browser_;
  scoped_refptr<WindowProxy> window_;
  scoped_refptr<TabProxy> tab_;
};

TEST_F(BlockedPopupContainerInteractiveTest, TestOpenAndResizeTo) {
  NavigateMainTabTo("constrained_window_onload_resizeto.html");
  SimulateClickInCenterOf(window_);

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, 1000));

  scoped_refptr<BrowserProxy> popup_browser(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(popup_browser != NULL);
  scoped_refptr<WindowProxy> popup_window(popup_browser->GetWindow());
  ASSERT_TRUE(popup_window != NULL);

  // Make sure we were created with the correct width and height.
  gfx::Rect rect;
  bool is_timeout = false;
  ASSERT_TRUE(popup_window->GetViewBoundsWithTimeout(
                  VIEW_ID_TAB_CONTAINER, &rect, false, 1000, &is_timeout));
  ASSERT_FALSE(is_timeout);
  EXPECT_EQ(300, rect.width());
  EXPECT_EQ(320, rect.height());

#if defined(OS_LINUX)
  // It seems we have to wait a little bit for the widgets to spin up before
  // we can start clicking on them.
  PlatformThread::Sleep(500);
#endif

  SimulateClickInCenterOf(popup_window);

  // No idea how to wait here other then sleeping. This timeout used to be
  // lower, then we started hitting it before it was done. :(
  PlatformThread::Sleep(5000);

  // The actual content will be LESS than (200, 200) because resizeTo
  // deals with a window's outer{Width,Height} instead of its
  // inner{Width,Height}.
  is_timeout = false;
  ASSERT_TRUE(popup_window->GetViewBoundsWithTimeout(
                 VIEW_ID_TAB_CONTAINER, &rect, false, 1000, &is_timeout));
  ASSERT_FALSE(is_timeout);
  EXPECT_LT(rect.height(), 200);
#if defined(OS_LINUX)
  // On Linux we may run in an environment where there is no window frame. In
  // this case our width might be exactly 200. The height will still be less
  // because we have to show the location bar.
  EXPECT_LE(rect.width(), 200);
#else
  EXPECT_LT(rect.width(), 200);
#endif
}

// Helper function used to get the number of blocked popups out of the window
// title.
bool ParseCountOutOfTitle(const std::wstring& title, int* output) {
  // Since we will be reading the number of popup windows open by grabbing the
  // number out of the window title, and that format string is localized, we
  // need to find out the offset into that string.
  const wchar_t* placeholder = L"XXXX";
  size_t offset =
      l10n_util::GetStringF(IDS_POPUPS_BLOCKED_COUNT, placeholder).
      find(placeholder);

  std::wstring number;
  while (offset < title.size() && iswdigit(title[offset])) {
    number += title[offset];
    offset++;
  }

  return StringToInt(WideToUTF16(number), output);
}

// Tests that in the window.open() equivalent of a fork bomb, we stop building
// windows.
// Flaky, http://crbug.com/26692.
TEST_F(BlockedPopupContainerInteractiveTest, FLAKY_DontSpawnEndlessPopups) {
  NavigateMainTabTo("infinite_popups.html");
  SimulateClickInCenterOf(window_);

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, 1000));

  scoped_refptr<BrowserProxy> popup_browser(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(popup_browser.get());
  scoped_refptr<TabProxy> popup_tab(popup_browser->GetTab(0));
  ASSERT_TRUE(popup_tab.get());

  // And now we spin, waiting to make sure that we don't spawn popup
  // windows endlessly. The current limit is 25, so allowing for possible race
  // conditions and one off errors, don't break out until we go over 30 popup
  // windows (in which case we are bork bork bork).
  const int kMaxPopupWindows = 30;

  int popup_window_count = 0;
  int new_popup_window_count = 0;
  int times_slept = 0;
  bool continuing = true;
  while (continuing && popup_window_count < kMaxPopupWindows) {
    ASSERT_TRUE(popup_tab->GetBlockedPopupCount(&new_popup_window_count));
    if (new_popup_window_count == popup_window_count) {
      if (times_slept == 10) {
        continuing = false;
      } else {
        // Nothing intereseting is going on wait it out.
        PlatformThread::Sleep(automation::kSleepTime);
        times_slept++;
      }
    } else {
      times_slept = 0;
    }

    EXPECT_GE(new_popup_window_count, popup_window_count);
    EXPECT_LE(new_popup_window_count, kMaxPopupWindows);
    popup_window_count = new_popup_window_count;
  }
}

// Make sure that we refuse to close windows when a constrained popup is
// displayed.
TEST_F(BlockedPopupContainerInteractiveTest, FLAKY_WindowOpenWindowClosePopup) {
  NavigateMainTabTo("openclose_main.html");
  SimulateClickInCenterOf(window_);

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, 5000));

  // Make sure we have a blocked popup notification
  scoped_refptr<BrowserProxy> popup_browser(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(popup_browser.get());
  scoped_refptr<WindowProxy> popup_window(popup_browser->GetWindow());
  ASSERT_TRUE(popup_window.get());
  scoped_refptr<TabProxy> popup_tab(popup_browser->GetTab(0));
  ASSERT_TRUE(popup_tab.get());
  ASSERT_TRUE(popup_tab->WaitForBlockedPopupCountToChangeTo(1, 1000));

  // Ensure we didn't close the first popup window.
  ASSERT_FALSE(automation()->WaitForWindowCountToBecome(1, 3000));
}

TEST_F(BlockedPopupContainerInteractiveTest, BlockAlertFromBlockedPopup) {
  NavigateMainTabTo("block_alert.html");

  // Wait for there to be an app modal dialog (and fail if it's shown).
  ASSERT_FALSE(automation()->WaitForAppModalDialog(4000));

  // Ensure one browser window.
  int browser_window_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&browser_window_count));
  ASSERT_EQ(1, browser_window_count);

  // Ensure one blocked popup window: the popup didn't escape.
  int popup_count = 0;
  ASSERT_TRUE(tab_->GetBlockedPopupCount(&popup_count));
  ASSERT_EQ(1, popup_count);
}

TEST_F(BlockedPopupContainerInteractiveTest, ShowAlertFromNormalPopup) {
  NavigateMainTabTo("show_alert.html");
  SimulateClickInCenterOf(window_);

  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, 5000));

  scoped_refptr<BrowserProxy> popup_browser(automation()->GetBrowserWindow(1));
  ASSERT_TRUE(popup_browser.get());
  scoped_refptr<WindowProxy> popup_window(popup_browser->GetWindow());
  ASSERT_TRUE(popup_window.get());
  scoped_refptr<TabProxy> popup_tab(popup_browser->GetTab(0));
  ASSERT_TRUE(popup_tab.get());

#if defined(OS_LINUX)
  // It seems we have to wait a little bit for the widgets to spin up before
  // we can start clicking on them.
  PlatformThread::Sleep(500);
#endif

  SimulateClickInCenterOf(popup_window);

  // Wait for there to be an app modal dialog.
  ASSERT_TRUE(automation()->WaitForAppModalDialog(5000));
}

// Make sure that window focus works while creating a popup window so that we
// don't
TEST_F(BlockedPopupContainerInteractiveTest, DontBreakOnBlur) {
  NavigateMainTabTo("window_blur_test.html");
  SimulateClickInCenterOf(window_);

  // Wait for the popup window to open.
  ASSERT_TRUE(automation()->WaitForWindowCountToBecome(2, 1000));

  // We popup shouldn't be closed by the onblur handler.
  ASSERT_FALSE(automation()->WaitForWindowCountToBecome(1, 1500));
}

// Tests that tab related keyboard accelerators are reserved by the app.

class BrowserInteractiveTest : public UITest {
};

TEST_F(BrowserInteractiveTest, ReserveKeyboardAccelerators) {
  const std::string kBadPage =
      "<html><script>"
      "document.onkeydown = function() {"
      "  event.preventDefault();"
      "  return false;"
      "}"
      "</script></html>";
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  browser->AppendTab(GURL("data:text/html," + kBadPage));
  int tab_count = 0;
  ASSERT_TRUE(browser->GetTabCount(&tab_count));
  ASSERT_EQ(tab_count, 2);

  int active_tab = 0;
  ASSERT_TRUE(browser->GetActiveTabIndex(&active_tab));
  ASSERT_EQ(active_tab, 1);

  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window->SimulateOSKeyPress(
      base::VKEY_TAB, views::Event::EF_CONTROL_DOWN));
  ASSERT_TRUE(browser->WaitForTabToBecomeActive(0, action_max_timeout_ms()));

#if !defined(OS_MACOSX)  // see BrowserWindowCocoa::GetCommandId
  ASSERT_TRUE(browser->ActivateTab(1));
  ASSERT_TRUE(window->SimulateOSKeyPress(
      base::VKEY_W, views::Event::EF_CONTROL_DOWN));
  ASSERT_TRUE(browser->WaitForTabCountToBecome(1, action_max_timeout_ms()));
#endif
}
