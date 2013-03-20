// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/balloon_host.h"
#include "chrome/browser/notifications/balloon_notification_ui_manager.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"

#if defined(ENABLE_MESSAGE_CENTER)
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#endif

using content::NavigationController;
using content::WebContents;
using extensions::Extension;

// Tests are timing out waiting for extension to crash.
// http://crbug.com/174705
#if defined(OS_MACOSX) || defined(USE_AURA)
#define MAYBE_ExtensionCrashRecoveryTest DISABLED_ExtensionCrashRecoveryTest
#else
#define MAYBE_ExtensionCrashRecoveryTest ExtensionCrashRecoveryTest
#endif  // defined(OS_MACOSX) || defined(USE_AURA)

class ExtensionCrashRecoveryTestBase : public ExtensionBrowserTest {
 protected:
  virtual void AcceptNotification(size_t index) = 0;
  virtual void CancelNotification(size_t index) = 0;
  virtual size_t CountBalloons() = 0;

  ExtensionService* GetExtensionService() {
    return browser()->profile()->GetExtensionService();
  }

  ExtensionProcessManager* GetExtensionProcessManager() {
    return extensions::ExtensionSystem::Get(browser()->profile())->
        process_manager();
  }

  void CrashExtension(std::string extension_id) {
    const Extension* extension =
        GetExtensionService()->GetExtensionById(extension_id, false);
    ASSERT_TRUE(extension);
    extensions::ExtensionHost* extension_host = GetExtensionProcessManager()->
        GetBackgroundHostForExtension(extension_id);
    ASSERT_TRUE(extension_host);

    base::KillProcess(extension_host->render_process_host()->GetHandle(),
                      content::RESULT_CODE_KILLED, false);
    ASSERT_TRUE(WaitForExtensionCrash(extension_id));
    ASSERT_FALSE(GetExtensionProcessManager()->
                 GetBackgroundHostForExtension(extension_id));
  }

  void CheckExtensionConsistency(std::string extension_id) {
    const Extension* extension =
        GetExtensionService()->extensions()->GetByID(extension_id);
    ASSERT_TRUE(extension);
    extensions::ExtensionHost* extension_host = GetExtensionProcessManager()->
        GetBackgroundHostForExtension(extension_id);
    ASSERT_TRUE(extension_host);
    ExtensionProcessManager::ViewSet all_views =
        GetExtensionProcessManager()->GetAllViews();
    ExtensionProcessManager::ViewSet::const_iterator it =
        all_views.find(extension_host->host_contents()->GetRenderViewHost());
    ASSERT_FALSE(it == all_views.end());
    ASSERT_TRUE(extension_host->IsRenderViewLive());
    extensions::ProcessMap* process_map =
        browser()->profile()->GetExtensionService()->process_map();
    ASSERT_TRUE(process_map->Contains(
        extension_id,
        extension_host->render_view_host()->GetProcess()->GetID()));
  }

  void LoadTestExtension() {
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
    const Extension* extension = LoadExtension(
        test_data_dir_.AppendASCII("common").AppendASCII("background_page"));
    ASSERT_TRUE(extension);
    first_extension_id_ = extension->id();
    CheckExtensionConsistency(first_extension_id_);
  }

  void LoadSecondExtension() {
    const Extension* extension = LoadExtension(
        test_data_dir_.AppendASCII("install").AppendASCII("install"));
    ASSERT_TRUE(extension);
    second_extension_id_ = extension->id();
    CheckExtensionConsistency(second_extension_id_);
  }

  std::string first_extension_id_;
  std::string second_extension_id_;

};

// TODO(rsesek): Implement and enable these tests. http://crbug.com/179904
#if defined(ENABLE_MESSAGE_CENTER) && !defined(OS_MACOSX)

class MessageCenterExtensionCrashRecoveryTest
    : public ExtensionCrashRecoveryTestBase {
 protected:
  virtual void AcceptNotification(size_t index) OVERRIDE {
    message_center::MessageCenter* message_center =
        message_center::MessageCenter::Get();
    ASSERT_GT(message_center->NotificationCount(), index);
    message_center::NotificationList::Notifications::reverse_iterator it =
        message_center->notification_list()->GetNotifications().rbegin();
    for (size_t i=0; i < index; ++i)
      it++;
    std::string id = (*it)->id();
    message_center->OnClicked(id);
    WaitForExtensionLoad();
  }

  virtual void CancelNotification(size_t index) OVERRIDE {
    message_center::MessageCenter* message_center =
        message_center::MessageCenter::Get();
    ASSERT_GT(message_center->NotificationCount(), index);
    message_center::NotificationList::Notifications::reverse_iterator it =
        message_center->notification_list()->GetNotifications().rbegin();
    for (size_t i=0; i < index; i++) { it++; }
    ASSERT_TRUE(
        g_browser_process->notification_ui_manager()->CancelById((*it)->id()));
  }

  virtual size_t CountBalloons() OVERRIDE {
    message_center::MessageCenter* message_center =
        message_center::MessageCenter::Get();
    return message_center->NotificationCount();
  }
};

