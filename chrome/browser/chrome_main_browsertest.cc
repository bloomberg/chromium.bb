// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_util.h"

// These tests don't apply to the Mac version; see GetCommandLineForRelaunch
// for details.
#if !defined(OS_MACOSX)

class ChromeMainTest : public InProcessBrowserTest {
 public:
  ChromeMainTest() {}

  void Relaunch(const CommandLine& new_command_line) {
    base::LaunchProcess(new_command_line, base::LaunchOptions(), NULL);
  }
};

// Make sure that the second invocation creates a new window.
IN_PROC_BROWSER_TEST_F(ChromeMainTest, SecondLaunch) {
  ui_test_utils::BrowserAddedObserver observer;
  Relaunch(GetCommandLineForRelaunch());
  observer.WaitForSingleNewBrowser();
  ASSERT_EQ(2u, browser::GetBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ChromeMainTest, ReuseBrowserInstanceWhenOpeningFile) {
  FilePath test_file_path = ui_test_utils::GetTestFilePath(
      FilePath(), FilePath().AppendASCII("empty.html"));
  CommandLine new_command_line(GetCommandLineForRelaunch());
  new_command_line.AppendArgPath(test_file_path);
  ui_test_utils::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
  Relaunch(new_command_line);
  observer.Wait();

  GURL url = net::FilePathToFileURL(test_file_path);
  content::WebContents* tab = browser()->GetSelectedWebContents();
  ASSERT_EQ(url, tab->GetController().GetActiveEntry()->GetVirtualURL());
}


IN_PROC_BROWSER_TEST_F(ChromeMainTest, SecondLaunchWithIncognitoUrl) {
  // We should start with one normal window.
  ASSERT_EQ(1u, browser::GetTabbedBrowserCount(browser()->profile()));

  // Run with --incognito switch and an URL specified.
  FilePath test_file_path = ui_test_utils::GetTestFilePath(
      FilePath(), FilePath().AppendASCII("empty.html"));
  CommandLine new_command_line(GetCommandLineForRelaunch());
  new_command_line.AppendSwitch(switches::kIncognito);
  new_command_line.AppendArgPath(test_file_path);

  Relaunch(new_command_line);

  // There should be one normal and one incognito window now.
  ui_test_utils::BrowserAddedObserver observer;
  Relaunch(new_command_line);
  observer.WaitForSingleNewBrowser();
  ASSERT_EQ(2u, BrowserList::size());

  ASSERT_EQ(1u, browser::GetTabbedBrowserCount(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ChromeMainTest, SecondLaunchFromIncognitoWithNormalUrl) {
  // We should start with one normal window.
  ASSERT_EQ(1u, browser::GetTabbedBrowserCount(browser()->profile()));

  // Create an incognito window.
  browser()->NewIncognitoWindow();

  ASSERT_EQ(2u, BrowserList::size());
  ASSERT_EQ(1u, browser::GetTabbedBrowserCount(browser()->profile()));

  // Close the first window.
  Profile* profile = browser()->profile();
  ui_test_utils::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::NotificationService::AllSources());
  browser()->CloseWindow();
  observer.Wait();

  // There should only be the incognito window open now.
  ASSERT_EQ(1u, BrowserList::size());
  ASSERT_EQ(0u, browser::GetTabbedBrowserCount(profile));

  // Run with just an URL specified, no --incognito switch.
  FilePath test_file_path = ui_test_utils::GetTestFilePath(
      FilePath(), FilePath().AppendASCII("empty.html"));
  CommandLine new_command_line(GetCommandLineForRelaunch());
  new_command_line.AppendArgPath(test_file_path);
  ui_test_utils::WindowedNotificationObserver tab_observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
  Relaunch(new_command_line);
  tab_observer.Wait();

  // There should be one normal and one incognito window now.
  ASSERT_EQ(2u, BrowserList::size());
  ASSERT_EQ(1u, browser::GetTabbedBrowserCount(profile));
}

#endif  // !OS_MACOSX
