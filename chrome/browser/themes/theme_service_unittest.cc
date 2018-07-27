// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

using extensions::ExtensionRegistry;

namespace theme_service_internal {

class ThemeServiceTest : public extensions::ExtensionServiceTestBase {
 public:
  ThemeServiceTest() : is_supervised_(false),
                       registry_(NULL) {}
  ~ThemeServiceTest() override {}

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    extensions::ExtensionServiceTestBase::ExtensionServiceInitParams params =
        CreateDefaultInitParams();
    params.profile_is_supervised = is_supervised_;
    InitializeExtensionService(params);
    service_->Init();
    registry_ = ExtensionRegistry::Get(profile_.get());
    ASSERT_TRUE(registry_);
  }

  // Moves a minimal theme to |temp_dir_path| and unpacks it from that
  // directory.
  std::string LoadUnpackedThemeAt(const base::FilePath& temp_dir) {
    base::FilePath dst_manifest_path = temp_dir.AppendASCII("manifest.json");
    base::FilePath test_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    base::FilePath src_manifest_path =
        test_data_dir.AppendASCII("extensions/theme_minimal/manifest.json");
    EXPECT_TRUE(base::CopyFile(src_manifest_path, dst_manifest_path));

    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service_));
    extensions::TestExtensionRegistryObserver observer(
        ExtensionRegistry::Get(profile()));
    installer->Load(temp_dir);
    std::string extension_id = observer.WaitForExtensionLoaded()->id();

    WaitForThemeInstall();

    return extension_id;
  }

  // Update the theme with |extension_id|.
  void UpdateUnpackedTheme(const std::string& extension_id) {
    const base::FilePath& path =
        service_->GetInstalledExtension(extension_id)->path();

    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service_));
    if (service_->IsExtensionEnabled(extension_id)) {
      extensions::TestExtensionRegistryObserver observer(
          ExtensionRegistry::Get(profile()));
      installer->Load(path);
      observer.WaitForExtensionLoaded();
    } else {
      content::WindowedNotificationObserver observer(
          extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
          content::Source<Profile>(profile_.get()));
      installer->Load(path);
      observer.Wait();
    }

    // Let the ThemeService finish creating the theme pack.
    base::RunLoop().RunUntilIdle();
  }

  const CustomThemeSupplier* get_theme_supplier(ThemeService* theme_service) {
    return theme_service->get_theme_supplier();
  }

  void WaitForThemeInstall() {
    content::WindowedNotificationObserver theme_change_observer(
        chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
        content::Source<ThemeService>(
            ThemeServiceFactory::GetForProfile(profile())));
    theme_change_observer.Wait();
  }

  // Alpha blends a non-opaque foreground color against an opaque background.
  // This is not the same as color_utils::AlphaBlend() since it gets the opacity
  // from the foreground color and then does not blend the two colors' alpha
  // values together.
  static SkColor AlphaBlend(SkColor foreground, SkColor background) {
    return color_utils::AlphaBlend(SkColorSetA(foreground, SK_AlphaOPAQUE),
                                   background, SkColorGetA(foreground));
  }

  // Returns the separator color as the opaque result of blending it atop the
  // frame color (which is the color we use when calculating the contrast of the
  // separator with the tab and frame colors).
  static SkColor GetSeparatorColor(SkColor tab_color, SkColor frame_color) {
    return AlphaBlend(ThemeService::GetSeparatorColor(tab_color, frame_color),
                      frame_color);
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
  base::RunLoop().RunUntilIdle();

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const std::string& extension_id = LoadUnpackedThemeAt(temp_dir.GetPath());
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  EXPECT_EQ(extension_id, theme_service->GetThemeID());

  // Now uninstall the extension, should revert to the default theme.
  service_->UninstallExtension(extension_id,
                               extensions::UNINSTALL_REASON_FOR_TESTING,
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

  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  // 1) Installing a theme should disable the previously active theme.
  const std::string& extension1_id = LoadUnpackedThemeAt(temp_dir1.GetPath());
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  EXPECT_EQ(extension1_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension1_id));

  // Show an infobar to prevent the current theme from being uninstalled.
  theme_service->OnInfobarDisplayed();

  const std::string& extension2_id = LoadUnpackedThemeAt(temp_dir2.GetPath());
  EXPECT_EQ(extension2_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension2_id));
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));

  // 2) Enabling a disabled theme extension should swap the current theme.
  service_->EnableExtension(extension1_id);
  WaitForThemeInstall();
  EXPECT_EQ(extension1_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension1_id));
  EXPECT_TRUE(registry_->GetExtensionById(extension2_id,
                                          ExtensionRegistry::DISABLED));

  // 3) Using RevertToTheme() with a disabled theme should enable and set the
  // theme. This is the case when the user reverts to the previous theme
  // via an infobar.
  const extensions::Extension* extension2 =
      service_->GetInstalledExtension(extension2_id);
  theme_service->RevertToTheme(extension2);
  WaitForThemeInstall();
  EXPECT_EQ(extension2_id, theme_service->GetThemeID());
  EXPECT_TRUE(service_->IsExtensionEnabled(extension2_id));
  EXPECT_TRUE(registry_->GetExtensionById(extension1_id,
                                          ExtensionRegistry::DISABLED));

  // 4) Disabling the current theme extension should revert to the default theme
  // and uninstall any installed theme extensions.
  theme_service->OnInfobarDestroyed();
  EXPECT_FALSE(theme_service->UsingDefaultTheme());
  service_->DisableExtension(extension2_id,
                             extensions::disable_reason::DISABLE_USER_ACTION);
  base::RunLoop().RunUntilIdle();
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
  base::RunLoop().RunUntilIdle();

  theme_service->OnInfobarDisplayed();

  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  const std::string& extension1_id = LoadUnpackedThemeAt(temp_dir1.GetPath());
  const std::string& extension2_id = LoadUnpackedThemeAt(temp_dir2.GetPath());

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

