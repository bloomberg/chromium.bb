// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "components/version_info/version_info.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

class ExtensionManifestBackgroundTest : public ChromeManifestTest {
};

TEST_F(ExtensionManifestBackgroundTest, BackgroundPermission) {
  LoadAndExpectError("background_permission.json",
                     errors::kBackgroundPermissionNeeded);
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundScripts) {
  std::string error;
  std::unique_ptr<base::DictionaryValue> manifest =
      LoadManifest("background_scripts.json", &error);
  ASSERT_TRUE(manifest.get());

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess(ManifestData(manifest.get(), "")));
  ASSERT_TRUE(extension.get());
  const std::vector<std::string>& background_scripts =
      BackgroundInfo::GetBackgroundScripts(extension.get());
  ASSERT_EQ(2u, background_scripts.size());
  EXPECT_EQ("foo.js", background_scripts[0u]);
  EXPECT_EQ("bar/baz.js", background_scripts[1u]);

  EXPECT_TRUE(BackgroundInfo::HasBackgroundPage(extension.get()));
  EXPECT_EQ(
      std::string("/") + kGeneratedBackgroundPageFilename,
      BackgroundInfo::GetBackgroundURL(extension.get()).path());

  manifest->SetString("background_page", "monkey.html");
  LoadAndExpectError(ManifestData(manifest.get(), ""),
                     errors::kInvalidBackgroundCombination);
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundPage) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("background_page.json"));
  ASSERT_TRUE(extension.get());
  EXPECT_EQ("/foo.html",
            BackgroundInfo::GetBackgroundURL(extension.get()).path());
  EXPECT_TRUE(BackgroundInfo::AllowJSAccess(extension.get()));

  std::string error;
  std::unique_ptr<base::DictionaryValue> manifest(
      LoadManifest("background_page_legacy.json", &error));
  ASSERT_TRUE(manifest.get());
  extension = LoadAndExpectSuccess(ManifestData(manifest.get(), ""));
  ASSERT_TRUE(extension.get());
  EXPECT_EQ("/foo.html",
            BackgroundInfo::GetBackgroundURL(extension.get()).path());

  manifest->SetInteger(keys::kManifestVersion, 2);
  LoadAndExpectWarning(
      ManifestData(manifest.get(), ""),
      "'background_page' requires manifest version of 1 or lower.");
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundAllowNoJsAccess) {
  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("background_allow_no_js_access.json");
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(BackgroundInfo::AllowJSAccess(extension.get()));

  extension = LoadAndExpectSuccess("background_allow_no_js_access2.json");
  ASSERT_TRUE(extension.get());
  EXPECT_FALSE(BackgroundInfo::AllowJSAccess(extension.get()));
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundPageWebRequest) {
  ScopedCurrentChannel current_channel(version_info::Channel::DEV);

  std::string error;
  std::unique_ptr<base::DictionaryValue> manifest(
      LoadManifest("background_page.json", &error));
  ASSERT_TRUE(manifest.get());
  manifest->SetBoolean(keys::kBackgroundPersistent, false);
  manifest->SetInteger(keys::kManifestVersion, 2);
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess(ManifestData(manifest.get(), "")));
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(BackgroundInfo::HasLazyBackgroundPage(extension.get()));

  auto permissions = std::make_unique<base::ListValue>();
  permissions->AppendString("webRequest");
  manifest->Set(keys::kPermissions, std::move(permissions));
  LoadAndExpectError(ManifestData(manifest.get(), ""),
                     errors::kWebRequestConflictsWithLazyBackground);
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundPagePersistentPlatformApp) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("background_page_persistent_app.json");
  ASSERT_TRUE(extension->is_platform_app());
  ASSERT_TRUE(BackgroundInfo::HasBackgroundPage(extension.get()));
  EXPECT_FALSE(BackgroundInfo::HasPersistentBackgroundPage(extension.get()));

  std::string error;
  std::vector<InstallWarning> warnings;
  ManifestHandler::ValidateExtension(extension.get(), &error, &warnings);
  // Persistent background pages are not supported for packaged apps.
  // The persistent flag is ignored and a warining is printed.
  EXPECT_EQ(1U, warnings.size());
  EXPECT_EQ(errors::kInvalidBackgroundPersistentInPlatformApp,
            warnings[0].message);
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundPagePersistentInvalidKey) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("background_page_invalid_persistent_key_app.json");
  ASSERT_TRUE(extension->is_platform_app());
  ASSERT_TRUE(BackgroundInfo::HasBackgroundPage(extension.get()));
  EXPECT_FALSE(BackgroundInfo::HasPersistentBackgroundPage(extension.get()));

  std::string error;
  std::vector<InstallWarning> warnings;
  ManifestHandler::ValidateExtension(extension.get(), &error, &warnings);
  // The key 'background.persistent' is not supported for packaged apps.
  EXPECT_EQ(1U, warnings.size());
  EXPECT_EQ(errors::kBackgroundPersistentInvalidForPlatformApps,
            warnings[0].message);
}

}  // namespace extensions
