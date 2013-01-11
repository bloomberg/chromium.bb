// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/reliability/automated_ui_test_base.h"

#include "base/test/test_timeouts.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "ui/base/events/event_constants.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

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
  for (int i = 0; i < browser_windows_count - 1; ++i) {
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(i));
    Browser::Type type;
    if (browser->GetType(&type) && type == Browser::TYPE_TABBED) {
      set_active_browser(browser);
      return true;
    }
  }

  LogErrorMessage("Can't find browser window.");
  return false;
}

bool AutomatedUITestBase::DuplicateTab() {
  return RunCommand(IDC_DUPLICATE_TAB);
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
  if (!automation()->OpenNewBrowserWindow(Browser::TYPE_TABBED,
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