TEST_F(ThemeServiceTest, IncognitoTest) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::RunLoop().RunUntilIdle();

  // Should get the same ThemeService for incognito and original profiles.
  ThemeService* otr_theme_service =
      ThemeServiceFactory::GetForProfile(profile_->GetOffTheRecordProfile());
  EXPECT_EQ(theme_service, otr_theme_service);

#if !defined(OS_MACOSX)
  // Should get a different ThemeProvider for incognito and original profiles.
  const ui::ThemeProvider& provider =
      ThemeService::GetThemeProviderForProfile(profile_.get());
  const ui::ThemeProvider& otr_provider =
      ThemeService::GetThemeProviderForProfile(
          profile_->GetOffTheRecordProfile());
  EXPECT_NE(&provider, &otr_provider);
  // And (some) colors should be different.
  EXPECT_NE(provider.GetColor(ThemeProperties::COLOR_TOOLBAR),
            otr_provider.GetColor(ThemeProperties::COLOR_TOOLBAR));
#endif
}

namespace {

// NotificationObserver which emulates an infobar getting destroyed when the
// theme changes.
class InfobarDestroyerOnThemeChange : public content::NotificationObserver {
 public:
  explicit InfobarDestroyerOnThemeChange(Profile* profile)
      : theme_service_(ThemeServiceFactory::GetForProfile(profile)) {
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
        content::Source<ThemeService>(theme_service_));
  }

  ~InfobarDestroyerOnThemeChange() override {}

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    theme_service_->OnInfobarDestroyed();
  }

  // Not owned.
  ThemeService* theme_service_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InfobarDestroyerOnThemeChange);
};

}  // namespace

