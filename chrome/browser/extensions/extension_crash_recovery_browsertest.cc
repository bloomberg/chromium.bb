// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/balloon_host.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/result_codes.h"

class ExtensionCrashRecoveryTest : public ExtensionBrowserTest {
 protected:
  ExtensionService* GetExtensionService() {
    return browser()->profile()->GetExtensionService();
  }

  ExtensionProcessManager* GetExtensionProcessManager() {
    return browser()->profile()->GetExtensionProcessManager();
  }

  Balloon* GetNotificationDelegate(size_t index) {
    NotificationUIManager* manager =
        g_browser_process->notification_ui_manager();
    BalloonCollection::Balloons balloons =
        manager->balloon_collection()->GetActiveBalloons();
    return balloons.at(index);
  }

  void AcceptNotification(size_t index) {
    Balloon* balloon = GetNotificationDelegate(index);
    ASSERT_TRUE(balloon);
    balloon->OnClick();
    WaitForExtensionLoad();
  }

  void CancelNotification(size_t index) {
    Balloon* balloon = GetNotificationDelegate(index);
    NotificationUIManager* manager =
        g_browser_process->notification_ui_manager();
    ASSERT_TRUE(manager->CancelById(balloon->notification().notification_id()));
  }

  size_t CountBalloons() {
    NotificationUIManager* manager =
        g_browser_process->notification_ui_manager();
    BalloonCollection::Balloons balloons =
        manager->balloon_collection()->GetActiveBalloons();
    return balloons.size();
  }

  void CrashExtension(std::string extension_id) {
    const Extension* extension =
        GetExtensionService()->extensions()->GetByID(extension_id);
    ASSERT_TRUE(extension);
    ExtensionHost* extension_host = GetExtensionProcessManager()->
        GetBackgroundHostForExtension(extension_id);
    ASSERT_TRUE(extension_host);

    content::RenderProcessHost* extension_rph =
        extension_host->render_view_host()->process();
    base::KillProcess(extension_rph->GetHandle(), content::RESULT_CODE_KILLED,
                      false);
    ASSERT_TRUE(WaitForExtensionCrash(extension_id));
    ASSERT_FALSE(GetExtensionProcessManager()->
                 GetBackgroundHostForExtension(extension_id));
  }

  void CheckExtensionConsistency(std::string extension_id) {
    const Extension* extension =
        GetExtensionService()->extensions()->GetByID(extension_id);
    ASSERT_TRUE(extension);
    ExtensionHost* extension_host = GetExtensionProcessManager()->
        GetBackgroundHostForExtension(extension_id);
    ASSERT_TRUE(extension_host);
    ASSERT_TRUE(GetExtensionProcessManager()->HasExtensionHost(extension_host));
    ASSERT_TRUE(extension_host->IsRenderViewLive());
    extensions::ProcessMap* process_map =
        browser()->profile()->GetExtensionService()->process_map();
    ASSERT_TRUE(process_map->Contains(
        extension_id, extension_host->render_view_host()->process()->GetID()));
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

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, Basic) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());
  AcceptNotification(0);

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(first_extension_id_);
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, CloseAndReload) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);

  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());

  CancelNotification(0);
  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(first_extension_id_);
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, ReloadIndependently) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(first_extension_id_);

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);

  // The balloon should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, CountBalloons());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       ReloadIndependentlyChangeTabs) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  TabContents* original_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(original_tab);
  ASSERT_EQ(1U, CountBalloons());

  // Open a new tab, but the balloon will still be there.
  browser()->NewTab();
  TabContents* new_current_tab = browser()->GetSelectedTabContents();
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

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       ReloadIndependentlyNavigatePage) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);
  ASSERT_EQ(1U, CountBalloons());

  // Navigate to another page.
  ui_test_utils::NavigateToURL(browser(),
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                FilePath(FILE_PATH_LITERAL("title1.html"))));
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

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, MAYBE_ShutdownWhileCrashed) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, TwoExtensionsCrashFirst) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  AcceptNotification(0);

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(first_extension_id_);
  CheckExtensionConsistency(second_extension_id_);
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, TwoExtensionsCrashSecond) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(second_extension_id_);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  AcceptNotification(0);

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(first_extension_id_);
  CheckExtensionConsistency(second_extension_id_);
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
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
    AcceptNotification(0);
    CheckExtensionConsistency(first_extension_id_);
  }

  {
    SCOPED_TRACE("second balloon");
    AcceptNotification(0);
    CheckExtensionConsistency(first_extension_id_);
    CheckExtensionConsistency(second_extension_id_);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, TwoExtensionsOneByOne) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  LoadSecondExtension();
  CrashExtension(second_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  {
    SCOPED_TRACE("first balloon");
    AcceptNotification(0);
    CheckExtensionConsistency(first_extension_id_);
  }

  {
    SCOPED_TRACE("second balloon");
    AcceptNotification(0);
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
IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       MAYBE_TwoExtensionsShutdownWhileCrashed) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  LoadSecondExtension();
  CrashExtension(second_extension_id_);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
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
  AcceptNotification(1);
  CancelNotification(0);

  SCOPED_TRACE("balloons done");
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  CheckExtensionConsistency(second_extension_id_);
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
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
    TabContents* current_tab = browser()->GetSelectedTabContents();
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
    AcceptNotification(0);
    CheckExtensionConsistency(first_extension_id_);
    CheckExtensionConsistency(second_extension_id_);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, CrashAndUninstall) {
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
  MessageLoop::current()->RunAllPending();

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

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, MAYBE_CrashAndUnloadAll) {
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

// Test that when an extension with a background page that has a tab open
// crashes, the tab stays open, and reloading it reloads the extension.
// Regression test for issue 71629.
IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       ReloadTabsWithBackgroundPage) {
  TabStripModel* tab_strip = browser()->tabstrip_model();
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();

  // Open a tab extension.
  browser()->NewTab();
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
    ui_test_utils::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(
            &browser()->GetSelectedTabContentsWrapper()->controller()));
    browser()->Reload(CURRENT_TAB);
    observer.Wait();
  }
  // Extension should now be loaded.
  SCOPED_TRACE("after reloading the tab");
  CheckExtensionConsistency(first_extension_id_);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(0U, CountBalloons());
}
