// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_tasks.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "base/values.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/drive/drive_app_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "google_apis/drive/drive_api_parser.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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
                                  const base::DictionaryValue& mime_types,
                                  const base::DictionaryValue& suffixes) {
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

  const std::string task_id =
      TaskDescriptorToId(full_descriptor.task_descriptor());
  EXPECT_EQ("app-id|file|action-id", task_id);
  EXPECT_EQ("http://example.com/icon.png", full_descriptor.icon_url().spec());
  EXPECT_EQ("task title", full_descriptor.task_title());
  EXPECT_TRUE(full_descriptor.is_default());
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

  const std::string task_id =
      TaskDescriptorToId(full_descriptor.task_descriptor());
  EXPECT_EQ("app-id|drive|action-id", task_id);
  EXPECT_TRUE(full_descriptor.icon_url().is_empty());
  EXPECT_EQ("task title", full_descriptor.task_title());
  EXPECT_FALSE(full_descriptor.is_default());
}

TEST(FileManagerFileTasksTest, MakeTaskID) {
  EXPECT_EQ("app-id|file|action-id",
            MakeTaskID("app-id", TASK_TYPE_FILE_BROWSER_HANDLER, "action-id"));
  EXPECT_EQ("app-id|app|action-id",
            MakeTaskID("app-id", TASK_TYPE_FILE_HANDLER, "action-id"));
  EXPECT_EQ("app-id|drive|action-id",
            MakeTaskID("app-id", TASK_TYPE_DRIVE_APP, "action-id"));
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

TEST(FileManagerFileTasksTest, FindDriveAppTasks) {
  TestingProfile profile;
  // For DriveAppRegistry, which checks CurrentlyOn(BrowserThread::UI).
  content::TestBrowserThreadBundle thread_bundle;

  // Foo.app can handle "text/plain" and "text/html"
  scoped_ptr<google_apis::AppResource> foo_app(new google_apis::AppResource);
  foo_app->set_product_id("foo_app_id");
  foo_app->set_application_id("foo_app_id");
  foo_app->set_name("Foo");
  foo_app->set_object_type("foo_object_type");
  ScopedVector<std::string> foo_mime_types;
  foo_mime_types.push_back(new std::string("text/plain"));
  foo_mime_types.push_back(new std::string("text/html"));
  foo_app->set_primary_mimetypes(foo_mime_types.Pass());

  // Bar.app can only handle "text/plain".
  scoped_ptr<google_apis::AppResource> bar_app(new google_apis::AppResource);
  bar_app->set_product_id("bar_app_id");
  bar_app->set_application_id("bar_app_id");
  bar_app->set_name("Bar");
  bar_app->set_object_type("bar_object_type");
  ScopedVector<std::string> bar_mime_types;
  bar_mime_types.push_back(new std::string("text/plain"));
  bar_app->set_primary_mimetypes(bar_mime_types.Pass());

  // Prepare DriveAppRegistry from Foo.app and Bar.app.
  ScopedVector<google_apis::AppResource> app_resources;
  app_resources.push_back(foo_app.release());
  app_resources.push_back(bar_app.release());
  google_apis::AppList app_list;
  app_list.set_items(app_resources.Pass());
  drive::DriveAppRegistry drive_app_registry(NULL);
  drive_app_registry.UpdateFromAppList(app_list);

  // Find apps for a "text/plain" file. Foo.app and Bar.app should be found.
  PathAndMimeTypeSet path_mime_set;
  path_mime_set.insert(
      std::make_pair(
          drive::util::GetDriveMountPointPath(&profile).AppendASCII("foo.txt"),
          "text/plain"));
  std::vector<FullTaskDescriptor> tasks;
  FindDriveAppTasks(drive_app_registry,
                    path_mime_set,
                    &tasks);
  ASSERT_EQ(2U, tasks.size());
  // Sort the app IDs, as the order is not guaranteed.
  std::vector<std::string> app_ids;
  app_ids.push_back(tasks[0].task_descriptor().app_id);
  app_ids.push_back(tasks[1].task_descriptor().app_id);
  std::sort(app_ids.begin(), app_ids.end());
  // Confirm that both Foo.app and Bar.app are found.
  EXPECT_EQ("bar_app_id", app_ids[0]);
  EXPECT_EQ("foo_app_id", app_ids[1]);

  // Find apps for "text/plain" and "text/html" files. Only Foo.app should be
  // found.
  path_mime_set.clear();
  path_mime_set.insert(
      std::make_pair(
          drive::util::GetDriveMountPointPath(&profile).AppendASCII("foo.txt"),
          "text/plain"));
  path_mime_set.insert(
      std::make_pair(
          drive::util::GetDriveMountPointPath(&profile).AppendASCII("foo.html"),
          "text/html"));
  tasks.clear();
  FindDriveAppTasks(drive_app_registry,
                    path_mime_set,
                    &tasks);
  ASSERT_EQ(1U, tasks.size());
  // Confirm that only Foo.app is found.
  EXPECT_EQ("foo_app_id", tasks[0].task_descriptor().app_id);

  // Add a "text/plain" file not on Drive. No tasks should be found.
  path_mime_set.insert(
      std::make_pair(base::FilePath::FromUTF8Unsafe("not_on_drive.txt"),
                     "text/plain"));
  tasks.clear();
  FindDriveAppTasks(drive_app_registry,
                    path_mime_set,
                    &tasks);
  // Confirm no tasks are found.
  ASSERT_TRUE(tasks.empty());
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
  base::DictionaryValue empty;
  base::DictionaryValue mime_types;
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
  base::DictionaryValue suffixes;
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

// Test using the test extension system, which needs lots of setup.
class FileManagerFileTasksComplexTest : public testing::Test {
 protected:
  FileManagerFileTasksComplexTest()
      : command_line_(CommandLine::NO_PROGRAM),
        extension_service_(NULL) {
    extensions::TestExtensionSystem* test_extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&test_profile_));
    extension_service_ = test_extension_system->CreateExtensionService(
        &command_line_,
        base::FilePath()  /* install_directory */,
        false  /* autoupdate_enabled*/);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
  TestingProfile test_profile_;
  CommandLine command_line_;
  ExtensionService* extension_service_;  // Owned by test_profile_;
};

// The basic logic is similar to a test case for FindDriveAppTasks above.
TEST_F(FileManagerFileTasksComplexTest, FindFileHandlerTasks) {
  // Random IDs generated by
  // % ruby -le 'print (0...32).to_a.map{(?a + rand(16)).chr}.join'
  const char kFooId[] = "hhgbjpmdppecanaaogonaigmmifgpaph";
  const char kBarId[] = "odlhccgofgkadkkhcmhgnhgahonahoca";
  const char kEphemeralId[] = "opoomfdlbjcbjinalcjdjfoiikdeaoel";

  // Foo.app can handle "text/plain" and "text/html".
  extensions::ExtensionBuilder foo_app;
  foo_app.SetManifest(extensions::DictionaryBuilder()
                      .Set("name", "Foo")
                      .Set("version", "1.0.0")
                      .Set("manifest_version", 2)
                      .Set("app",
                           extensions::DictionaryBuilder()
                           .Set("background",
                                extensions::DictionaryBuilder()
                                .Set("scripts",
                                     extensions::ListBuilder()
                                     .Append("background.js"))))
                      .Set("file_handlers",
                           extensions::DictionaryBuilder()
                           .Set("text",
                                extensions::DictionaryBuilder()
                                .Set("title", "Text")
                                .Set("types",
                                     extensions::ListBuilder()
                                     .Append("text/plain")
                                     .Append("text/html")))));
  foo_app.SetID(kFooId);
  extension_service_->AddExtension(foo_app.Build().get());

  // Bar.app can only handle "text/plain".
  extensions::ExtensionBuilder bar_app;
  bar_app.SetManifest(extensions::DictionaryBuilder()
                      .Set("name", "Bar")
                      .Set("version", "1.0.0")
                      .Set("manifest_version", 2)
                      .Set("app",
                           extensions::DictionaryBuilder()
                           .Set("background",
                                extensions::DictionaryBuilder()
                                .Set("scripts",
                                     extensions::ListBuilder()
                                     .Append("background.js"))))
                      .Set("file_handlers",
                           extensions::DictionaryBuilder()
                           .Set("text",
                                extensions::DictionaryBuilder()
                                .Set("title", "Text")
                                .Set("types",
                                     extensions::ListBuilder()
                                     .Append("text/plain")))));
  bar_app.SetID(kBarId);
  extension_service_->AddExtension(bar_app.Build().get());

  // Ephemeral.app is an ephemeral app that can handle "text/plain".
  // It should not ever be found as ephemeral apps cannot be file handlers.
  extensions::ExtensionBuilder ephemeral_app;
  ephemeral_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Ephemeral")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app",
               extensions::DictionaryBuilder().Set(
                   "background",
                   extensions::DictionaryBuilder().Set(
                       "scripts",
                       extensions::ListBuilder().Append("background.js"))))
          .Set("file_handlers",
               extensions::DictionaryBuilder().Set(
                   "text",
                   extensions::DictionaryBuilder().Set("title", "Text").Set(
                       "types",
                       extensions::ListBuilder().Append("text/plain")))));
  ephemeral_app.SetID(kEphemeralId);
  scoped_refptr<extensions::Extension> built_ephemeral_app(
      ephemeral_app.Build());
  extension_service_->AddExtension(built_ephemeral_app.get());
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(&test_profile_);
  extension_prefs->OnExtensionInstalled(built_ephemeral_app.get(),
                                        extensions::Extension::ENABLED,
                                        syncer::StringOrdinal(),
                                        extensions::kInstallFlagIsEphemeral,
                                        std::string());

  // Find apps for a "text/plain" file. Foo.app and Bar.app should be found.
  PathAndMimeTypeSet path_mime_set;
  path_mime_set.insert(
      std::make_pair(
          drive::util::GetDriveMountPointPath(&test_profile_).AppendASCII(
              "foo.txt"),
          "text/plain"));

  std::vector<FullTaskDescriptor> tasks;
  FindFileHandlerTasks(&test_profile_, path_mime_set, &tasks);
  ASSERT_EQ(2U, tasks.size());
  // Sort the app IDs, as the order is not guaranteed.
  std::vector<std::string> app_ids;
  app_ids.push_back(tasks[0].task_descriptor().app_id);
  app_ids.push_back(tasks[1].task_descriptor().app_id);
  std::sort(app_ids.begin(), app_ids.end());
  // Confirm that both Foo.app and Bar.app are found.
  EXPECT_EQ(kFooId, app_ids[0]);
  EXPECT_EQ(kBarId, app_ids[1]);

  // Find apps for "text/plain" and "text/html" files. Only Foo.app should be
  // found.
  path_mime_set.clear();
  path_mime_set.insert(
      std::make_pair(
          drive::util::GetDriveMountPointPath(&test_profile_).AppendASCII(
              "foo.txt"),
          "text/plain"));
  path_mime_set.insert(
      std::make_pair(
          drive::util::GetDriveMountPointPath(&test_profile_).AppendASCII(
              "foo.html"),
          "text/html"));
  tasks.clear();
  FindFileHandlerTasks(&test_profile_, path_mime_set, &tasks);
  ASSERT_EQ(1U, tasks.size());
  // Confirm that only Foo.app is found.
  EXPECT_EQ(kFooId, tasks[0].task_descriptor().app_id);

  // Add an "image/png" file. No tasks should be found.
  path_mime_set.insert(
      std::make_pair(base::FilePath::FromUTF8Unsafe("foo.png"),
                     "image/png"));
  tasks.clear();
  FindFileHandlerTasks(&test_profile_, path_mime_set, &tasks);
  // Confirm no tasks are found.
  ASSERT_TRUE(tasks.empty());
}

