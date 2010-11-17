// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include "app/l10n_util.h"
#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/background_contents_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crashed_extension_infobar.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const FilePath::CharType* kTitle1File = FILE_PATH_LITERAL("title1.html");

class ResourceChangeObserver : public TaskManagerModelObserver {
 public:
  ResourceChangeObserver(const TaskManagerModel* model,
                         int target_resource_count)
      : model_(model),
        target_resource_count_(target_resource_count) {
  }

  virtual void OnModelChanged() {
    OnResourceChange();
  }

  virtual void OnItemsChanged(int start, int length) {
    OnResourceChange();
  }

  virtual void OnItemsAdded(int start, int length) {
    OnResourceChange();
  }

  virtual void OnItemsRemoved(int start, int length) {
    OnResourceChange();
  }

 private:
  void OnResourceChange() {
    if (model_->ResourceCount() == target_resource_count_)
      MessageLoopForUI::current()->Quit();
  }

  const TaskManagerModel* model_;
  const int target_resource_count_;
};

}  // namespace

class TaskManagerBrowserTest : public ExtensionBrowserTest {
 public:
  TaskManagerModel* model() const {
    return TaskManager::GetInstance()->model();
  }

  void WaitForResourceChange(int target_count) {
    if (model()->ResourceCount() == target_count)
      return;
    ResourceChangeObserver observer(model(), target_count);
    model()->AddObserver(&observer);
    ui_test_utils::RunMessageLoop();
    model()->RemoveObserver(&observer);
  }
};

// Crashes on Vista (dbg): http://crbug.com/44991
#if defined(OS_WIN)
#define ShutdownWhileOpen DISABLED_ShutdownWhileOpen
#endif
// Regression test for http://crbug.com/13361
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, ShutdownWhileOpen) {
  browser()->window()->ShowTaskManager();
}

// Times out on Vista; disabled to keep tests fast. http://crbug.com/44991
#if defined(OS_WIN)
#define NoticeTabContentsChanges DISABLED_NoticeTabContentsChanges
#endif
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeTabContentsChanges) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  EXPECT_EQ(2, model()->ResourceCount());

  // Open a new tab and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));
  AddTabAtIndex(0, url, PageTransition::TYPED);
  WaitForResourceChange(3);

  // Close the tab and verify that we notice.
  TabContents* first_tab = browser()->GetTabContentsAt(0);
  ASSERT_TRUE(first_tab);
  browser()->CloseTabContents(first_tab);
  WaitForResourceChange(2);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeBGContentsChanges) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  EXPECT_EQ(2, model()->ResourceCount());

  // Open a new background contents and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));

  BackgroundContentsService* service =
      browser()->profile()->GetBackgroundContentsService();
  string16 application_id(ASCIIToUTF16("test_app_id"));
  service->LoadBackgroundContents(browser()->profile(),
                                  url,
                                  ASCIIToUTF16("background_page"),
                                  application_id);
  WaitForResourceChange(3);

  // Close the background contents and verify that we notice.
  service->ShutdownAssociatedBackgroundContents(application_id);
  WaitForResourceChange(2);
}

#if defined(OS_WIN)
// http://crbug.com/31663
#define MAYBE_NoticeExtensionChanges DISABLED_NoticeExtensionChanges
#else
// Flaky test bug filed in http://crbug.com/51701
#define MAYBE_NoticeExtensionChanges FLAKY_NoticeExtensionChanges
#endif

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_NoticeExtensionChanges) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  EXPECT_EQ(2, model()->ResourceCount());

  // Loading an extension with a background page should result in a new
  // resource being created for it.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));
  WaitForResourceChange(3);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeNotificationChanges) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager.
  browser()->window()->ShowTaskManager();
  // Expect to see the browser and the New Tab Page renderer.
  EXPECT_EQ(2, model()->ResourceCount());

  // Show a notification.
  NotificationUIManager* notifications =
      g_browser_process->notification_ui_manager();

  string16 content = DesktopNotificationService::CreateDataUrl(
      GURL(), ASCIIToUTF16("Hello World!"), string16(),
      WebKit::WebTextDirectionDefault);

  scoped_refptr<NotificationDelegate> del1(new MockNotificationDelegate("n1"));
  Notification n1(
      GURL(), GURL(content), ASCIIToUTF16("Test 1"), string16(), del1.get());
  scoped_refptr<NotificationDelegate> del2(new MockNotificationDelegate("n2"));
  Notification n2(
      GURL(), GURL(content), ASCIIToUTF16("Test 2"), string16(), del2.get());

  notifications->Add(n1, browser()->profile());
  WaitForResourceChange(3);
  notifications->Add(n2, browser()->profile());
  WaitForResourceChange(4);
  notifications->Cancel(n1);
  WaitForResourceChange(3);
  notifications->Cancel(n2);
  WaitForResourceChange(2);
}