typedef MessageCenterExtensionCrashRecoveryTest
    MAYBE_ExtensionCrashRecoveryTest;

#else  // defined(ENABLED_MESSAGE_CENTER)

class BalloonExtensionCrashRecoveryTest
    : public ExtensionCrashRecoveryTestBase {
 protected:
  virtual void AcceptNotification(size_t index) OVERRIDE {
    Balloon* balloon = GetNotificationDelegate(index);
    ASSERT_TRUE(balloon);
    balloon->OnClick();
    WaitForExtensionLoad();
  }

  virtual void CancelNotification(size_t index) OVERRIDE {
    Balloon* balloon = GetNotificationDelegate(index);
    ASSERT_TRUE(balloon);
    std::string id = balloon->notification().notification_id();
    ASSERT_TRUE(g_browser_process->notification_ui_manager()->CancelById(id));
  }

  virtual size_t CountBalloons() OVERRIDE {
    BalloonNotificationUIManager* manager =
        BalloonNotificationUIManager::GetInstanceForTesting();
    BalloonCollection::Balloons balloons =
        manager->balloon_collection()->GetActiveBalloons();
    return balloons.size();
  }
 private:
  Balloon* GetNotificationDelegate(size_t index) {
    BalloonNotificationUIManager* manager =
        BalloonNotificationUIManager::GetInstanceForTesting();
    BalloonCollection::Balloons balloons =
        manager->balloon_collection()->GetActiveBalloons();
    return index < balloons.size() ? balloons.at(index) : NULL;
  }
};

typedef BalloonExtensionCrashRecoveryTest MAYBE_ExtensionCrashRecoveryTest;
#endif  // defined(ENABLE_MESSAGE_CENTER)

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest, Basic) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());
  ASSERT_NO_FATAL_FAILURE(AcceptNotification(0));

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(first_extension_id_);
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest, CloseAndReload) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);

  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());

  ASSERT_NO_FATAL_FAILURE(CancelNotification(0));
  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(first_extension_id_);
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest, ReloadIndependently) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(first_extension_id_);

  WebContents* current_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(current_tab);

  // The balloon should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, CountBalloons());
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       ReloadIndependentlyChangeTabs) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  WebContents* original_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(original_tab);
  ASSERT_EQ(1U, CountBalloons());

  // Open a new tab, but the balloon will still be there.
  chrome::NewTab(browser());
  WebContents* new_current_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_current_tab);
  ASSERT_NE(new_current_tab, original_tab);
  ASSERT_EQ(1U, CountBalloons());

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(first_extension_id_);

  // The balloon should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, CountBalloons());
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       DISABLED_ReloadIndependentlyNavigatePage) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  WebContents* current_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(current_tab);
  ASSERT_EQ(1U, CountBalloons());

  // Navigate to another page.
  ui_test_utils::NavigateToURL(
      browser(), ui_test_utils::GetTestUrl(
                     base::FilePath(base::FilePath::kCurrentDirectory),
                     base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  ASSERT_EQ(1U, CountBalloons());

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(first_extension_id_);

  // The balloon should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, CountBalloons());
}

// Make sure that when we don't do anything about the crashed extension
// and close the browser, it doesn't crash. The browser is closed implicitly
// at the end of each browser test.
//
// http://crbug.com/84719
#if defined(OS_LINUX)
#define MAYBE_ShutdownWhileCrashed DISABLED_ShutdownWhileCrashed
#else
#define MAYBE_ShutdownWhileCrashed ShutdownWhileCrashed
#endif  // defined(OS_LINUX)

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       MAYBE_ShutdownWhileCrashed) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       TwoExtensionsCrashFirst) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_NO_FATAL_FAILURE(AcceptNotification(0));

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(first_extension_id_);
  CheckExtensionConsistency(second_extension_id_);
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       TwoExtensionsCrashSecond) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(second_extension_id_);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_NO_FATAL_FAILURE(AcceptNotification(0));

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(first_extension_id_);
  CheckExtensionConsistency(second_extension_id_);
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       TwoExtensionsCrashBothAtOnce) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());
  CrashExtension(second_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 2,
            GetExtensionService()->terminated_extensions()->size());

  {
    SCOPED_TRACE("first balloon");
    ASSERT_NO_FATAL_FAILURE(AcceptNotification(0));
    CheckExtensionConsistency(first_extension_id_);
  }

  {
    SCOPED_TRACE("second balloon");
    ASSERT_NO_FATAL_FAILURE(AcceptNotification(0));
    CheckExtensionConsistency(first_extension_id_);
    CheckExtensionConsistency(second_extension_id_);
  }
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       TwoExtensionsOneByOne) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  LoadSecondExtension();
  CrashExtension(second_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  {
    SCOPED_TRACE("first balloon");
    ASSERT_NO_FATAL_FAILURE(AcceptNotification(0));
    CheckExtensionConsistency(first_extension_id_);
  }

  {
    SCOPED_TRACE("second balloon");
    ASSERT_NO_FATAL_FAILURE(AcceptNotification(0));
    CheckExtensionConsistency(first_extension_id_);
    CheckExtensionConsistency(second_extension_id_);
  }
}

