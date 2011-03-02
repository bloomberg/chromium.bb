// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager.h"

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/background_contents_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/crashed_extension_infobar.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

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

// Helper class used to wait for a BackgroundContents to finish loading.
class BackgroundContentsListener : public NotificationObserver {
 public:
  explicit BackgroundContentsListener(Profile* profile) {
    registrar_.Add(this, NotificationType::BACKGROUND_CONTENTS_NAVIGATED,
                   Source<Profile>(profile));
  }
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    // Quit once the BackgroundContents has been loaded.
    if (type.value == NotificationType::BACKGROUND_CONTENTS_NAVIGATED)
      MessageLoopForUI::current()->Quit();
  }
 private:
  NotificationRegistrar registrar_;
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

  // Wait for any pending BackgroundContents to finish starting up.
  void WaitForBackgroundContents() {
    BackgroundContentsListener listener(browser()->profile());
    ui_test_utils::RunMessageLoop();
  }
};

// Regression test for http://crbug.com/13361
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, ShutdownWhileOpen) {
  browser()->window()->ShowTaskManager();
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeTabContentsChanges) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  WaitForResourceChange(2);

  // Open a new tab and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));
  AddTabAtIndex(0, url, PageTransition::TYPED);
  WaitForResourceChange(3);

  // Check that the third entry is a tab contents resource whose title starts
  // starts with "Tab:".
  ASSERT_TRUE(model()->GetResourceTabContents(2) != NULL);
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_TAB_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(2), prefix, true));

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
  WaitForResourceChange(2);

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

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, KillBGContents) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  WaitForResourceChange(2);

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
  // Wait for the background contents process to finish loading.
  WaitForBackgroundContents();
  EXPECT_EQ(3, model()->ResourceCount());

  // Kill the background contents process and verify that it disappears from the
  // model.
  bool found = false;
  for (int i = 0; i < model()->ResourceCount(); ++i) {
    if (model()->IsBackgroundResource(i)) {
      TaskManager::GetInstance()->KillProcess(i);
      found = true;
      break;
    }
  }
  ASSERT_TRUE(found);
  WaitForResourceChange(2);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeExtensionChanges) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  WaitForResourceChange(2);

  // Loading an extension with a background page should result in a new
  // resource being created for it.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));
  WaitForResourceChange(3);

  // Unload extension to avoid crash on Windows (see http://crbug.com/31663).
  UnloadExtension(last_loaded_extension_id_);
  WaitForResourceChange(2);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeExtensionTabs) {
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // Browser, Extension background page, and the New Tab Page.
  WaitForResourceChange(3);

  // Open a new tab to an extension URL and make sure we notice that.
  GURL url("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html");
  AddTabAtIndex(0, url, PageTransition::TYPED);
  WaitForResourceChange(4);

  // Check that the third entry (background) is an extension resource whose
  // title starts with "Extension:".
  ASSERT_EQ(TaskManager::Resource::EXTENSION, model()->GetResourceType(2));
  ASSERT_TRUE(model()->GetResourceTabContents(2) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) != NULL);
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_EXTENSION_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(2), prefix, true));

  // Check that the fourth entry (page.html) is of type extension and has both
  // a tab contents and an extension. The title should start with "Extension:".
  ASSERT_EQ(TaskManager::Resource::EXTENSION, model()->GetResourceType(3));
  ASSERT_TRUE(model()->GetResourceTabContents(3) != NULL);
  ASSERT_TRUE(model()->GetResourceExtension(3) != NULL);
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(3), prefix, true));

  // Unload extension to avoid crash on Windows.
  UnloadExtension(last_loaded_extension_id_);
  WaitForResourceChange(2);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeAppTabs) {
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("packaged_app")));
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const Extension* extension =
      service->GetExtensionById(last_loaded_extension_id_, false);

  // Browser and the New Tab Page.
  WaitForResourceChange(2);

  // Open a new tab to the app's launch URL and make sure we notice that.
  GURL url(extension->GetResourceURL("main.html"));
  AddTabAtIndex(0, url, PageTransition::TYPED);
  WaitForResourceChange(3);

  // Check that the third entry (main.html) is of type extension and has both
  // a tab contents and an extension. The title should start with "App:".
  ASSERT_EQ(TaskManager::Resource::EXTENSION, model()->GetResourceType(2));
  ASSERT_TRUE(model()->GetResourceTabContents(2) != NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) != NULL);
  string16 prefix = l10n_util::GetStringFUTF16(
      IDS_TASK_MANAGER_APP_PREFIX, string16());
  ASSERT_TRUE(StartsWith(model()->GetResourceTitle(2), prefix, true));

  // Unload extension to avoid crash on Windows.
  UnloadExtension(last_loaded_extension_id_);
  WaitForResourceChange(2);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeNotificationChanges) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager.
  browser()->window()->ShowTaskManager();
  // Expect to see the browser and the New Tab Page renderer.
  WaitForResourceChange(2);

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
  notifications->CancelById(n1.notification_id());
  WaitForResourceChange(3);
  notifications->CancelById(n2.notification_id());
  WaitForResourceChange(2);
}

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

// Disabled, http://crbug.com/66957.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest,
                       DISABLED_KillExtensionAndReload) {
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
  ASSERT_EQ(1U, current_tab->infobar_count());
  ConfirmInfoBarDelegate* delegate =
      current_tab->GetInfoBarDelegateAt(0)->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(delegate);
  delegate->Accept();
  WaitForResourceChange(3);
}

// Regression test for http://crbug.com/18693.
//
// This test is crashy. See http://crbug.com/42315.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DISABLED_ReloadExtension) {
  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  LOG(INFO) << "loading extension";
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));

  // Wait until we see the loaded extension in the task manager (the three
  // resources are: the browser process, New Tab Page, and the extension).
  LOG(INFO) << "waiting for resource change";
  WaitForResourceChange(3);

  EXPECT_TRUE(model()->GetResourceExtension(0) == NULL);
  EXPECT_TRUE(model()->GetResourceExtension(1) == NULL);
  ASSERT_TRUE(model()->GetResourceExtension(2) != NULL);

  const Extension* extension = model()->GetResourceExtension(2);
  ASSERT_TRUE(extension != NULL);

  // Reload the extension a few times and make sure our resource count
  // doesn't increase.
  LOG(INFO) << "First extension reload";
  ReloadExtension(extension->id());
  WaitForResourceChange(3);
  extension = model()->GetResourceExtension(2);
  ASSERT_TRUE(extension != NULL);

  LOG(INFO) << "Second extension reload";
  ReloadExtension(extension->id());
  WaitForResourceChange(3);
  extension = model()->GetResourceExtension(2);
  ASSERT_TRUE(extension != NULL);

  LOG(INFO) << "Third extension reload";
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
  WaitForResourceChange(2);

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
