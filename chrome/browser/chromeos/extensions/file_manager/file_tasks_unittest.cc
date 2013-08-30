// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_tasks.h"

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace file_tasks {

TEST(FileManagerFileTasksTest,
     FullTaskDescriptor_NonDriveAppWithIconAndDefault) {
  FullTaskDescriptor full_descriptor(
      TaskDescriptor("app-id",
                     TASK_TYPE_FILE_BROWSER_HANDLER,
                     "action-id"),
      "task title",
      GURL("http://example.com/icon.png"),
      true /* is_default */);

  scoped_ptr<base::DictionaryValue> dictionary(
      full_descriptor.AsDictionaryValue());
  std::string task_id;
  EXPECT_TRUE(dictionary->GetString("taskId", &task_id));
  EXPECT_EQ("app-id|file|action-id", task_id);

  std::string icon_url;
  EXPECT_TRUE(dictionary->GetString("iconUrl", &icon_url));
  EXPECT_EQ("http://example.com/icon.png", icon_url);

  std::string title;
  EXPECT_TRUE(dictionary->GetString("title", &title));
  EXPECT_EQ("task title", title);

  bool is_drive_app = false;
  EXPECT_TRUE(dictionary->GetBoolean("driveApp", &is_drive_app));
  EXPECT_FALSE(is_drive_app);

  bool is_default = false;
  EXPECT_TRUE(dictionary->GetBoolean("isDefault", &is_default));
  EXPECT_TRUE(is_default);
}

TEST(FileManagerFileTasksTest,
     FullTaskDescriptor_DriveAppWithoutIconAndNotDefault) {
  FullTaskDescriptor full_descriptor(
      TaskDescriptor("app-id",
                     TASK_TYPE_DRIVE_APP,
                     "action-id"),
      "task title",
      GURL(),  // No icon URL.
      false /* is_default */);

  scoped_ptr<base::DictionaryValue> dictionary(
      full_descriptor.AsDictionaryValue());
  std::string task_id;
  EXPECT_TRUE(dictionary->GetString("taskId", &task_id));
  EXPECT_EQ("app-id|drive|action-id", task_id);

  std::string icon_url;
  EXPECT_FALSE(dictionary->GetString("iconUrl", &icon_url));

  std::string title;
  EXPECT_TRUE(dictionary->GetString("title", &title));
  EXPECT_EQ("task title", title);

  bool is_drive_app = false;
  EXPECT_TRUE(dictionary->GetBoolean("driveApp", &is_drive_app));
  EXPECT_TRUE(is_drive_app);

  bool is_default = false;
  EXPECT_TRUE(dictionary->GetBoolean("isDefault", &is_default));
  EXPECT_FALSE(is_default);
}

TEST(FileManagerFileTasksTest, MakeTaskID) {
  EXPECT_EQ("app-id|file|action-id",
            MakeTaskID("app-id", TASK_TYPE_FILE_BROWSER_HANDLER, "action-id"));
  EXPECT_EQ("app-id|app|action-id",
            MakeTaskID("app-id", TASK_TYPE_FILE_HANDLER, "action-id"));
  EXPECT_EQ("app-id|drive|action-id",
            MakeTaskID("app-id", TASK_TYPE_DRIVE_APP, "action-id"));
}

TEST(FileManagerFileTasksTest, MakeDriveAppTaskId) {
  EXPECT_EQ("app-id|drive|open-with", MakeDriveAppTaskId("app-id"));
}

TEST(FileManagerFileTasksTest, TaskDescriptorToId) {
  EXPECT_EQ("app-id|file|action-id",
            TaskDescriptorToId(TaskDescriptor("app-id",
                                              TASK_TYPE_FILE_BROWSER_HANDLER,
                                              "action-id")));
}

TEST(FileManagerFileTasksTest, ParseTaskID_FileBrowserHandler) {
  TaskDescriptor task;
  EXPECT_TRUE(ParseTaskID("app-id|file|action-id", &task));
  EXPECT_EQ("app-id", task.app_id);
  EXPECT_EQ(TASK_TYPE_FILE_BROWSER_HANDLER, task.task_type);
  EXPECT_EQ("action-id", task.action_id);
}

TEST(FileManagerFileTasksTest, ParseTaskID_FileHandler) {
  TaskDescriptor task;
  EXPECT_TRUE(ParseTaskID("app-id|app|action-id", &task));
  EXPECT_EQ("app-id", task.app_id);
  EXPECT_EQ(TASK_TYPE_FILE_HANDLER, task.task_type);
  EXPECT_EQ("action-id", task.action_id);
}

TEST(FileManagerFileTasksTest, ParseTaskID_DriveApp) {
  TaskDescriptor task;
  EXPECT_TRUE(ParseTaskID("app-id|drive|action-id", &task));
  EXPECT_EQ("app-id", task.app_id);
  EXPECT_EQ(TASK_TYPE_DRIVE_APP, task.task_type);
  EXPECT_EQ("action-id", task.action_id);
}

TEST(FileManagerFileTasksTest, ParseTaskID_Legacy) {
  TaskDescriptor task;
  // A legacy task ID only has two parts. The task type should be
  // TASK_TYPE_FILE_BROWSER_HANDLER.
  EXPECT_TRUE(ParseTaskID("app-id|action-id", &task));
  EXPECT_EQ("app-id", task.app_id);
  EXPECT_EQ(TASK_TYPE_FILE_BROWSER_HANDLER, task.task_type);
  EXPECT_EQ("action-id", task.action_id);
}

TEST(FileManagerFileTasksTest, ParseTaskID_LegacyDrive) {
  TaskDescriptor task;
  // A legacy task ID only has two parts. For Drive app, the app ID is
  // prefixed with "drive-app:".
  EXPECT_TRUE(ParseTaskID("drive-app:app-id|action-id", &task));
  EXPECT_EQ("app-id", task.app_id);
  EXPECT_EQ(TASK_TYPE_DRIVE_APP, task.task_type);
  EXPECT_EQ("action-id", task.action_id);
}

TEST(FileManagerFileTasksTest, ParseTaskID_Invalid) {
  TaskDescriptor task;
  EXPECT_FALSE(ParseTaskID("invalid", &task));
}

TEST(FileManagerFileTasksTest, ParseTaskID_UnknownTaskType) {
  TaskDescriptor task;
  EXPECT_FALSE(ParseTaskID("app-id|unknown|action-id", &task));
}

}  // namespace file_tasks
}  // namespace file_manager.