// crbug.com/468280
TEST_F(ThemeServiceTest, UninstallThemeOnThemeChangeNotification) {
  // Setup.
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());
  theme_service->UseDefaultTheme();
  // Let the ThemeService uninstall unused themes.
  base::RunLoop().RunUntilIdle();

  base::ScopedTempDir temp_dir1;
  ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

  const std::string& extension1_id = LoadUnpackedThemeAt(temp_dir1.GetPath());
  ASSERT_EQ(extension1_id, theme_service->GetThemeID());

  // Show an infobar.
  theme_service->OnInfobarDisplayed();

  // Install another theme. The first extension shouldn't be uninstalled yet as
  // it should be possible to revert to it. Emulate the infobar destroying
  // itself as a result of the NOTIFICATION_BROWSER_THEME_CHANGED notification.
  {
    InfobarDestroyerOnThemeChange destroyer(profile_.get());
    const std::string& extension2_id = LoadUnpackedThemeAt(temp_dir2.GetPath());
    EXPECT_EQ(extension2_id, theme_service->GetThemeID());
  }

  auto* extension1 = service_->GetInstalledExtension(extension1_id);
  ASSERT_TRUE(extension1);

  // Check that it is possible to reinstall extension1.
  ThemeServiceFactory::GetForProfile(profile_.get())->RevertToTheme(extension1);
  WaitForThemeInstall();
  EXPECT_EQ(extension1_id, theme_service->GetThemeID());
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
class ThemeServiceSupervisedUserTest : public ThemeServiceTest {
 public:
  ThemeServiceSupervisedUserTest() {}
  ~ThemeServiceSupervisedUserTest() override {}

  void SetUp() override {
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

TEST_F(ThemeServiceTest, UserThemeTakesPrecedenceOverSystemTheme) {
  ThemeService* theme_service =
      ThemeServiceFactory::GetForProfile(profile_.get());

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const std::string& extension_id = LoadUnpackedThemeAt(temp_dir.GetPath());
  ASSERT_EQ(extension_id, theme_service->GetThemeID());

  // Set preference |prefs::kUsesSystemTheme| to true which conflicts with
  // having a user theme selected.
  profile_->GetPrefs()->SetBoolean(prefs::kUsesSystemTheme, true);
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme));

  // Initialization should fix the preference inconsistency.
  theme_service->Init(profile_.get());
  ASSERT_EQ(extension_id, theme_service->GetThemeID());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kUsesSystemTheme));
}
#endif // defined(OS_LINUX) && !defined(OS_CHROMEOS)
#endif // BUILDFLAG(ENABLE_SUPERVISED_USERS)