// http://crbug.com/84719
#if defined(OS_LINUX)
#define MAYBE_TwoExtensionsShutdownWhileCrashed \
    DISABLED_TwoExtensionsShutdownWhileCrashed
#else
#define MAYBE_TwoExtensionsShutdownWhileCrashed \
    TwoExtensionsShutdownWhileCrashed
#endif  // defined(OS_LINUX)

// Make sure that when we don't do anything about the crashed extensions
// and close the browser, it doesn't crash. The browser is closed implicitly
// at the end of each browser test.
IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       MAYBE_TwoExtensionsShutdownWhileCrashed) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  LoadSecondExtension();
  CrashExtension(second_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       TwoExtensionsIgnoreFirst) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  CrashExtension(second_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  // Accept notification 1 before canceling notification 0.
  // Otherwise, on Linux and Windows, there is a race here, in which
  // canceled notifications do not immediately go away.
  ASSERT_NO_FATAL_FAILURE(AcceptNotification(1));
  ASSERT_NO_FATAL_FAILURE(CancelNotification(0));

  SCOPED_TRACE("balloons done");
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  CheckExtensionConsistency(second_extension_id_);
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       TwoExtensionsReloadIndependently) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  CrashExtension(second_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  {
    SCOPED_TRACE("first: reload");
    WebContents* current_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(current_tab);
    // At the beginning we should have one balloon displayed for each extension.
    ASSERT_EQ(2U, CountBalloons());
    ReloadExtension(first_extension_id_);
    // One of the balloons should hide after the extension is reloaded.
    ASSERT_EQ(1U, CountBalloons());
    CheckExtensionConsistency(first_extension_id_);
  }

  {
    SCOPED_TRACE("second: balloon");
    ASSERT_NO_FATAL_FAILURE(AcceptNotification(0));
    CheckExtensionConsistency(first_extension_id_);
    CheckExtensionConsistency(second_extension_id_);
  }
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest, CrashAndUninstall) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());

  ASSERT_EQ(1U, CountBalloons());
  UninstallExtension(first_extension_id_);
  MessageLoop::current()->RunUntilIdle();

  SCOPED_TRACE("after uninstalling");
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
  ASSERT_EQ(0U, CountBalloons());
}

// http://crbug.com/84719
#if defined(OS_LINUX)
#define MAYBE_CrashAndUnloadAll DISABLED_CrashAndUnloadAll
#else
#define MAYBE_CrashAndUnloadAll CrashAndUnloadAll
#endif  // defined(OS_LINUX)

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       MAYBE_CrashAndUnloadAll) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());

  GetExtensionService()->UnloadAllExtensions();
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
}

// Disabled on aura as flakey: http://crbug.com/169622
#if defined(USE_AURA)
#define MAYBE_ReloadTabsWithBackgroundPage DISABLED_ReloadTabsWithBackgroundPage
#else
#define MAYBE_ReloadTabsWithBackgroundPage ReloadTabsWithBackgroundPage
#endif  // defined(OS_LINUX)

// Test that when an extension with a background page that has a tab open
// crashes, the tab stays open, and reloading it reloads the extension.
// Regression test for issue 71629.
IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       MAYBE_ReloadTabsWithBackgroundPage) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();

  // Open a tab extension.
  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://" + first_extension_id_ + "/background.html"));

  const int tabs_before = tab_strip->count();
  CrashExtension(first_extension_id_);

  // Tab should still be open, and extension should be crashed.
  EXPECT_EQ(tabs_before, tab_strip->count());
  EXPECT_EQ(size_before, GetExtensionService()->extensions()->size());
  EXPECT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(
            &browser()->tab_strip_model()->GetActiveWebContents()->
                GetController()));
    chrome::Reload(browser(), CURRENT_TAB);
    observer.Wait();
  }
  // Extension should now be loaded.
  SCOPED_TRACE("after reloading the tab");
  CheckExtensionConsistency(first_extension_id_);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(0U, CountBalloons());
}
