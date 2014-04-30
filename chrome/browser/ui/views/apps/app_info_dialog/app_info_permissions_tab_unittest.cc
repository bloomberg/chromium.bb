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
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

class AppInfoPermissionsTabTest : public testing::Test {
 protected:
  AppInfoPermissionsTabTest() : window(NULL) {};

  gfx::NativeWindow window;
  TestingProfile profile;

  // We need the UI thread in order to construct UI elements in the view.
  content::TestBrowserThreadBundle thread_bundle;
};

// Tests that an app with no permissions is treated correctly.
TEST_F(AppInfoPermissionsTabTest, NoPermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(
               extensions::DictionaryBuilder().Set("name", "Test App Name").Set(
                   "version", "2.0"))
          .SetID("cedabbhfglmiikkmdgcpjdkocfcmbkee")
          .Build();
  AppInfoPermissionsTab tab(window, &profile, app, base::Closure());

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
          .SetManifest(
               extensions::DictionaryBuilder()
                   .Set("name", "Test App Name")
                   .Set("version", "2.0")
                   .Set("permissions",
                        extensions::ListBuilder()
                            .Append("location")  // A valid permission with a
                                                 // message
                            .Append("bad_perm")  // An invalid permission
                            .Append("browsingData")  // An valid permission with
                                                     // no message
                            .Append("tabs")))  // Another valid permission with
                                               // a message
          .SetID("cedabbhfglmiikkmdgcpjdkocfcmbkee")
          .Build();
  AppInfoPermissionsTab tab(window, &profile, app, base::Closure());

  const extensions::PermissionSet* required_permissions =
      tab.GetRequiredPermissions();
  EXPECT_FALSE(required_permissions->IsEmpty());
  EXPECT_EQ((size_t)3, required_permissions->GetAPIsAsStrings().size());

  EXPECT_TRUE(tab.GetOptionalPermissions()->IsEmpty());
  EXPECT_TRUE(tab.GetRetainedFilePermissions().empty());

  const std::vector<base::string16> required_permission_messages =
      tab.GetRequiredPermissionMessages();
  EXPECT_EQ((size_t)2, required_permission_messages.size());
  ASSERT_STREQ(l10n_util::GetStringUTF8(
                   IDS_EXTENSION_PROMPT_WARNING_GEOLOCATION).c_str(),
               base::UTF16ToUTF8(required_permission_messages[0]).c_str());
  ASSERT_STREQ(
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_TABS).c_str(),
      base::UTF16ToUTF8(required_permission_messages[1]).c_str());
}

// Tests that an app's optional permissions are detected and converted to
// messages correctly.
TEST_F(AppInfoPermissionsTabTest, OptionalPermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(
               extensions::DictionaryBuilder()
                   .Set("name", "Test App Name")
                   .Set("version", "2.0")
                   .Set("optional_permissions",
                        extensions::ListBuilder()
                            .Append("bookmarks")  // A valid permission with a
                                                  // message
                            .Append("bad_perm")   // An invalid permission
                            .Append("cookies")    // A valid permission with
                                                  // no message
                            .Append("tabs")))  // Another valid permission with
                                               // a message
          .SetID("cedabbhfglmiikkmdgcpjdkocfcmbkee")
          .Build();
  AppInfoPermissionsTab tab(window, &profile, app, base::Closure());

  const extensions::PermissionSet* optional_permissions =
      tab.GetOptionalPermissions();
  EXPECT_FALSE(optional_permissions->IsEmpty());
  EXPECT_EQ((size_t)3, optional_permissions->GetAPIsAsStrings().size());

  EXPECT_TRUE(tab.GetRequiredPermissions()->IsEmpty());
  EXPECT_TRUE(tab.GetRetainedFilePermissions().empty());

  const std::vector<base::string16> optional_permission_messages =
      tab.GetOptionalPermissionMessages();
  EXPECT_EQ((size_t)2, optional_permission_messages.size());
  ASSERT_STREQ(
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_BOOKMARKS).c_str(),
      base::UTF16ToUTF8(optional_permission_messages[0]).c_str());
  ASSERT_STREQ(
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_TABS).c_str(),
      base::UTF16ToUTF8(optional_permission_messages[1]).c_str());
}

// Tests that an app's retained files are detected and converted to paths
// correctly.
TEST_F(AppInfoPermissionsTabTest, RetainedFilePermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", "Test App Name")
                           .Set("version", "2.0")
                           .Set("manifest_version", 2)
                           .Set("app",
                                extensions::DictionaryBuilder().Set(
                                    "background",
                                    extensions::DictionaryBuilder().Set(
                                        "scripts",
                                        extensions::ListBuilder().Append(
                                            "background.js"))))
                           .Set("permissions",
                                extensions::ListBuilder().Append(
                                    extensions::DictionaryBuilder().Set(
                                        "fileSystem",
                                        extensions::ListBuilder().Append(
                                            "retainEntries")))))
          .SetID("cedabbhfglmiikkmdgcpjdkocfcmbkee")
          .Build();
  AppInfoPermissionsTab tab(window, &profile, app, base::Closure());

  apps::SavedFilesService* files_service =
      apps::SavedFilesService::Get(&profile);
  files_service->RegisterFileEntry(
      app->id(),
      "file_id_1",
      base::FilePath(FILE_PATH_LITERAL("file_1.ext")),
      false);
  files_service->RegisterFileEntry(
      app->id(),
      "file_id_2",
      base::FilePath(FILE_PATH_LITERAL("file_2.ext")),
      false);
  files_service->RegisterFileEntry(
      app->id(),
      "file_id_3",
      base::FilePath(FILE_PATH_LITERAL("file_3.ext")),
      false);

  // There should be 2 required permissions: fileSystem and
  // fileSystem.retainEntries.
  const extensions::PermissionSet* required_permissions =
      tab.GetRequiredPermissions();
  EXPECT_FALSE(required_permissions->IsEmpty());
  EXPECT_EQ((size_t)2, required_permissions->GetAPIsAsStrings().size());

  EXPECT_TRUE(tab.GetOptionalPermissions()->IsEmpty());

  const std::vector<base::FilePath> retained_files =
      tab.GetRetainedFilePermissions();
  EXPECT_EQ((size_t)3, retained_files.size());
  ASSERT_EQ(base::FilePath::StringType(FILE_PATH_LITERAL("file_1.ext")),
            retained_files[0].value());
  ASSERT_EQ(base::FilePath::StringType(FILE_PATH_LITERAL("file_2.ext")),
            retained_files[1].value());
  ASSERT_EQ(base::FilePath::StringType(FILE_PATH_LITERAL("file_3.ext")),
            retained_files[2].value());

  const std::vector<base::string16> retained_file_messages =
      tab.GetRetainedFilePermissionMessages();
  EXPECT_EQ((size_t)3, retained_file_messages.size());
  ASSERT_STREQ("file_1.ext",
               base::UTF16ToUTF8(retained_file_messages[0]).c_str());
  ASSERT_STREQ("file_2.ext",
               base::UTF16ToUTF8(retained_file_messages[1]).c_str());
  ASSERT_STREQ("file_3.ext",
               base::UTF16ToUTF8(retained_file_messages[2]).c_str());
}
