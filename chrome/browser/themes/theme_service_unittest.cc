// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/json/json_reader.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ThemeServiceTest : public ExtensionServiceTestBase {
 public:
  ThemeServiceTest() {}
  virtual ~ThemeServiceTest() {}

  scoped_refptr<extensions::Extension> MakeThemeExtension(base::FilePath path) {
    DictionaryValue source;
    source.SetString(extension_manifest_keys::kName, "theme");
    source.Set(extension_manifest_keys::kTheme, new DictionaryValue());
    source.SetString(extension_manifest_keys::kUpdateURL, "http://foo.com");
    source.SetString(extension_manifest_keys::kVersion, "0.0.0.0");
    std::string error;
    scoped_refptr<extensions::Extension> extension =
        extensions::Extension::Create(
            path, extensions::Manifest::EXTERNAL_PREF_DOWNLOAD,
            source, extensions::Extension::NO_FLAGS, &error);
    EXPECT_TRUE(extension);
    EXPECT_EQ("", error);
    return extension;
  }

  virtual void SetUp() {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }
};

// Installs then uninstalls a theme and makes sure that the ThemeService
// reverts to the default theme after the uninstall.
TEST_F(ThemeServiceTest, ThemeInstallUninstall) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  scoped_refptr<extensions::Extension> extension =
      MakeThemeExtension(temp_dir.path());
  service_->FinishInstallationForTest(extension);
  // Let ThemeService finish creating the theme pack.
  MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  EXPECT_EQ(extension->id(), theme_service->GetThemeID());

  // Now unload the extension, should revert to the default theme.
  service_->UnloadExtension(extension->id(),
                            extension_misc::UNLOAD_REASON_UNINSTALL);
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
}

// Upgrades a theme and ensures that the ThemeService does not revert to the
// default theme.
TEST_F(ThemeServiceTest, ThemeUpgrade) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  scoped_refptr<extensions::Extension> extension =
      MakeThemeExtension(temp_dir.path());
  service_->FinishInstallationForTest(extension);
  // Let ThemeService finish creating the theme pack.
  MessageLoop::current()->RunUntilIdle();
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  EXPECT_EQ(extension->id(), theme_service->GetThemeID());

  // Now unload the extension, should revert to the default theme.
  service_->UnloadExtension(extension->id(),
                            extension_misc::UNLOAD_REASON_UPDATE);
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
}

}; // namespace
