// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"

using content::NavigationController;
using content::WebContents;
using extensions::Extension;
using extensions::ExtensionRegistry;

// Tests are timing out waiting for extension to crash.
// http://crbug.com/174705
#if defined(OS_MACOSX) || defined(USE_AURA) || defined(OS_LINUX)
#define MAYBE_ExtensionCrashRecoveryTest DISABLED_ExtensionCrashRecoveryTest
#else
#define MAYBE_ExtensionCrashRecoveryTest ExtensionCrashRecoveryTest
#endif  // defined(OS_MACOSX) || defined(USE_AURA) || defined(OS_LINUX)

class ExtensionCrashRecoveryTestBase : public ExtensionBrowserTest {
 protected:
  virtual void AcceptNotification(size_t index) = 0;
  virtual void CancelNotification(size_t index) = 0;
  virtual size_t CountBalloons() = 0;

  ExtensionService* GetExtensionService() {
    return extensions::ExtensionSystem::Get(browser()->profile())->
        extension_service();
  }

  extensions::ProcessManager* GetProcessManager() {
    return extensions::ProcessManager::Get(browser()->profile());
  }

  ExtensionRegistry* GetExtensionRegistry() {
    return ExtensionRegistry::Get(browser()->profile());
  }

  size_t GetEnabledExtensionCount() {
    return GetExtensionRegistry()->enabled_extensions().size();
  }

  size_t GetTerminatedExtensionCount() {
    return GetExtensionRegistry()->terminated_extensions().size();
  }

  void CrashExtension(const std::string& extension_id) {
    const Extension* extension = GetExtensionRegistry()->GetExtensionById(
        extension_id, ExtensionRegistry::ENABLED);
    ASSERT_TRUE(extension);
    extensions::ExtensionHost* extension_host = GetProcessManager()->
        GetBackgroundHostForExtension(extension_id);
    ASSERT_TRUE(extension_host);

    extension_host->render_process_host()->Shutdown(content::RESULT_CODE_KILLED,
                                                    false);
    ASSERT_TRUE(WaitForExtensionCrash(extension_id));
    ASSERT_FALSE(GetProcessManager()->
                 GetBackgroundHostForExtension(extension_id));

    // Wait for extension crash balloon to appear.
    base::RunLoop().RunUntilIdle();
  }

  void CheckExtensionConsistency(const std::string& extension_id) {
    const Extension* extension = GetExtensionRegistry()->GetExtensionById(
        extension_id, ExtensionRegistry::ENABLED);
    ASSERT_TRUE(extension);
    extensions::ExtensionHost* extension_host = GetProcessManager()->
        GetBackgroundHostForExtension(extension_id);
    ASSERT_TRUE(extension_host);
    extensions::ProcessManager::FrameSet frames =
        GetProcessManager()->GetAllFrames();
    ASSERT_NE(frames.end(),
              frames.find(extension_host->host_contents()->GetMainFrame()));
    ASSERT_FALSE(GetProcessManager()->GetAllFrames().empty());
    ASSERT_TRUE(extension_host->IsRenderViewLive());
    extensions::ProcessMap* process_map =
        extensions::ProcessMap::Get(browser()->profile());
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

class MAYBE_ExtensionCrashRecoveryTest : public ExtensionCrashRecoveryTestBase {
 protected:
  void AcceptNotification(size_t index) override {
    message_center::MessageCenter* message_center =
        message_center::MessageCenter::Get();
    ASSERT_GT(message_center->NotificationCount(), index);
    message_center::NotificationList::Notifications::reverse_iterator it =
        message_center->GetVisibleNotifications().rbegin();
    for (size_t i = 0; i < index; ++i)
      ++it;
    std::string id = (*it)->id();
    message_center->ClickOnNotification(id);
    WaitForExtensionLoad();
  }

  void CancelNotification(size_t index) override {
    message_center::MessageCenter* message_center =
        message_center::MessageCenter::Get();
    ASSERT_GT(message_center->NotificationCount(), index);
    message_center::NotificationList::Notifications::reverse_iterator it =
        message_center->GetVisibleNotifications().rbegin();
    for (size_t i = 0; i < index; ++i)
      ++it;
    ASSERT_TRUE(g_browser_process->notification_ui_manager()->CancelById(
        (*it)->id(),
        NotificationUIManager::GetProfileID(browser()->profile())));
  }

  size_t CountBalloons() override {
    return message_center::MessageCenter::Get()->NotificationCount();
  }
};

// Flaky: http://crbug.com/242167.
IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest, DISABLED_Basic) {
  const size_t count_before = GetEnabledExtensionCount();
  const size_t crash_count_before = GetTerminatedExtensionCount();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());
  ASSERT_EQ(crash_count_before + 1, GetTerminatedExtensionCount());
  ASSERT_NO_FATAL_FAILURE(AcceptNotification(0));

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(first_extension_id_);
  ASSERT_EQ(crash_count_before, GetTerminatedExtensionCount());
}

