// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_tab.h"

#include "apps/saved_files_service.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

}  // namespace

using base::FilePath;
using testing::Contains;
using testing::Eq;

class AppInfoPermissionsTabTest : public testing::Test {
 protected:
  AppInfoPermissionsTabTest() : window_(NULL) {};

  scoped_ptr<base::DictionaryValue> ValidAppManifest() {
    return extensions::DictionaryBuilder()
        .Set("name", "Test App Name")
        .Set("version", "2.0")
        .Set("manifest_version", 2)
        .Set("app",
             extensions::DictionaryBuilder().Set(
                 "background",
                 extensions::DictionaryBuilder().Set(
                     "scripts",
                     extensions::ListBuilder().Append("background.js"))))
        .Build();
  }

  gfx::NativeWindow window_;
  TestingProfile profile_;

  // We need the UI thread in order to construct UI elements in the view.
  content::TestBrowserThreadBundle thread_bundle_;
};

// Tests that an app with no permissions is treated correctly.
TEST_F(AppInfoPermissionsTabTest, NoPermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsTab tab(window_, &profile_, app, base::Closure());

  EXPECT_TRUE(tab.GetRequiredPermissions()->IsEmpty());
  EXPECT_TRUE(tab.GetRequiredPermissionMessages().empty());

  EXPECT_TRUE(tab.GetOptionalPermissions()->IsEmpty());
  EXPECT_TRUE(tab.GetOptionalPermissionMessages().empty());

  EXPECT_TRUE(tab.GetRetainedFilePermissions().empty());
  EXPECT_TRUE(tab.GetRetainedFilePermissionMessages().empty());
}

// Tests that an app's required permissions are detected and converted to
// messages correctly.
TEST_F(AppInfoPermissionsTabTest, RequiredPermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .MergeManifest(extensions::DictionaryBuilder().Set(
              "permissions",
              extensions::ListBuilder()
                  .Append("desktopCapture")  // A valid permission with a
                                             // message
                  .Append("bad_perm")        // An invalid permission
                  .Append("notifications")   // An valid permission with
                                             // no message
                  .Append("serial")))        // Another valid permission with
                                             // a message
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsTab tab(window_, &profile_, app, base::Closure());

  const extensions::PermissionSet* required_permissions =
      tab.GetRequiredPermissions();
  EXPECT_FALSE(required_permissions->IsEmpty());
  EXPECT_EQ(3U, required_permissions->GetAPIsAsStrings().size());

  EXPECT_TRUE(tab.GetOptionalPermissions()->IsEmpty());
  EXPECT_TRUE(tab.GetRetainedFilePermissions().empty());

  const std::vector<base::string16> required_permission_messages =
      tab.GetRequiredPermissionMessages();
  ASSERT_EQ(2U, required_permission_messages.size());
  EXPECT_EQ(
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_DESKTOP_CAPTURE),
      base::UTF16ToUTF8(required_permission_messages[0]));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_SERIAL),
            base::UTF16ToUTF8(required_permission_messages[1]));
}

// Tests that an app's optional permissions are detected and converted to
// messages correctly.
TEST_F(AppInfoPermissionsTabTest, OptionalPermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .MergeManifest(extensions::DictionaryBuilder().Set(
              "optional_permissions",
              extensions::ListBuilder()
                  .Append("clipboardRead")  // A valid permission with a
                                            // message
                  .Append("bad_perm")       // An invalid permission
                  .Append("idle")           // A valid permission with
                                            // no message
                  .Append("serial")))       // Another valid permission with
                                            // a message
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsTab tab(window_, &profile_, app, base::Closure());

  const extensions::PermissionSet* optional_permissions =
      tab.GetOptionalPermissions();
  EXPECT_FALSE(optional_permissions->IsEmpty());
  EXPECT_EQ(3U, optional_permissions->GetAPIsAsStrings().size());

  EXPECT_TRUE(tab.GetRequiredPermissions()->IsEmpty());
  EXPECT_TRUE(tab.GetRetainedFilePermissions().empty());

  const std::vector<base::string16> optional_permission_messages =
      tab.GetOptionalPermissionMessages();
  ASSERT_EQ(2U, optional_permission_messages.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD),
            base::UTF16ToUTF8(optional_permission_messages[0]));
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_SERIAL),
            base::UTF16ToUTF8(optional_permission_messages[1]));
}

// Tests that an app's retained files are detected and converted to paths
// correctly.
TEST_F(AppInfoPermissionsTabTest, RetainedFilePermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .MergeManifest(extensions::DictionaryBuilder().Set(
              "permissions",
              extensions::ListBuilder().Append(
                  extensions::DictionaryBuilder().Set(
                      "fileSystem",
                      extensions::ListBuilder().Append("retainEntries")))))
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsTab tab(window_, &profile_, app, base::Closure());

  apps::SavedFilesService* files_service =
      apps::SavedFilesService::Get(&profile_);
  files_service->RegisterFileEntry(
      app->id(), "file_id_1", FilePath(FILE_PATH_LITERAL("file_1.ext")), false);
  files_service->RegisterFileEntry(
      app->id(), "file_id_2", FilePath(FILE_PATH_LITERAL("file_2.ext")), false);
  files_service->RegisterFileEntry(
      app->id(), "file_id_3", FilePath(FILE_PATH_LITERAL("file_3.ext")), false);

  // There should be 2 required permissions: fileSystem and
  // fileSystem.retainEntries.
  const extensions::PermissionSet* required_permissions =
      tab.GetRequiredPermissions();
  EXPECT_FALSE(required_permissions->IsEmpty());
  EXPECT_EQ(2U, required_permissions->GetAPIsAsStrings().size());

  EXPECT_TRUE(tab.GetOptionalPermissions()->IsEmpty());

  // Convert the list of FilePaths into a list of StringTypes for comparisons
  // using Contains.
  const std::vector<FilePath> retained_files = tab.GetRetainedFilePermissions();
  std::vector<FilePath::StringType> retained_file_paths;
  for (size_t i = 0; i < retained_files.size(); ++i)
    retained_file_paths.push_back(retained_files[i].value());

  // Since we have no guarantees on the order of retained files, make sure the
  // list is the expected length and all required entries are present.
  ASSERT_EQ(3U, retained_files.size());
  EXPECT_THAT(
      retained_file_paths,
      Contains(Eq(FilePath::StringType(FILE_PATH_LITERAL("file_1.ext")))));
  EXPECT_THAT(
      retained_file_paths,
      Contains(Eq(FilePath::StringType(FILE_PATH_LITERAL("file_2.ext")))));
  EXPECT_THAT(
      retained_file_paths,
      Contains(Eq(FilePath::StringType(FILE_PATH_LITERAL("file_3.ext")))));

  const std::vector<base::string16> retained_file_messages =
      tab.GetRetainedFilePermissionMessages();
  ASSERT_EQ(3U, retained_file_messages.size());
  EXPECT_THAT(retained_file_messages,
              Contains(Eq(base::UTF8ToUTF16("file_1.ext"))));
  EXPECT_THAT(retained_file_messages,
              Contains(Eq(base::UTF8ToUTF16("file_2.ext"))));
  EXPECT_THAT(retained_file_messages,
              Contains(Eq(base::UTF8ToUTF16("file_3.ext"))));
}