#if !defined(OS_MACOSX)  // Mac uses different colors than other platforms.
// Check that the function which computes the separator color behaves as
// expected for a variety of inputs.
TEST_F(ThemeServiceTest, SeparatorColor) {
  // Refresh does not draw the toolbar top separator.
  if (ui::MaterialDesignController::IsRefreshUi())
    return;

  // Ensure Windows 10 machines use the built-in default colors rather than the
  // current system native colors.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDwmComposition);

  {
    const ui::ThemeProvider& theme_provider =
        ThemeService::GetThemeProviderForProfile(profile_.get());
    const SkColor frame_color =
        theme_provider.GetColor(ThemeProperties::COLOR_FRAME);
    const SkColor tab_color =
        theme_provider.GetColor(ThemeProperties::COLOR_TOOLBAR);
    SCOPED_TRACE(base::StringPrintf("Tab color: 0x%08X, frame color: 0x%08X",
                                    tab_color, frame_color));

    // Check that the TOOLBAR_TOP_SEPARATOR color is the same whether we ask the
    // theme provider or compute it manually.
    const SkColor theme_color = AlphaBlend(
        theme_provider.GetColor(ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR),
        frame_color);
    SkColor separator_color = GetSeparatorColor(tab_color, frame_color);
    EXPECT_EQ(theme_color, separator_color);

    // For the default theme, the separator should darken the frame.
    float frame_luminance = color_utils::GetRelativeLuminance(frame_color);
    EXPECT_LT(color_utils::GetRelativeLuminance(separator_color),
              frame_luminance);

    // If we reverse the colors, the separator should darken the "frame" (which
    // in this case is actually the tab color), since otherwise the contrast
    // with the "frame" would be too minimal.  It should also be darker than the
    // "tab" (frame color) since otherwise the contrast the contrast with the
    // "tab color" would be too minimal.
    separator_color = GetSeparatorColor(frame_color, tab_color);
    float tab_luminance = color_utils::GetRelativeLuminance(tab_color);
    float separator_luminance =
        color_utils::GetRelativeLuminance(separator_color);
    EXPECT_LT(separator_luminance, tab_luminance);
    EXPECT_LT(separator_luminance, frame_luminance);

    // When the frame color is black, the separator should lighten the frame,
    // but it should still be darker than the tab color.
    separator_color = GetSeparatorColor(tab_color, SK_ColorBLACK);
    separator_luminance = color_utils::GetRelativeLuminance(separator_color);
    EXPECT_GT(separator_luminance, 0);
    EXPECT_LT(separator_luminance, tab_luminance);

    // When the frame color is white, the separator should darken the frame; it
    // should also be lighter than the tab color since otherwise the contrast
    // with the tab would be too minimal.
    separator_color = GetSeparatorColor(tab_color, SK_ColorWHITE);
    separator_luminance = color_utils::GetRelativeLuminance(separator_color);
    EXPECT_LT(separator_luminance, 1);
    EXPECT_LT(separator_luminance, tab_luminance);
  }

  // Now make similar checks as above but for the incognito theme.
  {
    const ui::ThemeProvider& otr_provider =
        ThemeService::GetThemeProviderForProfile(
            profile_->GetOffTheRecordProfile());
    const SkColor frame_color =
        otr_provider.GetColor(ThemeProperties::COLOR_FRAME);
    const SkColor tab_color =
        otr_provider.GetColor(ThemeProperties::COLOR_TOOLBAR);
    SCOPED_TRACE(base::StringPrintf("Tab color: 0x%08X, frame color: 0x%08X",
                                    tab_color, frame_color));

    const SkColor theme_color = AlphaBlend(
        otr_provider.GetColor(ThemeProperties::COLOR_TOOLBAR_TOP_SEPARATOR),
        frame_color);
    SkColor separator_color = GetSeparatorColor(tab_color, frame_color);
    EXPECT_EQ(theme_color, separator_color);

    // For the default incognito theme, the separator should darken the frame.
    EXPECT_LT(color_utils::GetRelativeLuminance(separator_color),
              color_utils::GetRelativeLuminance(frame_color));

    // And if we reverse the colors, the separator should lighten the "frame"
    // (tab color).
    separator_color = GetSeparatorColor(frame_color, tab_color);
    float tab_luminance = color_utils::GetRelativeLuminance(tab_color);
    EXPECT_GT(color_utils::GetRelativeLuminance(separator_color),
              tab_luminance);

    // When the frame color is black, the separator should lighten the frame; it
    // should also be lighter than the tab color since otherwise the contrast
    // with the tab would be too minimal.
    separator_color = GetSeparatorColor(tab_color, SK_ColorBLACK);
    float separator_luminance =
        color_utils::GetRelativeLuminance(separator_color);
    EXPECT_GT(separator_luminance, 0);
    EXPECT_GT(separator_luminance, tab_luminance);

    // When the frame color is white, the separator should darken the frame, but
    // it should still be lighter than the tab color.
    separator_color = GetSeparatorColor(tab_color, SK_ColorWHITE);
    separator_luminance = color_utils::GetRelativeLuminance(separator_color);
    EXPECT_LT(separator_luminance, 1);
    EXPECT_GT(separator_luminance, tab_luminance);
  }
}
#endif  // !defined(OS_MACOSX)

}; // namespace theme_service_internal
