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

  scoped_refptr<extensions::Extension> MakeThemeExtension(FilePath path) {
    DictionaryValue source;
    source.SetString(extension_manifest_keys::kName, "theme");
    source.Set(extension_manifest_keys::kTheme, new DictionaryValue());
    source.SetString(extension_manifest_keys::kUpdateURL, "http://foo.com");
    source.SetString(extension_manifest_keys::kVersion, "0.0.0.0");
    std::string error;
    scoped_refptr<extensions::Extension> extension =
        extensions::Extension::Create(
            path, extensions::Extension::EXTERNAL_PREF_DOWNLOAD,
            source, extensions::Extension::NO_FLAGS, &error);
    EXPECT_TRUE(extension);
    EXPECT_EQ("", error);
    return extension;
  }

  void SetUp() {
    InitializeEmptyExtensionService();
  }
};

TEST_F(ThemeServiceTest, AlignmentConversion) {
  // Verify that we get out what we put in.
  std::string top_left = "left top";
  int alignment = ThemeService::StringToAlignment(top_left);
  EXPECT_EQ(ThemeService::ALIGN_TOP | ThemeService::ALIGN_LEFT,
            alignment);
  EXPECT_EQ(top_left, ThemeService::AlignmentToString(alignment));

  // We get back a normalized version of what we put in.
  alignment = ThemeService::StringToAlignment("top");
  EXPECT_EQ(ThemeService::ALIGN_TOP, alignment);
  EXPECT_EQ("center top", ThemeService::AlignmentToString(alignment));

  alignment = ThemeService::StringToAlignment("left");
  EXPECT_EQ(ThemeService::ALIGN_LEFT, alignment);
  EXPECT_EQ("left center", ThemeService::AlignmentToString(alignment));

  alignment = ThemeService::StringToAlignment("right");
  EXPECT_EQ(ThemeService::ALIGN_RIGHT, alignment);
  EXPECT_EQ("right center", ThemeService::AlignmentToString(alignment));

  alignment = ThemeService::StringToAlignment("righttopbottom");
  EXPECT_EQ(ThemeService::ALIGN_CENTER, alignment);
  EXPECT_EQ("center center", ThemeService::AlignmentToString(alignment));
}

TEST_F(ThemeServiceTest, AlignmentConversionInput) {
  // Verify that we output in an expected format.
  int alignment = ThemeService::StringToAlignment("bottom right");
  EXPECT_EQ("right bottom", ThemeService::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeService::StringToAlignment("new zealand");
  EXPECT_EQ("center center", ThemeService::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeService::StringToAlignment("new zealand top");
  EXPECT_EQ("center top", ThemeService::AlignmentToString(alignment));

  // Verify that bad strings don't cause explosions.
  alignment = ThemeService::StringToAlignment("new zealandtop");
  EXPECT_EQ("center center", ThemeService::AlignmentToString(alignment));
}

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
