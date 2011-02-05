// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_timeouts.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/automated_ui_tests/automated_ui_test_base.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "views/event.h"

AutomatedUITestBase::AutomatedUITestBase() {}

AutomatedUITestBase::~AutomatedUITestBase() {}

void AutomatedUITestBase::LogErrorMessage(const std::string& error) {
}

void AutomatedUITestBase::LogWarningMessage(const std::string& warning) {
}

void AutomatedUITestBase::LogInfoMessage(const std::string& info) {
}

void AutomatedUITestBase::SetUp() {
  UITest::SetUp();
  set_active_browser(automation()->GetBrowserWindow(0));
}

bool AutomatedUITestBase::BackButton() {
  return RunCommand(IDC_BACK);
}

bool AutomatedUITestBase::CloseActiveTab() {
  BrowserProxy* browser = active_browser();
  int tab_count;
  if (!browser->GetTabCount(&tab_count)) {
    LogErrorMessage("get_tab_count_failed");
    return false;
  }

  if (tab_count > 1) {
    return RunCommand(IDC_CLOSE_TAB);
  } else if (tab_count == 1) {
    // Synchronously close the window if it is not the last window.
    return CloseActiveWindow();
  } else {
    LogInfoMessage("invalid_tab_count");
    return false;
  }
}

bool AutomatedUITestBase::CloseActiveWindow() {
  int browser_windows_count = 0;
  if (!automation()->GetNormalBrowserWindowCount(&browser_windows_count))
    return false;
  // Avoid quitting the application by not closing the last window.
  if (browser_windows_count < 2)
    return false;
  bool application_closed;
  CloseBrowser(active_browser(), &application_closed);
  if (application_closed) {
    LogErrorMessage("Application closed unexpectedly.");
    return false;
  }
  scoped_refptr<BrowserProxy> browser(automation()->FindNormalBrowserWindow());
  if (!browser.get()) {
    LogErrorMessage("Can't find browser window.");
    return false;
  }
  set_active_browser(browser);
  return true;
}

bool AutomatedUITestBase::DuplicateTab() {
  return RunCommand(IDC_DUPLICATE_TAB);
}

bool AutomatedUITestBase::DragTabOut() {
  BrowserProxy* browser = active_browser();
  if (browser == NULL) {
    LogErrorMessage("browser_window_not_found");
    return false;
  }

  scoped_refptr<WindowProxy> window(
      GetAndActivateWindowForBrowser(browser));
  if (window.get() == NULL) {
    LogErrorMessage("active_window_not_found");
    return false;
  }

  int tab_count;
  if (!browser->GetTabCount(&tab_count)) {
    LogErrorMessage("get_tab_count_failed");
    return false;
  }

  if (tab_count < 2) {
    LogWarningMessage("not_enough_tabs_to_drag_out");
    return false;
  }

  int tab_index;
  if (!browser->GetActiveTabIndex(&tab_index)) {
    LogWarningMessage("no_active_tab");
    return false;
  }

  if (tab_index < 0) {
    LogInfoMessage("invalid_active_tab_index");
    return false;
  }

  gfx::Rect dragged_tab_bounds;
  if (!window->GetViewBounds(VIEW_ID_TAB_0 + tab_index,
                             &dragged_tab_bounds, false)) {
    LogWarningMessage("no_tab_view_found");
    return false;
  }

  gfx::Rect urlbar_bounds;
  if (!window->GetViewBounds(VIEW_ID_LOCATION_BAR,
                             &urlbar_bounds, false)) {
    LogWarningMessage("no_location_bar_found");
    return false;
  }

  // Click on the center of the tab, and drag it downwards.
  gfx::Point start;
  gfx::Point end;
  start.set_x(dragged_tab_bounds.x() + dragged_tab_bounds.width() / 2);
  start.set_y(dragged_tab_bounds.y() + dragged_tab_bounds.height() / 2);
  end.set_x(start.x());
  end.set_y(start.y() + 3 * urlbar_bounds.height());

  if (!browser->SimulateDrag(start, end, views::Event::EF_LEFT_BUTTON_DOWN,
                             false)) {
    LogWarningMessage("failed_to_simulate_drag");
    return false;
  }

  return true;
}

bool AutomatedUITestBase::DragActiveTab(bool drag_right) {
  BrowserProxy* browser = active_browser();
  if (browser == NULL) {
    LogErrorMessage("browser_window_not_found");
    return false;
  }

  scoped_refptr<WindowProxy> window(
      GetAndActivateWindowForBrowser(browser));
  if (window.get() == NULL) {
    LogErrorMessage("active_window_not_found");
    return false;
  }

  int tab_count;
  if (!browser->GetTabCount(&tab_count)) {
    LogErrorMessage("get_tab_count_failed");
    return false;
  }

  if (tab_count < 2) {
    LogWarningMessage("not_enough_tabs_to_drag_around");
    return false;
  }

  int tab_index;
  if (!browser->GetActiveTabIndex(&tab_index)) {
    LogWarningMessage("no_active_tab");
    return false;
  }

  gfx::Rect dragged_tab_bounds;
  if (!window->GetViewBounds(VIEW_ID_TAB_0 + tab_index,
                             &dragged_tab_bounds, false)) {
    LogWarningMessage("no_tab_view_found");
    return false;
  }

  // Click on the center of the tab, and drag it to the left or the right.
  gfx::Point dragged_tab_point = dragged_tab_bounds.CenterPoint();
  gfx::Point destination_point = dragged_tab_point;

  int new_tab_index;
  if (drag_right) {
    if (tab_index >= (tab_count - 1)) {
      LogInfoMessage("cant_drag_to_right");
      return false;
    }
    new_tab_index = tab_index + 1;
    destination_point.Offset(2 * dragged_tab_bounds.width() / 3, 0);
  } else {
    if (tab_index <= 0) {
      LogInfoMessage("cant_drag_to_left");
      return false;
    }
    new_tab_index = tab_index - 1;
    destination_point.Offset(-2 * dragged_tab_bounds.width() / 3, 0);
  }

  if (!browser->SimulateDrag(dragged_tab_point, destination_point,
                             views::Event::EF_LEFT_BUTTON_DOWN, false)) {
    LogWarningMessage("failed_to_simulate_drag");
    return false;
  }

  if (!browser->WaitForTabToBecomeActive(new_tab_index,
                                         TestTimeouts::action_timeout_ms())) {
    LogWarningMessage("failed_to_reindex_tab");
    return false;
  }

  return true;
}

