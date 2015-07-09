// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/task_management/providers/task_provider_observer.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/task_management/providers/web_contents/web_contents_task_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_management {

// Defines a browser test for testing that BackgroundContents are tagged
// properly and the TagsManager records these tags. It is also used to test that
// the WebContentsTaskProvider will be able to provide the appropriate
// BackgroundContentsTask.
class BackgroundContentsTagTest
    : public ExtensionBrowserTest,
      public TaskProviderObserver {
 public:
  BackgroundContentsTagTest() {}
  ~BackgroundContentsTagTest() override {}

  const extensions::Extension* LoadBackgroundExtension() {
    auto extension = LoadExtension(
        test_data_dir_.AppendASCII("app_process_background_instances"));
    return extension;
  }

  base::string16 GetBackgroundTaskExpectedName(
      const extensions::Extension* extension) {
    return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_BACKGROUND_PREFIX,
                                      base::UTF8ToUTF16(extension->name()));
  }

  // task_management::Task_providerObserver:
  void TaskAdded(Task* task) override {
    CHECK(task);
    ASSERT_FALSE(provided_tasks_.count(task));
    provided_tasks_.insert(task);
  }

  void TaskRemoved(Task* task) override {
    CHECK(task);
    ASSERT_TRUE(provided_tasks_.count(task));
    provided_tasks_.erase(task);
  }

  WebContentsTagsManager* tags_manager() const {
    return WebContentsTagsManager::GetInstance();
  }

  const std::set<Task*>& provided_tasks() const { return provided_tasks_; }

 protected:
  // ExtensionApiTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Pass flags to make testing apps easier.
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    test_data_dir_ = test_data_dir_.AppendASCII("api_test");
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisablePopupBlocking);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        extensions::switches::kAllowHTTPBackgroundPage);
  }

 private:
  std::set<Task*> provided_tasks_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundContentsTagTest);
};

// Tests that loading an extension that has a background contents will result in
// the tags manager recording a WebContentsTag.
IN_PROC_BROWSER_TEST_F(BackgroundContentsTagTest, TagsManagerRecordsATag) {
  EXPECT_TRUE(provided_tasks().empty());
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());
  EXPECT_TRUE(LoadBackgroundExtension());
  EXPECT_FALSE(tags_manager()->tracked_tags().empty());
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
  EXPECT_TRUE(provided_tasks().empty());
}

// Tests that background contents creation while the provider is being observed
// will also provide tasks.
IN_PROC_BROWSER_TEST_F(BackgroundContentsTagTest, TasksProvidedWhileObserving) {
  EXPECT_TRUE(provided_tasks().empty());
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());

  WebContentsTaskProvider provider;
  provider.SetObserver(this);

  // Still empty, no pre-existing tasks.
  EXPECT_TRUE(provided_tasks().empty());

  auto extension = LoadBackgroundExtension();
  ASSERT_NE(nullptr, extension);
  EXPECT_FALSE(tags_manager()->tracked_tags().empty());
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
  EXPECT_FALSE(provided_tasks().empty());
  EXPECT_EQ(1U, provided_tasks().size());

  // Now check the provided task.
  const Task* task = *provided_tasks().begin();
  EXPECT_EQ(Task::RENDERER, task->GetType());
  EXPECT_EQ(GetBackgroundTaskExpectedName(extension), task->title());

  // Unload the extension.
  UnloadExtension(extension->id());
  EXPECT_TRUE(provided_tasks().empty());
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());
}

// Tests providing a pre-existing background task to the observing operation.
IN_PROC_BROWSER_TEST_F(BackgroundContentsTagTest, PreExistingTasksAreProvided) {
  EXPECT_TRUE(provided_tasks().empty());
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());
  auto extension = LoadBackgroundExtension();
  ASSERT_NE(nullptr, extension);
  EXPECT_FALSE(tags_manager()->tracked_tags().empty());
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());

  WebContentsTaskProvider provider;
  provider.SetObserver(this);

  // Pre-existing task will be provided to us.
  EXPECT_FALSE(provided_tasks().empty());
  EXPECT_EQ(1U, provided_tasks().size());

  // Now check the provided task.
  const Task* task = *provided_tasks().begin();
  EXPECT_EQ(Task::RENDERER, task->GetType());
  EXPECT_EQ(GetBackgroundTaskExpectedName(extension), task->title());

  // Unload the extension.
  UnloadExtension(extension->id());
  EXPECT_TRUE(provided_tasks().empty());
  EXPECT_TRUE(tags_manager()->tracked_tags().empty());
}

}  // namespace task_management
