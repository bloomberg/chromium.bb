// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/test/test_suite.h"
#include "chrome/browser/ui/browser.h"
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
#include "content/test/test_launcher.h"
#include "net/base/net_util.h"

// These tests don't apply to the Mac version; see
// LaunchAnotherBrowserBlockUntilClosed for details.
#if !defined(OS_MACOSX)

class ChromeMainTest : public InProcessBrowserTest {
 public:
  ChromeMainTest()
      : InProcessBrowserTest(),
        new_command_line_(CommandLine::ForCurrentProcess()->GetProgram()) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    CommandLine::SwitchMap switches =
        CommandLine::ForCurrentProcess()->GetSwitches();
    switches.erase(switches::kUserDataDir);
    switches.erase(test_launcher::kGTestFilterFlag);

    for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
          iter != switches.end(); ++iter) {
      new_command_line_.AppendSwitchNative((*iter).first, (*iter).second);
    }

    FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    new_command_line_.AppendSwitchPath(switches::kUserDataDir, user_data_dir);

    new_command_line_.AppendSwitchASCII(
        test_launcher::kGTestFilterFlag, test_launcher::kEmptyTestName);
    new_command_line_.AppendSwitch(TestSuite::kSilent);
  }

  void Relaunch() {
    base::LaunchProcess(new_command_line_, base::LaunchOptions(), NULL);
  }

 protected:
  CommandLine new_command_line_;
};

// Make sure that the second invocation creates a new window.
IN_PROC_BROWSER_TEST_F(ChromeMainTest, SecondLaunch) {
  ui_test_utils::BrowserAddedObserver observer;
  Relaunch();
  observer.WaitForSingleNewBrowser();
  ASSERT_EQ(BrowserList::GetBrowserCount(browser()->profile()), 2u);
}

IN_PROC_BROWSER_TEST_F(ChromeMainTest, ReuseBrowserInstanceWhenOpeningFile) {
  FilePath test_file_path = ui_test_utils::GetTestFilePath(
      FilePath(), FilePath().AppendASCII("empty.html"));
  new_command_line_.AppendArgPath(test_file_path);
  ui_test_utils::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
  Relaunch();
  observer.Wait();

  GURL url = net::FilePathToFileURL(test_file_path);
  content::WebContents* tab = browser()->GetSelectedWebContents();
  ASSERT_EQ(url, tab->GetController().GetActiveEntry()->GetVirtualURL());
}


IN_PROC_BROWSER_TEST_F(ChromeMainTest, SecondLaunchWithIncognitoUrl) {
  // We should start with one normal window.
  ASSERT_EQ(1u,
            BrowserList::GetBrowserCountForType(browser()->profile(), true));

  // Run with --incognito switch and an URL specified.
  FilePath test_file_path = ui_test_utils::GetTestFilePath(
      FilePath(), FilePath().AppendASCII("empty.html"));
  new_command_line_.AppendSwitch(switches::kIncognito);
  new_command_line_.AppendArgPath(test_file_path);

  Relaunch();

  // There should be one normal and one incognito window now.
  ui_test_utils::BrowserAddedObserver observer;
  Relaunch();
  observer.WaitForSingleNewBrowser();
  ASSERT_EQ(2u, BrowserList::size());

  ASSERT_EQ(1u,
            BrowserList::GetBrowserCountForType(browser()->profile(), true));
}

IN_PROC_BROWSER_TEST_F(ChromeMainTest, SecondLaunchFromIncognitoWithNormalUrl) {
  // We should start with one normal window.
  ASSERT_EQ(1u,
            BrowserList::GetBrowserCountForType(browser()->profile(), true));

  // Create an incognito window.
  browser()->NewIncognitoWindow();

  ASSERT_EQ(2u, BrowserList::size());
  ASSERT_EQ(1u,
            BrowserList::GetBrowserCountForType(browser()->profile(), true));

  // Close the first window.
  Profile* profile = browser()->profile();
  ui_test_utils::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::NotificationService::AllSources());
  browser()->CloseWindow();
  observer.Wait();

  // There should only be the incognito window open now.
  ASSERT_EQ(1u, BrowserList::size());
  ASSERT_EQ(0u, BrowserList::GetBrowserCountForType(profile, true));

  // Run with just an URL specified, no --incognito switch.
  FilePath test_file_path = ui_test_utils::GetTestFilePath(
      FilePath(), FilePath().AppendASCII("empty.html"));
  new_command_line_.AppendArgPath(test_file_path);
  ui_test_utils::WindowedNotificationObserver tab_observer(
        chrome::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
  Relaunch();
  tab_observer.Wait();

  // There should be one normal and one incognito window now.
  ASSERT_EQ(2u, BrowserList::size());
  ASSERT_EQ(1u, BrowserList::GetBrowserCountForType(profile, true));
}

#endif  // !OS_MACOSX
