// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager.h"

#include "app/l10n_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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
  EXPECT_EQ(2, model()->ResourceCount());

  // Open a new tab and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(L".", L"title1.html"));
  browser()->AddTabWithURL(url, GURL(), PageTransition::TYPED,
                           true, 0, false, NULL);
  WaitForResourceChange(3);

  // Close the tab and verify that we notice.
  TabContents* first_tab = browser()->GetTabContentsAt(0);
  ASSERT_TRUE(first_tab);
  browser()->CloseTabContents(first_tab);
  WaitForResourceChange(2);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeExtensionChanges) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  EXPECT_EQ(2, model()->ResourceCount());

  // Loading an extension should result in a new resource being
  // created for it.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("one_in_shelf")));
  WaitForResourceChange(3);

  // Make sure we also recognize extensions with just background pages.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));
  WaitForResourceChange(4);
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

// Regression test for http://crbug.com/18693.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, ReloadExtension) {
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
  EXPECT_EQ(3, model()->ResourceCount());
  extension = model()->GetResourceExtension(2);

  ReloadExtension(extension->id());
  EXPECT_EQ(3, model()->ResourceCount());
  extension = model()->GetResourceExtension(2);

  ReloadExtension(extension->id());
  EXPECT_EQ(3, model()->ResourceCount());
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, PopulateWebCacheFields) {
  EXPECT_EQ(0, model()->ResourceCount());

  // Show the task manager. This populates the model, and helps with debugging
  // (you see the task manager).
  browser()->window()->ShowTaskManager();

  // Browser and the New Tab Page.
  EXPECT_EQ(2, model()->ResourceCount());

  // Open a new tab and make sure we notice that.
  GURL url(ui_test_utils::GetTestUrl(L".", L"title1.html"));
  browser()->AddTabWithURL(url, GURL(), PageTransition::TYPED,
                           true, 0, false, NULL);
  WaitForResourceChange(3);

  // Check that we get some value for the cache columns.
  DCHECK_NE(model()->GetResourceWebCoreImageCacheSize(2),
            l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT));
  DCHECK_NE(model()->GetResourceWebCoreScriptsCacheSize(2),
            l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT));
  DCHECK_NE(model()->GetResourceWebCoreCSSCacheSize(2),
            l10n_util::GetString(IDS_TASK_MANAGER_NA_CELL_TEXT));
}
