// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/features/base_feature_provider.h"
#include "chrome/common/extensions/features/feature.h"
#include "extensions/common/error_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

namespace extensions {

TEST_F(ExtensionManifestTest, BackgroundPermission) {
  LoadAndExpectError("background_permission.json",
                     errors::kBackgroundPermissionNeeded);
}

TEST_F(ExtensionManifestTest, BackgroundScripts) {
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("background_scripts.json", &error));
  ASSERT_TRUE(manifest.get());

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess(Manifest(manifest.get(), "")));
  ASSERT_TRUE(extension);
  EXPECT_EQ(2u, extension->background_scripts().size());
  EXPECT_EQ("foo.js", extension->background_scripts()[0u]);
  EXPECT_EQ("bar/baz.js", extension->background_scripts()[1u]);

  EXPECT_TRUE(extension->has_background_page());
  EXPECT_EQ(std::string("/") +
            extension_filenames::kGeneratedBackgroundPageFilename,
            extension->GetBackgroundURL().path());

  manifest->SetString("background_page", "monkey.html");
  LoadAndExpectError(Manifest(manifest.get(), ""),
                     errors::kInvalidBackgroundCombination);
}

TEST_F(ExtensionManifestTest, BackgroundPage) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("background_page.json"));
  ASSERT_TRUE(extension);
  EXPECT_EQ("/foo.html", extension->GetBackgroundURL().path());
  EXPECT_TRUE(extension->allow_background_js_access());

  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("background_page_legacy.json", &error));
  ASSERT_TRUE(manifest.get());
  extension = LoadAndExpectSuccess(Manifest(manifest.get(), ""));
  ASSERT_TRUE(extension);
  EXPECT_EQ("/foo.html", extension->GetBackgroundURL().path());

  manifest->SetInteger(keys::kManifestVersion, 2);
  LoadAndExpectWarning(
      Manifest(manifest.get(), ""),
      "'background_page' requires manifest version of 1 or lower.");
}

TEST_F(ExtensionManifestTest, BackgroundAllowNoJsAccess) {
  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("background_allow_no_js_access.json");
  ASSERT_TRUE(extension);
  EXPECT_FALSE(extension->allow_background_js_access());

  extension = LoadAndExpectSuccess("background_allow_no_js_access2.json");
  ASSERT_TRUE(extension);
  EXPECT_FALSE(extension->allow_background_js_access());
}

TEST_F(ExtensionManifestTest, BackgroundPageWebRequest) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  Feature::ScopedCurrentChannel current_channel(
      chrome::VersionInfo::CHANNEL_DEV);

  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("background_page.json", &error));
  ASSERT_TRUE(manifest.get());
  manifest->SetBoolean(keys::kBackgroundPersistent, false);
  manifest->SetInteger(keys::kManifestVersion, 2);
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess(Manifest(manifest.get(), "")));
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->has_lazy_background_page());

  ListValue* permissions = new ListValue();
  permissions->Append(Value::CreateStringValue("webRequest"));
  manifest->Set(keys::kPermissions, permissions);
  LoadAndExpectError(Manifest(manifest.get(), ""),
                     errors::kWebRequestConflictsWithLazyBackground);
}

}  // namespace extensions