// The basic logic is similar to a test case for FindDriveAppTasks above.
TEST_F(FileManagerFileTasksComplexTest, FindFileBrowserHandlerTasks) {
  // Copied from FindFileHandlerTasks test above.
  const char kFooId[] = "hhgbjpmdppecanaaogonaigmmifgpaph";
  const char kBarId[] = "odlhccgofgkadkkhcmhgnhgahonahoca";
  const char kEphemeralId[] = "opoomfdlbjcbjinalcjdjfoiikdeaoel";

  // Foo.app can handle ".txt" and ".html".
  // This one is an extension, and has "file_browser_handlers"
  extensions::ExtensionBuilder foo_app;
  foo_app.SetManifest(extensions::DictionaryBuilder()
                      .Set("name", "Foo")
                      .Set("version", "1.0.0")
                      .Set("manifest_version", 2)
                      .Set("file_browser_handlers",
                           extensions::ListBuilder()
                           .Append(extensions::DictionaryBuilder()
                                   .Set("id", "open")
                                   .Set("default_title", "open")
                                   .Set("file_filters",
                                        extensions::ListBuilder()
                                        .Append("filesystem:*.txt")
                                        .Append("filesystem:*.html")))));
  foo_app.SetID(kFooId);
  extension_service_->AddExtension(foo_app.Build().get());

  // Bar.app can only handle ".txt".
  extensions::ExtensionBuilder bar_app;
  bar_app.SetManifest(extensions::DictionaryBuilder()
                      .Set("name", "Bar")
                      .Set("version", "1.0.0")
                      .Set("manifest_version", 2)
                      .Set("file_browser_handlers",
                           extensions::ListBuilder()
                           .Append(extensions::DictionaryBuilder()
                                   .Set("id", "open")
                                   .Set("default_title", "open")
                                   .Set("file_filters",
                                        extensions::ListBuilder()
                                        .Append("filesystem:*.txt")))));
  bar_app.SetID(kBarId);
  extension_service_->AddExtension(bar_app.Build().get());

  // Ephemeral.app is an ephemeral app that can handle ".txt".
  // It should not ever be found as ephemeral apps cannot be file browser
  // handlers.
  extensions::ExtensionBuilder ephemeral_app;
  ephemeral_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Ephemeral")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("file_browser_handlers",
               extensions::ListBuilder().Append(
                   extensions::DictionaryBuilder()
                       .Set("id", "open")
                       .Set("default_title", "open")
                       .Set("file_filters",
                            extensions::ListBuilder().Append(
                                "filesystem:*.txt")))));
  ephemeral_app.SetID(kEphemeralId);
  scoped_refptr<extensions::Extension> built_ephemeral_app(
      ephemeral_app.Build());
  extension_service_->AddExtension(built_ephemeral_app.get());
  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(&test_profile_);
  extension_prefs->OnExtensionInstalled(built_ephemeral_app.get(),
                                        extensions::Extension::ENABLED,
                                        syncer::StringOrdinal(),
                                        extensions::kInstallFlagIsEphemeral,
                                        std::string());

  // Find apps for a ".txt" file. Foo.app and Bar.app should be found.
  std::vector<GURL> file_urls;
  file_urls.push_back(GURL("filesystem:chrome-extension://id/dir/foo.txt"));

  std::vector<FullTaskDescriptor> tasks;
  FindFileBrowserHandlerTasks(&test_profile_, file_urls, &tasks);
  ASSERT_EQ(2U, tasks.size());
  // Sort the app IDs, as the order is not guaranteed.
  std::vector<std::string> app_ids;
  app_ids.push_back(tasks[0].task_descriptor().app_id);
  app_ids.push_back(tasks[1].task_descriptor().app_id);
  std::sort(app_ids.begin(), app_ids.end());
  // Confirm that both Foo.app and Bar.app are found.
  EXPECT_EQ(kFooId, app_ids[0]);
  EXPECT_EQ(kBarId, app_ids[1]);

  // Find apps for ".txt" and ".html" files. Only Foo.app should be found.
  file_urls.clear();
  file_urls.push_back(GURL("filesystem:chrome-extension://id/dir/foo.txt"));
  file_urls.push_back(GURL("filesystem:chrome-extension://id/dir/foo.html"));
  tasks.clear();
  FindFileBrowserHandlerTasks(&test_profile_, file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  // Confirm that only Foo.app is found.
  EXPECT_EQ(kFooId, tasks[0].task_descriptor().app_id);

  // Add an ".png" file. No tasks should be found.
  file_urls.push_back(GURL("filesystem:chrome-extension://id/dir/foo.png"));
  tasks.clear();
  FindFileBrowserHandlerTasks(&test_profile_, file_urls, &tasks);
  // Confirm no tasks are found.
  ASSERT_TRUE(tasks.empty());
}

