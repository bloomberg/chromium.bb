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
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/result_codes.h"

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
    manager->CancelById(balloon->notification().notification_id());
  }

  size_t CountBalloons() {
    NotificationUIManager* manager =
        g_browser_process->notification_ui_manager();
    BalloonCollection::Balloons balloons =
        manager->balloon_collection()->GetActiveBalloons();
    return balloons.size();
  }

  void CrashExtension(size_t index) {
    ASSERT_LT(index, GetExtensionService()->extensions()->size());
    const Extension* extension =
        GetExtensionService()->extensions()->at(index);
    ASSERT_TRUE(extension);
    std::string extension_id(extension->id());
    ExtensionHost* extension_host =
        GetExtensionProcessManager()->GetBackgroundHostForExtension(extension);
    ASSERT_TRUE(extension_host);

    RenderProcessHost* extension_rph =
        extension_host->render_view_host()->process();
    base::KillProcess(extension_rph->GetHandle(), ResultCodes::KILLED, false);
    ASSERT_TRUE(WaitForExtensionCrash(extension_id));
    ASSERT_FALSE(
        GetExtensionProcessManager()->GetBackgroundHostForExtension(extension));
  }

  void CheckExtensionConsistency(size_t index) {
    ASSERT_LT(index, GetExtensionService()->extensions()->size());
    const Extension* extension =
        GetExtensionService()->extensions()->at(index);
    ASSERT_TRUE(extension);
    ExtensionHost* extension_host =
        GetExtensionProcessManager()->GetBackgroundHostForExtension(extension);
    ASSERT_TRUE(extension_host);
    ASSERT_TRUE(GetExtensionProcessManager()->HasExtensionHost(extension_host));
    ASSERT_TRUE(extension_host->IsRenderViewLive());
    ASSERT_EQ(extension_host->render_view_host()->process(),
        GetExtensionProcessManager()->GetExtensionProcess(extension->id()));
  }

  void LoadTestExtension() {
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
    const size_t size_before = GetExtensionService()->extensions()->size();
    ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("common").AppendASCII("background_page")));
    const Extension* extension = GetExtensionService()->extensions()->back();
    ASSERT_TRUE(extension);
    first_extension_id_ = extension->id();
    CheckExtensionConsistency(size_before);
  }

  void LoadSecondExtension() {
    int offset = GetExtensionService()->extensions()->size();
    ASSERT_TRUE(LoadExtension(
        test_data_dir_.AppendASCII("install").AppendASCII("install")));
    const Extension* extension =
        GetExtensionService()->extensions()->at(offset);
    ASSERT_TRUE(extension);
    second_extension_id_ = extension->id();
    CheckExtensionConsistency(offset);
  }

  std::string first_extension_id_;
  std::string second_extension_id_;
};

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, Basic) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());
  AcceptNotification(0);

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(size_before);
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, CloseAndReload) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);

  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());

  CancelNotification(0);
  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(size_before);
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, ReloadIndependently) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(size_before);

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
  CrashExtension(size_before);
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
  CheckExtensionConsistency(size_before);

  // The balloon should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, CountBalloons());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       ReloadIndependentlyNavigatePage) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
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
  CheckExtensionConsistency(size_before);

  // The infobar should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, CountBalloons());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       ReloadIndependentlyTwoInfoBars) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();

  // Open a new window so that there will be an info bar in each.
  Browser* browser2 = CreateBrowser(browser()->profile());

  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);
  ASSERT_EQ(1U, CountBalloons());

  TabContents* current_tab2 = browser2->GetSelectedTabContents();
  ASSERT_TRUE(current_tab2);
  ASSERT_EQ(1U, CountBalloons());

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(size_before);

  // Both infobars should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, CountBalloons());
  ASSERT_EQ(0U, CountBalloons());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       ReloadIndependentlyTwoInfoBarsSameBrowser) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();

  // Open a new window so that there will be an info bar in each.
  Browser* browser2 = CreateBrowser(browser()->profile());

  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(current_tab);
  ASSERT_EQ(1U, CountBalloons());

  TabContents* current_tab2 = browser2->GetSelectedTabContents();
  ASSERT_TRUE(current_tab2);
  ASSERT_EQ(1U, CountBalloons());

  // Move second window into first browser so there will be multiple tabs
  // with the info bar for the same extension in one browser.
  TabContentsWrapper* contents =
      browser2->tabstrip_model()->DetachTabContentsAt(0);
  browser()->tabstrip_model()->AppendTabContents(contents, true);
  current_tab2 = browser()->GetSelectedTabContents();
  ASSERT_EQ(1U, CountBalloons());
  ASSERT_NE(current_tab2, current_tab);

  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(size_before);

  // Both infobars should automatically hide after the extension is successfully
  // reloaded.
  ASSERT_EQ(0U, CountBalloons());
  browser()->SelectPreviousTab();
  ASSERT_EQ(current_tab, browser()->GetSelectedTabContents());
  ASSERT_EQ(0U, CountBalloons());
}

