// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "chrome/common/extensions/features/feature.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "extensions/common/error_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

namespace extensions {

class ExtensionManifestBackgroundTest : public ExtensionManifestTest {
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    (new BackgroundManifestHandler)->Register();
  }
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
      LoadAndExpectSuccess(Manifest(manifest.get(), "")));
  ASSERT_TRUE(extension);
  const std::vector<std::string>& background_scripts =
      BackgroundInfo::GetBackgroundScripts(extension);
  ASSERT_EQ(2u, background_scripts.size());
  EXPECT_EQ("foo.js", background_scripts[0u]);
  EXPECT_EQ("bar/baz.js", background_scripts[1u]);

  EXPECT_TRUE(BackgroundInfo::HasBackgroundPage(extension));
  EXPECT_EQ(std::string("/") +
            extension_filenames::kGeneratedBackgroundPageFilename,
            BackgroundInfo::GetBackgroundURL(extension).path());

  manifest->SetString("background_page", "monkey.html");
  LoadAndExpectError(Manifest(manifest.get(), ""),
                     errors::kInvalidBackgroundCombination);
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundPage) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("background_page.json"));
  ASSERT_TRUE(extension);
  EXPECT_EQ("/foo.html", BackgroundInfo::GetBackgroundURL(extension).path());
  EXPECT_TRUE(BackgroundInfo::AllowJSAccess(extension));

  std::string error;
  scoped_ptr<base::DictionaryValue> manifest(
      LoadManifest("background_page_legacy.json", &error));
  ASSERT_TRUE(manifest.get());
  extension = LoadAndExpectSuccess(Manifest(manifest.get(), ""));
  ASSERT_TRUE(extension);
  EXPECT_EQ("/foo.html", BackgroundInfo::GetBackgroundURL(extension).path());

  manifest->SetInteger(keys::kManifestVersion, 2);
  LoadAndExpectWarning(
      Manifest(manifest.get(), ""),
      "'background_page' requires manifest version of 1 or lower.");
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundAllowNoJsAccess) {
  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("background_allow_no_js_access.json");
  ASSERT_TRUE(extension);
  EXPECT_FALSE(BackgroundInfo::AllowJSAccess(extension));

  extension = LoadAndExpectSuccess("background_allow_no_js_access2.json");
  ASSERT_TRUE(extension);
  EXPECT_FALSE(BackgroundInfo::AllowJSAccess(extension));
}

TEST_F(ExtensionManifestBackgroundTest, BackgroundPageWebRequest) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  Feature::ScopedCurrentChannel current_channel(
      chrome::VersionInfo::CHANNEL_DEV);

  std::string error;
  scoped_ptr<base::DictionaryValue> manifest(
      LoadManifest("background_page.json", &error));
  ASSERT_TRUE(manifest.get());
  manifest->SetBoolean(keys::kBackgroundPersistent, false);
  manifest->SetInteger(keys::kManifestVersion, 2);
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess(Manifest(manifest.get(), "")));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(BackgroundInfo::HasLazyBackgroundPage(extension));

  base::ListValue* permissions = new base::ListValue();
  permissions->Append(new base::StringValue("webRequest"));
  manifest->Set(keys::kPermissions, permissions);
  LoadAndExpectError(Manifest(manifest.get(), ""),
                     errors::kWebRequestConflictsWithLazyBackground);
}

}  // namespace extensions