// Test that all kinds of apps (file handler, file browser handler, and Drive
// app) are returned.
TEST_F(FileManagerFileTasksComplexTest, FindAllTypesOfTasks) {
  // kFooId and kBarId copied from FindFileHandlerTasks test above.
  const char kFooId[] = "hhgbjpmdppecanaaogonaigmmifgpaph";
  const char kBarId[] = "odlhccgofgkadkkhcmhgnhgahonahoca";
  const char kBazId[] = "plifkpkakemokpflgbnnigcoldgcbdmc";

  // Foo.app can handle "text/plain".
  // This is a packaged app (file handler).
  extensions::ExtensionBuilder foo_app;
  foo_app.SetManifest(extensions::DictionaryBuilder()
                      .Set("name", "Foo")
                      .Set("version", "1.0.0")
                      .Set("manifest_version", 2)
                      .Set("app",
                           extensions::DictionaryBuilder()
                           .Set("background",
                                extensions::DictionaryBuilder()
                                .Set("scripts",
                                     extensions::ListBuilder()
                                     .Append("background.js"))))
                      .Set("file_handlers",
                           extensions::DictionaryBuilder()
                           .Set("text",
                                extensions::DictionaryBuilder()
                                .Set("title", "Text")
                                .Set("types",
                                     extensions::ListBuilder()
                                     .Append("text/plain")))));
  foo_app.SetID(kFooId);
  extension_service_->AddExtension(foo_app.Build().get());

  // Bar.app can only handle ".txt".
  // This is an extension (file browser handler).
  extensions::ExtensionBuilder bar_app;
  bar_app.SetManifest(extensions::DictionaryBuilder()
                      .Set("name", "Bar")
                      .Set("version", "1.0.0")
                      .Set("manifest_version", 2)
                      .Set("file_browser_handlers",
                           extensions::ListBuilder()
                           .Append(extensions::DictionaryBuilder()
                                   .Set("id", "open")
                                   .Set("default_title", "open")
                                   .Set("file_filters",
                                        extensions::ListBuilder()
                                        .Append("filesystem:*.txt")))));
  bar_app.SetID(kBarId);
  extension_service_->AddExtension(bar_app.Build().get());

  // Baz.app can handle "text/plain".
  // This is a Drive app.
  scoped_ptr<google_apis::AppResource> baz_app(new google_apis::AppResource);
  baz_app->set_product_id("baz_app_id");
  baz_app->set_application_id(kBazId);
  baz_app->set_name("Baz");
  baz_app->set_object_type("baz_object_type");
  ScopedVector<std::string> baz_mime_types;
  baz_mime_types.push_back(new std::string("text/plain"));
  baz_app->set_primary_mimetypes(baz_mime_types.Pass());
  // Set up DriveAppRegistry.
  ScopedVector<google_apis::AppResource> app_resources;
  app_resources.push_back(baz_app.release());
  google_apis::AppList app_list;
  app_list.set_items(app_resources.Pass());
  drive::DriveAppRegistry drive_app_registry(NULL);
  drive_app_registry.UpdateFromAppList(app_list);

  // Find apps for "foo.txt". All apps should be found.
  PathAndMimeTypeSet path_mime_set;
  std::vector<GURL> file_urls;
  path_mime_set.insert(
      std::make_pair(
          drive::util::GetDriveMountPointPath(&test_profile_).AppendASCII(
              "foo.txt"),
          "text/plain"));
  file_urls.push_back(GURL("filesystem:chrome-extension://id/dir/foo.txt"));

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasks(&test_profile_,
                      &drive_app_registry,
                      path_mime_set,
                      file_urls,
                      &tasks);
  ASSERT_EQ(3U, tasks.size());

  // Sort the app IDs, as the order is not guaranteed.
  std::vector<std::string> app_ids;
  app_ids.push_back(tasks[0].task_descriptor().app_id);
  app_ids.push_back(tasks[1].task_descriptor().app_id);
  app_ids.push_back(tasks[2].task_descriptor().app_id);
  std::sort(app_ids.begin(), app_ids.end());
  // Confirm that all apps are found.
  EXPECT_EQ(kFooId, app_ids[0]);
  EXPECT_EQ(kBarId, app_ids[1]);
  EXPECT_EQ(kBazId, app_ids[2]);
}