// Flaky, http://crbug.com/241191.
IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       DISABLED_CloseAndReload) {
  const size_t count_before = GetEnabledExtensionCount();
  const size_t crash_count_before = GetTerminatedExtensionCount();
  LoadTestExtension();
  CrashExtension(first_extension_id_);

  ASSERT_EQ(count_before, GetEnabledExtensionCount());
  ASSERT_EQ(crash_count_before + 1, GetTerminatedExtensionCount());

  ASSERT_NO_FATAL_FAILURE(CancelNotification(0));
  ReloadExtension(first_extension_id_);

  SCOPED_TRACE("after reloading");
  CheckExtensionConsistency(first_extension_id_);
  ASSERT_EQ(crash_count_before, GetTerminatedExtensionCount());
}

// Test is timing out on Windows http://crbug.com/174705.
#if defined(OS_WIN)
#define MAYBE_ReloadIndependently DISABLED_ReloadIndependently
#else
#define MAYBE_ReloadIndependently ReloadIndependently
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       MAYBE_ReloadIndependently) {
  const size_t count_before = GetEnabledExtensionCount();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());

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

// Test is timing out on Windows http://crbug.com/174705.
#if defined(OS_WIN)
#define MAYBE_ReloadIndependentlyChangeTabs DISABLED_ReloadIndependentlyChangeTabs
#else
#define MAYBE_ReloadIndependentlyChangeTabs ReloadIndependentlyChangeTabs
#endif  // defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       MAYBE_ReloadIndependentlyChangeTabs) {
  const size_t count_before = GetEnabledExtensionCount();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());

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
  const size_t count_before = GetEnabledExtensionCount();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());

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
  const size_t count_before = GetEnabledExtensionCount();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());
}

// Flaky, http://crbug.com/241245.
IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       DISABLED_TwoExtensionsCrashFirst) {
  const size_t count_before = GetEnabledExtensionCount();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before + 1, GetEnabledExtensionCount());
  ASSERT_NO_FATAL_FAILURE(AcceptNotification(0));

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(first_extension_id_);
  CheckExtensionConsistency(second_extension_id_);
}

// Flaky: http://crbug.com/242196
IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       DISABLED_TwoExtensionsCrashSecond) {
  const size_t count_before = GetEnabledExtensionCount();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(second_extension_id_);
  ASSERT_EQ(count_before + 1, GetEnabledExtensionCount());
  ASSERT_NO_FATAL_FAILURE(AcceptNotification(0));

  SCOPED_TRACE("after clicking the balloon");
  CheckExtensionConsistency(first_extension_id_);
  CheckExtensionConsistency(second_extension_id_);
}

IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       TwoExtensionsCrashBothAtOnce) {
  const size_t count_before = GetEnabledExtensionCount();
  const size_t crash_count_before = GetTerminatedExtensionCount();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before + 1, GetEnabledExtensionCount());
  ASSERT_EQ(crash_count_before + 1, GetTerminatedExtensionCount());
  CrashExtension(second_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());
  ASSERT_EQ(crash_count_before + 2, GetTerminatedExtensionCount());

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
  const size_t count_before = GetEnabledExtensionCount();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());
  LoadSecondExtension();
  CrashExtension(second_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());

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
  const size_t count_before = GetEnabledExtensionCount();
  LoadTestExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());
  LoadSecondExtension();
  CrashExtension(second_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());
}

