// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
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
  scoped_ptr<base::DictionaryValue> manifest(
      LoadManifest("background_scripts.json", &error));
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
  scoped_ptr<base::DictionaryValue> manifest(
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
  ScopedCurrentChannel current_channel(chrome::VersionInfo::CHANNEL_DEV);

  std::string error;
  scoped_ptr<base::DictionaryValue> manifest(
      LoadManifest("background_page.json", &error));
  ASSERT_TRUE(manifest.get());
  manifest->SetBoolean(keys::kBackgroundPersistent, false);
  manifest->SetInteger(keys::kManifestVersion, 2);
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess(ManifestData(manifest.get(), "")));
  ASSERT_TRUE(extension.get());
  EXPECT_TRUE(BackgroundInfo::HasLazyBackgroundPage(extension.get()));

  base::ListValue* permissions = new base::ListValue();
  permissions->Append(new base::StringValue("webRequest"));
  manifest->Set(keys::kPermissions, permissions);
  LoadAndExpectError(ManifestData(manifest.get(), ""),
                     errors::kWebRequestConflictsWithLazyBackground);
}

}  // namespace extensions