TEST_F(FileManagerFileTasksComplexTest, FindAllTypesOfTasks_GoogleDocument) {
  // kFooId and kBarId copied from FindFileHandlerTasks test above.
  const char kFooId[] = "hhgbjpmdppecanaaogonaigmmifgpaph";
  const char kBarId[] = "odlhccgofgkadkkhcmhgnhgahonahoca";

  // Foo.app can handle ".gdoc" files.
  scoped_ptr<google_apis::AppResource> foo_app(new google_apis::AppResource);
  foo_app->set_product_id("foo_app");
  foo_app->set_application_id(kFooId);
  foo_app->set_name("Foo");
  foo_app->set_object_type("foo_object_type");
  ScopedVector<std::string> foo_extensions;
  foo_extensions.push_back(new std::string("gdoc"));  // Not ".gdoc"
  foo_app->set_primary_file_extensions(foo_extensions.Pass());

  // Prepare DriveAppRegistry from Foo.app.
  ScopedVector<google_apis::AppResource> app_resources;
  app_resources.push_back(foo_app.release());
  google_apis::AppList app_list;
  app_list.set_items(app_resources.Pass());
  drive::DriveAppRegistry drive_app_registry(NULL);
  drive_app_registry.UpdateFromAppList(app_list);

  // Bar.app can handle ".gdoc" files.
  // This is an extension (file browser handler).
  extensions::ExtensionBuilder bar_app;
  bar_app.SetManifest(extensions::DictionaryBuilder()
                      .Set("name", "Bar")
                      .Set("version", "1.0.0")
                      .Set("manifest_version", 2)
                      .Set("file_browser_handlers",
                           extensions::ListBuilder()
                           .Append(extensions::DictionaryBuilder()
                                   .Set("id", "open")
                                   .Set("default_title", "open")
                                   .Set("file_filters",
                                        extensions::ListBuilder()
                                        .Append("filesystem:*.gdoc")))));
  bar_app.SetID(kBarId);
  extension_service_->AddExtension(bar_app.Build().get());

  // Files.app can handle ".gdoc" files.
  // The ID "kFileManagerAppId" used here is precisely the one that identifies
  // the Chrome OS Files.app application.
  extensions::ExtensionBuilder files_app;
  files_app.SetManifest(extensions::DictionaryBuilder()
                       .Set("name", "Files")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2)
                       .Set("file_browser_handlers",
                            extensions::ListBuilder()
                            .Append(extensions::DictionaryBuilder()
                                    .Set("id", "open")
                                    .Set("default_title", "open")
                                    .Set("file_filters",
                                         extensions::ListBuilder()
                                         .Append("filesystem:*.gdoc")))));
  files_app.SetID(kFileManagerAppId);
  extension_service_->AddExtension(files_app.Build().get());

  // Find apps for a ".gdoc file". Only the built-in handler of Files.apps
  // should be found.
  PathAndMimeTypeSet path_mime_set;
  std::vector<GURL> file_urls;
  path_mime_set.insert(
      std::make_pair(
          drive::util::GetDriveMountPointPath(&test_profile_).AppendASCII(
              "foo.gdoc"),
          "application/vnd.google-apps.document"));
  file_urls.push_back(GURL("filesystem:chrome-extension://id/dir/foo.gdoc"));

  std::vector<FullTaskDescriptor> tasks;
  FindAllTypesOfTasks(&test_profile_,
                      &drive_app_registry,
                      path_mime_set,
                      file_urls,
                      &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kFileManagerAppId, tasks[0].task_descriptor().app_id);
}

}  // namespace file_tasks
}  // namespace file_manager.
