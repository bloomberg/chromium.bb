// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_tasks.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/file_manager/app_id.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace file_tasks {
namespace {

// Registers the default task preferences. Used for testing
// ChooseAndSetDefaultTask().
void RegisterDefaultTaskPreferences(TestingPrefServiceSimple* pref_service) {
  DCHECK(pref_service);

  pref_service->registry()->RegisterDictionaryPref(
      prefs::kDefaultTasksByMimeType);
  pref_service->registry()->RegisterDictionaryPref(
      prefs::kDefaultTasksBySuffix);
}

// Updates the default task preferences per the given dictionary values. Used
// for testing ChooseAndSetDefaultTask.
void UpdateDefaultTaskPreferences(TestingPrefServiceSimple* pref_service,
                                  const DictionaryValue& mime_types,
                                  const DictionaryValue& suffixes) {
  DCHECK(pref_service);

  pref_service->Set(prefs::kDefaultTasksByMimeType, mime_types);
  pref_service->Set(prefs::kDefaultTasksBySuffix, suffixes);
}

}  // namespace

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

// Test that the right task is chosen from multiple choices per mime types
// and file extensions.
TEST(FileManagerFileTasksTest, ChooseAndSetDefaultTask_MultipleTasks) {
  TestingPrefServiceSimple pref_service;
  RegisterDefaultTaskPreferences(&pref_service);

  // Text.app and Nice.app were found for "foo.txt".
  TaskDescriptor text_app_task("text-app-id",
                               TASK_TYPE_FILE_HANDLER,
                               "action-id");
  TaskDescriptor nice_app_task("nice-app-id",
                               TASK_TYPE_FILE_HANDLER,
                               "action-id");
  std::vector<FullTaskDescriptor> tasks;
  tasks.push_back(FullTaskDescriptor(
      text_app_task,
      "Text.app",
      GURL("http://example.com/text_app.png"),
      false /* is_default */));
  tasks.push_back(FullTaskDescriptor(
      nice_app_task,
      "Nice.app",
      GURL("http://example.com/nice_app.png"),
      false /* is_default */));
  PathAndMimeTypeSet path_mime_set;
  path_mime_set.insert(std::make_pair(
      base::FilePath::FromUTF8Unsafe("foo.txt"),
      "text/plain"));

  // None of them should be chosen as default, as nothing is set in the
  // preferences.
  ChooseAndSetDefaultTask(pref_service, path_mime_set, &tasks);
  EXPECT_FALSE(tasks[0].is_default());
  EXPECT_FALSE(tasks[1].is_default());

  // Set Text.app as default for "text/plain" in the preferences.
  DictionaryValue empty;
  DictionaryValue mime_types;
  mime_types.SetStringWithoutPathExpansion(
      "text/plain",
      TaskDescriptorToId(text_app_task));
  UpdateDefaultTaskPreferences(&pref_service, mime_types, empty);

  // Text.app should be chosen as default.
  ChooseAndSetDefaultTask(pref_service, path_mime_set, &tasks);
  EXPECT_TRUE(tasks[0].is_default());
  EXPECT_FALSE(tasks[1].is_default());

  // Change it back to non-default for testing further.
  tasks[0].set_is_default(false);

  // Clear the preferences and make sure none of them are default.
  UpdateDefaultTaskPreferences(&pref_service, empty, empty);
  ChooseAndSetDefaultTask(pref_service, path_mime_set, &tasks);
  EXPECT_FALSE(tasks[0].is_default());
  EXPECT_FALSE(tasks[1].is_default());

  // Set Nice.app as default for ".txt" in the preferences.
  DictionaryValue suffixes;
  suffixes.SetStringWithoutPathExpansion(
      ".txt",
      TaskDescriptorToId(nice_app_task));
  UpdateDefaultTaskPreferences(&pref_service, empty, suffixes);

  // Now Nice.app should be chosen as default.
  ChooseAndSetDefaultTask(pref_service, path_mime_set, &tasks);
  EXPECT_FALSE(tasks[0].is_default());
  EXPECT_TRUE(tasks[1].is_default());
}

// Test that Files.app's internal file browser handler is chosen as default
// even if nothing is set in the preferences.
TEST(FileManagerFileTasksTest, ChooseAndSetDefaultTask_FallbackFileBrowser) {
  TestingPrefServiceSimple pref_service;
  RegisterDefaultTaskPreferences(&pref_service);

  // Files.app's internal file browser handler was found for "foo.txt".
  TaskDescriptor files_app_task(kFileManagerAppId,
                                TASK_TYPE_FILE_BROWSER_HANDLER,
                                "view-in-browser");
  std::vector<FullTaskDescriptor> tasks;
  tasks.push_back(FullTaskDescriptor(
      files_app_task,
      "View in browser",
      GURL("http://example.com/some_icon.png"),
      false /* is_default */));
  PathAndMimeTypeSet path_mime_set;
  path_mime_set.insert(std::make_pair(
      base::FilePath::FromUTF8Unsafe("foo.txt"),
      "text/plain"));

  // The internal file browser handler should be chosen as default, as it's a
  // fallback file browser handler.
  ChooseAndSetDefaultTask(pref_service, path_mime_set, &tasks);
  EXPECT_TRUE(tasks[0].is_default());
}

}  // namespace file_tasks
}  // namespace file_manager.