// Flaky, http://crbug.com/241573.
IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       DISABLED_TwoExtensionsIgnoreFirst) {
  const size_t count_before = GetEnabledExtensionCount();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before + 1, GetEnabledExtensionCount());
  CrashExtension(second_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());

  // Accept notification 1 before canceling notification 0.
  // Otherwise, on Linux and Windows, there is a race here, in which
  // canceled notifications do not immediately go away.
  ASSERT_NO_FATAL_FAILURE(AcceptNotification(1));
  ASSERT_NO_FATAL_FAILURE(CancelNotification(0));

  SCOPED_TRACE("balloons done");
  ASSERT_EQ(count_before + 1, GetEnabledExtensionCount());
  CheckExtensionConsistency(second_extension_id_);
}

// Flaky, http://crbug.com/241164.
IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       DISABLED_TwoExtensionsReloadIndependently) {
  const size_t count_before = GetEnabledExtensionCount();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before + 1, GetEnabledExtensionCount());
  CrashExtension(second_extension_id_);
  ASSERT_EQ(count_before, GetEnabledExtensionCount());

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

// http://crbug.com/243648
#if defined(OS_WIN)
#define MAYBE_CrashAndUninstall DISABLED_CrashAndUninstall
#else
#define MAYBE_CrashAndUninstall CrashAndUninstall
#endif
IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       MAYBE_CrashAndUninstall) {
  const size_t count_before = GetEnabledExtensionCount();
  const size_t crash_count_before = GetTerminatedExtensionCount();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before + 1, GetEnabledExtensionCount());
  ASSERT_EQ(crash_count_before + 1, GetTerminatedExtensionCount());

  ASSERT_EQ(1U, CountBalloons());
  UninstallExtension(first_extension_id_);
  base::RunLoop().RunUntilIdle();

  SCOPED_TRACE("after uninstalling");
  ASSERT_EQ(count_before + 1, GetEnabledExtensionCount());
  ASSERT_EQ(crash_count_before, GetTerminatedExtensionCount());
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
  const size_t count_before = GetEnabledExtensionCount();
  const size_t crash_count_before = GetTerminatedExtensionCount();
  LoadTestExtension();
  LoadSecondExtension();
  CrashExtension(first_extension_id_);
  ASSERT_EQ(count_before + 1, GetEnabledExtensionCount());
  ASSERT_EQ(crash_count_before + 1, GetTerminatedExtensionCount());

  GetExtensionService()->UnloadAllExtensionsForTest();
  ASSERT_EQ(crash_count_before, GetTerminatedExtensionCount());
}

// Fails a DCHECK on Aura and Linux: http://crbug.com/169622
// Failing on Windows: http://crbug.com/232340
#if defined(USE_AURA)
#define MAYBE_ReloadTabsWithBackgroundPage DISABLED_ReloadTabsWithBackgroundPage
#else
#define MAYBE_ReloadTabsWithBackgroundPage ReloadTabsWithBackgroundPage
#endif

// Test that when an extension with a background page that has a tab open
// crashes, the tab stays open, and reloading it reloads the extension.
// Regression test for issue 71629.
IN_PROC_BROWSER_TEST_F(MAYBE_ExtensionCrashRecoveryTest,
                       MAYBE_ReloadTabsWithBackgroundPage) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  const size_t count_before = GetEnabledExtensionCount();
  const size_t crash_count_before = GetTerminatedExtensionCount();
  LoadTestExtension();

  // Open a tab extension.
  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(),
                               GURL(std::string(extensions::kExtensionScheme) +
                                   url::kStandardSchemeSeparator +
                                   first_extension_id_ + "/background.html"));

  const int tabs_before = tab_strip->count();
  CrashExtension(first_extension_id_);

  // Tab should still be open, and extension should be crashed.
  EXPECT_EQ(tabs_before, tab_strip->count());
  EXPECT_EQ(count_before, GetEnabledExtensionCount());
  EXPECT_EQ(crash_count_before + 1, GetTerminatedExtensionCount());

  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(
            &browser()->tab_strip_model()->GetActiveWebContents()->
                GetController()));
    chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }
  // Extension should now be loaded.
  SCOPED_TRACE("after reloading the tab");
  CheckExtensionConsistency(first_extension_id_);
  ASSERT_EQ(count_before + 1, GetEnabledExtensionCount());
  ASSERT_EQ(0U, CountBalloons());
}