// Make sure that when we don't do anything about the crashed extension
// and close the browser, it doesn't crash. The browser is closed implicitly
// at the end of each browser test.
IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, ShutdownWhileCrashed) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, TwoExtensionsCrashFirst) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  AcceptNotification(0);

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(size_before);
  CheckExtensionConsistency(size_before + 1);
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, TwoExtensionsCrashSecond) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before + 1);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  AcceptNotification(0);

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(size_before);
  CheckExtensionConsistency(size_before + 1);
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       TwoExtensionsCrashBothAtOnce) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 2,
            GetExtensionService()->terminated_extensions()->size());

  {
    SCOPED_TRACE("first balloon");
    AcceptNotification(0);
    CheckExtensionConsistency(size_before);
  }

  {
    SCOPED_TRACE("second balloon");
    AcceptNotification(0);
    CheckExtensionConsistency(size_before);
    CheckExtensionConsistency(size_before + 1);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, TwoExtensionsOneByOne) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  {
    SCOPED_TRACE("first balloon");
    AcceptNotification(0);
    CheckExtensionConsistency(size_before);
  }

  {
    SCOPED_TRACE("second balloon");
    AcceptNotification(0);
    CheckExtensionConsistency(size_before);
    CheckExtensionConsistency(size_before + 1);
  }
}

// Make sure that when we don't do anything about the crashed extensions
// and close the browser, it doesn't crash. The browser is closed implicitly
// at the end of each browser test.
IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       TwoExtensionsShutdownWhileCrashed) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, TwoExtensionsIgnoreFirst) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  CancelNotification(0);
  // Cancelling the balloon at 0 will close the balloon, and the balloon in
  // index 1 will move into index 0.
  AcceptNotification(0);

  SCOPED_TRACE("balloons done");
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  CheckExtensionConsistency(size_before);
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest,
                       TwoExtensionsReloadIndependently) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  CrashExtension(size_before);
  ASSERT_EQ(size_before, GetExtensionService()->extensions()->size());

  {
    SCOPED_TRACE("first: reload");
    TabContents* current_tab = browser()->GetSelectedTabContents();
    ASSERT_TRUE(current_tab);
    // At the beginning we should have one infobar displayed for each extension.
    ASSERT_EQ(2U, CountBalloons());
    ReloadExtension(first_extension_id_);
    // One of the infobars should hide after the extension is reloaded.
    ASSERT_EQ(1U, CountBalloons());
    CheckExtensionConsistency(size_before);
  }

  {
    SCOPED_TRACE("second: balloon");
    AcceptNotification(0);
    CheckExtensionConsistency(size_before);
    CheckExtensionConsistency(size_before + 1);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, CrashAndUninstall) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
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

IN_PROC_BROWSER_TEST_F(ExtensionCrashRecoveryTest, CrashAndUnloadAll) {
  const size_t size_before = GetExtensionService()->extensions()->size();
  const size_t crash_size_before =
      GetExtensionService()->terminated_extensions()->size();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(size_before);
  ASSERT_EQ(size_before + 1, GetExtensionService()->extensions()->size());
  ASSERT_EQ(crash_size_before + 1,
            GetExtensionService()->terminated_extensions()->size());

  GetExtensionService()->UnloadAllExtensions();
  ASSERT_EQ(crash_size_before,
            GetExtensionService()->terminated_extensions()->size());
}
