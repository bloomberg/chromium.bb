// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_panel.h"

#include <utility>

#include "apps/saved_files_service.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message_test_util.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/extensions_strings.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kTestExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

}  // namespace

using base::FilePath;
using testing::Contains;
using testing::Eq;

class AppInfoPermissionsPanelTest : public testing::Test {
 protected:
  AppInfoPermissionsPanelTest() {}

  scoped_ptr<base::DictionaryValue> ValidAppManifest() {
    return extensions::DictionaryBuilder()
        .Set("name", "Test App Name")
        .Set("version", "2.0")
        .Set("manifest_version", 2)
        .Set("app",
             std::move(extensions::DictionaryBuilder().Set(
                 "background",
                 std::move(extensions::DictionaryBuilder().Set(
                     "scripts", std::move(extensions::ListBuilder().Append(
                                    "background.js")))))))
        .Build();
  }

  // We need the UI thread in order to construct UI elements in the view.
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

// Tests that an app with no permissions is treated correctly.
TEST_F(AppInfoPermissionsPanelTest, NoPermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsPanel panel(&profile_, app.get());

  EXPECT_TRUE(VerifyNoPermissionMessages(panel.app_->permissions_data()));
  EXPECT_TRUE(panel.GetRetainedFilePaths().empty());
}

// Tests that an app's required permissions are detected and converted to
// messages correctly.
TEST_F(AppInfoPermissionsPanelTest, RequiredPermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .MergeManifest(extensions::DictionaryBuilder().Set(
              "permissions",
              std::move(
                  extensions::ListBuilder()
                      .Append("desktopCapture")  // A valid permission with a
                                                 // message
                      .Append("bad_perm")        // An invalid permission
                      .Append("cookies")         // An valid permission with
                                                 // no message
                      .Append("serial"))))       // A valid permission with a
                                                 // message
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsPanel panel(&profile_, app.get());

  EXPECT_TRUE(VerifyTwoPermissionMessages(
      panel.app_->permissions_data(),
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_DESKTOP_CAPTURE),
      l10n_util::GetStringUTF8(IDS_EXTENSION_PROMPT_WARNING_SERIAL),
      false));
}

// Tests that an app's optional permissions are detected and converted to
// messages correctly.
TEST_F(AppInfoPermissionsPanelTest, OptionalPermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .MergeManifest(extensions::DictionaryBuilder().Set(
              "optional_permissions",
              std::move(
                  extensions::ListBuilder()
                      .Append("clipboardRead")  // A valid permission with a
                                                // message
                      .Append("bad_perm")       // An invalid permission
                      .Append("idle")           // A valid permission with
                                                // no message
                      .Append("serial"))))      // Another valid permission with
                                                // a message
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsPanel panel(&profile_, app.get());

  // Optional permissions don't appear until they are 'activated' at runtime.
  // TODO(sashab): Activate the optional permissions and ensure they are
  // successfully added to the dialog.
  EXPECT_TRUE(VerifyNoPermissionMessages(panel.app_->permissions_data()));
  EXPECT_TRUE(panel.GetRetainedFilePaths().empty());
}

// Tests that an app's retained files are detected and converted to paths
// correctly.
TEST_F(AppInfoPermissionsPanelTest, RetainedFilePermissionsObtainedCorrectly) {
  scoped_refptr<const extensions::Extension> app =
      extensions::ExtensionBuilder()
          .SetManifest(ValidAppManifest())
          .MergeManifest(extensions::DictionaryBuilder().Set(
              "permissions",
              std::move(extensions::ListBuilder().Append(
                  std::move(extensions::DictionaryBuilder().Set(
                      "fileSystem", std::move(extensions::ListBuilder().Append(
                                        "retainEntries"))))))))
          .SetID(kTestExtensionId)
          .Build();
  AppInfoPermissionsPanel panel(&profile_, app.get());
  apps::SavedFilesService* files_service =
      apps::SavedFilesService::Get(&profile_);
  files_service->RegisterFileEntry(
      app->id(), "file_id_1", FilePath(FILE_PATH_LITERAL("file_1.ext")), false);
  files_service->RegisterFileEntry(
      app->id(), "file_id_2", FilePath(FILE_PATH_LITERAL("file_2.ext")), false);
  files_service->RegisterFileEntry(
      app->id(), "file_id_3", FilePath(FILE_PATH_LITERAL("file_3.ext")), false);

  ASSERT_TRUE(VerifyNoPermissionMessages(panel.app_->permissions_data()));

  // Since we have no guarantees on the order of retained files, make sure the
  // list is the expected length and all required entries are present.
  const std::vector<base::string16> retained_file_paths =
      panel.GetRetainedFilePaths();
  ASSERT_EQ(3U, retained_file_paths.size());
  EXPECT_THAT(retained_file_paths,
              Contains(Eq(base::UTF8ToUTF16("file_1.ext"))));
  EXPECT_THAT(retained_file_paths,
              Contains(Eq(base::UTF8ToUTF16("file_2.ext"))));
  EXPECT_THAT(retained_file_paths,
              Contains(Eq(base::UTF8ToUTF16("file_3.ext"))));
}
