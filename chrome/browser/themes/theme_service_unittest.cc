// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::ExtensionRegistry;

namespace theme_service_internal {

class ThemeServiceTest : public extensions::ExtensionServiceTestBase {
 public:
  ThemeServiceTest() : is_supervised_(false),
                       registry_(NULL) {}
  virtual ~ThemeServiceTest() {}

  // Moves a minimal theme to |temp_dir_path| and unpacks it from that
  // directory.
  std::string LoadUnpackedThemeAt(const base::FilePath& temp_dir) {
    base::FilePath dst_manifest_path = temp_dir.AppendASCII("manifest.json");
    base::FilePath test_data_dir;
    EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    base::FilePath src_manifest_path =
        test_data_dir.AppendASCII("extensions/theme_minimal/manifest.json");
    EXPECT_TRUE(base::CopyFile(src_manifest_path, dst_manifest_path));

    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service_));
    content::WindowedNotificationObserver observer(
        extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
        content::Source<Profile>(profile_.get()));
    installer->Load(temp_dir);
    observer.Wait();

    std::string extension_id =
        content::Details<extensions::Extension>(observer.details())->id();

    // Let the ThemeService finish creating the theme pack.
    base::MessageLoop::current()->RunUntilIdle();

    return extension_id;
  }

  // Update the theme with |extension_id|.
  void UpdateUnpackedTheme(const std::string& extension_id) {
    int updated_notification =
        service_->IsExtensionEnabled(extension_id)
            ? extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED
            : extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED;

    const base::FilePath& path =
        service_->GetInstalledExtension(extension_id)->path();

    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service_));
    content::WindowedNotificationObserver observer(updated_notification,
        content::Source<Profile>(profile_.get()));
    installer->Load(path);
    observer.Wait();

    // Let the ThemeService finish creating the theme pack.
    base::MessageLoop::current()->RunUntilIdle();
  }

  virtual void SetUp() {
    extensions::ExtensionServiceTestBase::SetUp();
    extensions::ExtensionServiceTestBase::ExtensionServiceInitParams params =
        CreateDefaultInitParams();
    params.profile_is_supervised = is_supervised_;
    InitializeExtensionService(params);
    service_->Init();
    registry_ = ExtensionRegistry::Get(profile_.get());
    ASSERT_TRUE(registry_);
  }

  const CustomThemeSupplier* get_theme_supplier(ThemeService* theme_service) {
    return theme_service->get_theme_supplier();
  }

 protected:
  bool is_supervised_;
  ExtensionRegistry* registry_;

};

// Installs then uninstalls a theme and makes sure that the ThemeService
// reverts to the default theme after the uninstall.
TEST_F(ThemeServiceTest, ThemeInstallUninstall) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::MessageLoop::current()->RunUntilIdle();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const std::string& extension_id = LoadUnpackedThemeAt(temp_dir.path());
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  EXPECT_EQ(extension_id, theme_service->GetThemeID());

  // Now uninstall the extension, should revert to the default theme.
  service_->UninstallExtension(extension_id,
                               extensions::UNINSTALL_REASON_FOR_TESTING,
                               base::Bind(&base::DoNothing),
                               NULL);
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
}

// Test that a theme extension is disabled when not in use. A theme may be
// installed but not in use if it there is an infobar to revert to the previous
// theme.
TEST_F(ThemeServiceTest, DisableUnusedTheme) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::MessageLoop::current()->RunUntilIdle();

  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  // 1) Installing a theme should disable the previously active theme.
  const std::string& extension1_id = LoadUnpackedThemeAt(temp_dir1.path());
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  EXPECT_EQ(extension1_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension1_id));

  // Show an infobar to prevent the current theme from being uninstalled.
  theme_service->OnInfobarDisplayed();

  const std::string& extension2_id = LoadUnpackedThemeAt(temp_dir2.path());
  EXPECT_EQ(extension2_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension2_id));
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));

  // 2) Enabling a disabled theme extension should swap the current theme.
  service_->EnableExtension(extension1_id);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(extension1_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension1_id));
  EXPECT_TRUE(registry_->GetExtensionById(extension2_id,
                                          ExtensionRegistry::DISABLED));

  // 3) Using SetTheme() with a disabled theme should enable and set the
  // theme. This is the case when the user reverts to the previous theme
  // via an infobar.
  const extensions::Extension* extension2 =
      service_->GetInstalledExtension(extension2_id);
  theme_service->SetTheme(extension2);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(extension2_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension2_id));
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));

  // 4) Disabling the current theme extension should revert to the default theme
  // and uninstall any installed theme extensions.
  theme_service->OnInfobarDestroyed();
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  service_->DisableExtension(extension2_id,
      extensions::Extension::DISABLE_USER_ACTION);
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
  EXPECT_FALSE(service_->GetInstalledExtension(extension1_id));
  EXPECT_FALSE(service_->GetInstalledExtension(extension2_id));
}

// Test the ThemeService's behavior when a theme is upgraded.
TEST_F(ThemeServiceTest, ThemeUpgrade) {
  // Setup.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::MessageLoop::current()->RunUntilIdle();

  theme_service->OnInfobarDisplayed();

  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  const std::string& extension1_id = LoadUnpackedThemeAt(temp_dir1.path());
  const std::string& extension2_id = LoadUnpackedThemeAt(temp_dir2.path());

  // Test the initial state.
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));
  EXPECT_EQ(extension2_id, theme_service->GetThemeID());

  // 1) Upgrading the current theme should not revert to the default theme.
  content::WindowedNotificationObserver theme_change_observer(
      chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(theme_service));
  UpdateUnpackedTheme(extension2_id);

  // The ThemeService should have sent an theme change notification even though
  // the id of the current theme did not change.
  theme_change_observer.Wait();

  EXPECT_EQ(extension2_id, theme_service->GetThemeID());
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));

  // 2) Upgrading a disabled theme should not change the current theme.
  UpdateUnpackedTheme(extension1_id);
  EXPECT_EQ(extension2_id, theme_service->GetThemeID());
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));
}

class ThemeServiceSupervisedUserTest : public ThemeServiceTest {
 public:
  ThemeServiceSupervisedUserTest() {}
  virtual ~ThemeServiceSupervisedUserTest() {}

  virtual void SetUp() OVERRIDE {
    is_supervised_ = true;
    ThemeServiceTest::SetUp();
  }
};

// Checks that supervised users have their own default theme.
TEST_F(ThemeServiceSupervisedUserTest,
       SupervisedUserThemeReplacesDefaultTheme) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
  EXPECT_TRUE(get_theme_supplier(theme_service));
  EXPECT_EQ(get_theme_supplier(theme_service)->get_theme_type(),
            CustomThemeSupplier::SUPERVISED_USER_THEME);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Checks that supervised users don't use the system theme even if it is the
// default. The system theme is only available on Linux.
TEST_F(ThemeServiceSupervisedUserTest, SupervisedUserThemeReplacesNativeTheme) {
  profile_->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, true);
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  EXPECT_TRUE(theme_service->UsingDefaultTheme());
  EXPECT_TRUE(get_theme_supplier(theme_service));
  EXPECT_EQ(get_theme_supplier(theme_service)->get_theme_type(),
            CustomThemeSupplier::SUPERVISED_USER_THEME);
}
#endif

}; // namespace theme_service_internal