bool AutomatedUITestBase::FindInPage() {
  if (!RunCommandAsync(IDC_FIND))
    return false;

  return WaitForFindWindowVisibilityChange(active_browser(), true);
}

bool AutomatedUITestBase::ForwardButton() {
  return RunCommand(IDC_FORWARD);
}

bool AutomatedUITestBase::GoOffTheRecord() {
  return RunCommand(IDC_NEW_INCOGNITO_WINDOW);
}

bool AutomatedUITestBase::Home() {
  return RunCommand(IDC_HOME);
}

bool AutomatedUITestBase::OpenAndActivateNewBrowserWindow(
    scoped_refptr<BrowserProxy>* previous_browser) {
  if (!automation()->OpenNewBrowserWindow(Browser::TYPE_NORMAL,
                                          true /* SW_SHOWNORMAL */)) {
    LogWarningMessage("failed_to_open_new_browser_window");
    return false;
  }
  int num_browser_windows;
  if (!automation()->GetBrowserWindowCount(&num_browser_windows)) {
    LogErrorMessage("failed_to_get_browser_window_count");
    return false;
  }
  // Get the most recently opened browser window and activate the tab
  // in order to activate this browser window.
  scoped_refptr<BrowserProxy> browser(
      automation()->GetBrowserWindow(num_browser_windows - 1));
  if (browser.get() == NULL) {
    LogErrorMessage("browser_window_not_found");
    return false;
  }
  if (!browser->ActivateTab(0)) {
    LogWarningMessage("failed_to_activate_tab");
    return false;
  }

  if (previous_browser) {
    DCHECK(previous_browser->get() == NULL);
    active_browser_.swap(*previous_browser);
  }

  active_browser_.swap(browser);
  return true;
}

bool AutomatedUITestBase::Navigate(const GURL& url) {
  scoped_refptr<TabProxy> tab(GetActiveTab());
  if (tab.get() == NULL) {
    LogErrorMessage("active_tab_not_found");
    return false;
  }
  AutomationMsg_NavigationResponseValues result = tab->NavigateToURL(url);
  if (result != AUTOMATION_MSG_NAVIGATION_SUCCESS) {
    LogErrorMessage("navigation_failed");
    return false;
  }

  return true;
}

bool AutomatedUITestBase::NewTab() {
  // Apply accelerator and wait for a new tab to open, if either
  // fails, return false. Apply Accelerator takes care of logging its failure.
  return RunCommand(IDC_NEW_TAB);
}

bool AutomatedUITestBase::ReloadPage() {
  return RunCommand(IDC_RELOAD);
}

bool AutomatedUITestBase::RestoreTab() {
  return RunCommand(IDC_RESTORE_TAB);
}

bool AutomatedUITestBase::SelectNextTab() {
  return RunCommand(IDC_SELECT_NEXT_TAB);
}

bool AutomatedUITestBase::SelectPreviousTab() {
  return RunCommand(IDC_SELECT_PREVIOUS_TAB);
}

bool AutomatedUITestBase::ShowBookmarkBar() {
  bool is_visible;
  bool is_animating;
  if (!active_browser()->GetBookmarkBarVisibility(&is_visible,
                                                  &is_animating)) {
    return false;
  }

  if (is_visible) {
    // If the bar is visible, then issuing the command again will toggle it.
    return true;
  }

  if (!RunCommandAsync(IDC_SHOW_BOOKMARK_BAR))
    return false;

  return WaitForBookmarkBarVisibilityChange(active_browser(), true);
}

bool AutomatedUITestBase::ShowDownloads() {
  return RunCommand(IDC_SHOW_DOWNLOADS);
}

bool AutomatedUITestBase::ShowHistory() {
  return RunCommand(IDC_SHOW_HISTORY);
}

bool AutomatedUITestBase::RunCommandAsync(int browser_command) {
  BrowserProxy* browser = active_browser();
  if (NULL == browser) {
    LogErrorMessage("browser_window_not_found");
    return false;
  }

  if (!browser->RunCommandAsync(browser_command)) {
    LogWarningMessage("failure_running_browser_command");
    return false;
  }
  return true;
}

bool AutomatedUITestBase::RunCommand(int browser_command) {
  BrowserProxy* browser = active_browser();
  if (NULL == browser) {
    LogErrorMessage("browser_window_not_found");
    return false;
  }

  if (!browser->RunCommand(browser_command)) {
    LogWarningMessage("failure_running_browser_command");
    return false;
  }
  return true;
}

scoped_refptr<TabProxy> AutomatedUITestBase::GetActiveTab() {
  BrowserProxy* browser = active_browser();
  if (browser == NULL) {
    LogErrorMessage("browser_window_not_found");
    return NULL;
  }

  return browser->GetActiveTab();
}

scoped_refptr<WindowProxy> AutomatedUITestBase::GetAndActivateWindowForBrowser(
    BrowserProxy* browser) {
  if (!browser->BringToFront()) {
    LogWarningMessage("failed_to_bring_window_to_front");
    return NULL;
  }

  return browser->GetWindow();
}
