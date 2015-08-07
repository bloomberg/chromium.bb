// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/task_management/task_management_browsertest_util.h"
#include "chrome/common/chrome_switches.h"

namespace task_management {

class ExtensionTagsTest : public ExtensionBrowserTest {
 public:
  ExtensionTagsTest() {}
  ~ExtensionTagsTest() override {}

 protected:
  // ExtensionBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    // Do not launch device discovery process.
    command_line->AppendSwitch(switches::kDisableDeviceDiscoveryNotifications);
  }

  const std::vector<WebContentsTag*>& tracked_tags() const {
    return WebContentsTagsManager::GetInstance()->tracked_tags();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionTagsTest);
};

// Tests loading, disabling, enabling and unloading extensions and how that will
// affect the recording of tags.
IN_PROC_BROWSER_TEST_F(ExtensionTagsTest, Basic) {
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tracked_tags().size());

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
          .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
          .AppendASCII("1.0.0.0"));
  ASSERT_TRUE(extension);

  EXPECT_EQ(2U, tracked_tags().size());

  DisableExtension(extension->id());
  EXPECT_EQ(1U, tracked_tags().size());

  EnableExtension(extension->id());
  EXPECT_EQ(2U, tracked_tags().size());

  UnloadExtension(extension->id());
  EXPECT_EQ(1U, tracked_tags().size());
}

IN_PROC_BROWSER_TEST_F(ExtensionTagsTest, PreAndPostExistingTaskProviding) {
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tracked_tags().size());
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
          .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
          .AppendASCII("1.0.0.0"));
  ASSERT_TRUE(extension);

  EXPECT_EQ(2U, tracked_tags().size());
  EXPECT_TRUE(task_manager.tasks().empty());

  // Start observing, pre-existing tasks will be provided.
  task_manager.StartObserving();
  EXPECT_EQ(2U, task_manager.tasks().size());
  EXPECT_EQ(Task::EXTENSION, task_manager.tasks().back()->GetType());

  // Unload the extension and expect that the task manager now shows only the
  // about:blank tab.
  UnloadExtension(extension->id());
  EXPECT_EQ(1U, tracked_tags().size());
  EXPECT_EQ(1U, task_manager.tasks().size());
  const Task* about_blank_task = task_manager.tasks().back();
  EXPECT_EQ(Task::RENDERER, about_blank_task->GetType());
  EXPECT_EQ(base::UTF8ToUTF16("Tab: about:blank"), about_blank_task->title());

  // Reload the extension, the task manager should show it again.
  ReloadExtension(extension->id());
  EXPECT_EQ(2U, tracked_tags().size());
  EXPECT_EQ(2U, task_manager.tasks().size());
  EXPECT_EQ(Task::EXTENSION, task_manager.tasks().back()->GetType());
}

}  // namespace task_management