// Times out on Vista; disabled to keep tests fast. http://crbug.com/44991
#if defined(OS_WIN)
#define KillExtension DISABLED_KillExtension
#endif
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, KillExtension) {
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));

  // Wait until we see the loaded extension in the task manager (the three
  // resources are: the browser process, New Tab Page, and the extension).
  WaitForResourceChange(3);

  EXPECT_TRUE(model()->GetResourceExtension(0) == NULL);
  EXPECT_TRUE(model()->GetResourceExtension(1) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) != NULL);

  // Kill the extension process and make sure we notice it.
  TaskManager::GetInstance()->KillProcess(2);
  WaitForResourceChange(2);
}

// Times out on Vista; disabled to keep tests fast. http://crbug.com/44991
#if defined(OS_WIN)
#define KillExtensionAndReload DISABLED_KillExtensionAndReload
#endif
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, KillExtensionAndReload) {
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));

  // Wait until we see the loaded extension in the task manager (the three
  // resources are: the browser process, New Tab Page, and the extension).
  WaitForResourceChange(3);

  EXPECT_TRUE(model()->GetResourceExtension(0) == NULL);
  EXPECT_TRUE(model()->GetResourceExtension(1) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) != NULL);

  // Kill the extension process and make sure we notice it.
  TaskManager::GetInstance()->KillProcess(2);
  WaitForResourceChange(2);

  // Reload the extension using the "crashed extension" infobar while the task
  // manager is still visible. Make sure we don't crash and the extension
  // gets reloaded and noticed in the task manager.
  TabContents* current_tab = browser()->GetSelectedTabContents();
  ASSERT_EQ(1, current_tab->infobar_delegate_count());
  InfoBarDelegate* delegate = current_tab->GetInfoBarDelegateAt(0);
  CrashedExtensionInfoBarDelegate* crashed_delegate =
      delegate->AsCrashedExtensionInfoBarDelegate();
  ASSERT_TRUE(crashed_delegate);
  crashed_delegate->Accept();
  WaitForResourceChange(3);
}

// Regression test for http://crbug.com/18693.
// Crashy, http://crbug.com/42315.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DISABLED_ReloadExtension) {
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));

  // Wait until we see the loaded extension in the task manager (the three
  // resources are: the browser process, New Tab Page, and the extension).
  WaitForResourceChange(3);

  EXPECT_TRUE(model()->GetResourceExtension(0) == NULL);
  EXPECT_TRUE(model()->GetResourceExtension(1) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) != NULL);

  const Extension* extension = model()->GetResourceExtension(2);

  // Reload the extension a few times and make sure our resource count
  // doesn't increase.
  ReloadExtension(extension->id());
  WaitForResourceChange(3);
  extension = model()->GetResourceExtension(2);

  ReloadExtension(extension->id());
  WaitForResourceChange(3);
  extension = model()->GetResourceExtension(2);

  ReloadExtension(extension->id());
  WaitForResourceChange(3);
}

// Crashy, http://crbug.com/42301.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest,
                       DISABLED_PopulateWebCacheFields) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  EXPECT_EQ(2, model()->ResourceCount());

  // Open a new tab and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));
  AddTabAtIndex(0, url, PageTransition::TYPED);
  WaitForResourceChange(3);

  // Check that we get some value for the cache columns.
  DCHECK_NE(model()->GetResourceWebCoreImageCacheSize(2),
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT));
  DCHECK_NE(model()->GetResourceWebCoreScriptsCacheSize(2),
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT));
  DCHECK_NE(model()->GetResourceWebCoreCSSCacheSize(2),
            l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NA_CELL_TEXT));
}
