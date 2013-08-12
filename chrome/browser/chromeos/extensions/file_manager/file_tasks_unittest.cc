// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_tasks.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace file_tasks {

TEST(FileManagerFileTasksTest, MakeTaskID) {
  EXPECT_EQ("app-id|app|action-id", MakeTaskID("app-id", "app", "action-id"));
}

TEST(FileManagerFileTasksTest, MakeDriveAppTaskId) {
  EXPECT_EQ("app-id|drive|open-with", MakeDriveAppTaskId("app-id"));
}

TEST(FileManagerFileTasksTest, ParseTaskID_Basic) {
  TaskDescriptor task;
  // A task ID usually has three parts.
  EXPECT_TRUE(ParseTaskID("app-id|app|action-id", &task));
  EXPECT_EQ("app-id", task.app_id);
  EXPECT_EQ("app", task.task_type);
  EXPECT_EQ("action-id", task.action_id);
}

TEST(FileManagerFileTasksTest, ParseTaskID_Legacy) {
  TaskDescriptor task;
  // A legacy task ID only has two parts. The task type should be to "file".
  EXPECT_TRUE(ParseTaskID("app-id|action-id", &task));
  EXPECT_EQ("app-id", task.app_id);
  EXPECT_EQ("file", task.task_type);
  EXPECT_EQ("action-id", task.action_id);
}

TEST(FileManagerFileTasksTest, ParseTaskID_LegacyDrive) {
  TaskDescriptor task;
  // A legacy task ID only has two parts. For Drive app, the app ID is
  // prefixed with "drive-app:".
  EXPECT_TRUE(ParseTaskID("drive-app:app-id|action-id", &task));
  EXPECT_EQ("app-id", task.app_id);
  EXPECT_EQ("drive", task.task_type);
  EXPECT_EQ("action-id", task.action_id);
}

TEST(FileManagerFileTasksTest, ParseTaskID_Invalid) {
  TaskDescriptor task;
  EXPECT_FALSE(ParseTaskID("invalid", &task));
}

}  // namespace file_tasks
}  // namespace file_manager.
