// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/note_taking_helper.h"

#include <utility>

#include "ash/common/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/value_builder.h"

namespace app_runtime = extensions::api::app_runtime;

namespace chromeos {
namespace {

// Helper functions returning strings that can be used for comparison.
std::string GetAppString(const std::string& id,
                         const std::string& name,
                         bool preferred) {
  return base::StringPrintf("%s %s %d", id.c_str(), name.c_str(), preferred);
}
std::string GetAppString(const NoteTakingAppInfo& info) {
  return GetAppString(info.app_id, info.name, info.preferred);
}

}  // namespace

class NoteTakingHelperTest : public BrowserWithTestWindowTest {
 public:
  NoteTakingHelperTest() = default;
  ~NoteTakingHelperTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        base::FilePath() /* install_directory */,
        false /* autoupdate_enabled */);
  }

  void TearDown() override {
    if (initialized_) {
      NoteTakingHelper::Shutdown();
    }
    extensions::ExtensionSystem::Get(profile())->Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  // Information about a Chrome app passed to LaunchChromeApp().
  struct ChromeAppLaunchInfo {
    extensions::ExtensionId id;
    base::FilePath path;
  };

  static NoteTakingHelper* helper() { return NoteTakingHelper::Get(); }

  // Initializes NoteTakingHelper.
  void Init() {
    ASSERT_FALSE(initialized_);
    initialized_ = true;

    if (palette_enabled_) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          ash::switches::kAshEnablePalette);
    }

    NoteTakingHelper::Initialize();
    NoteTakingHelper::Get()->set_launch_chrome_app_callback_for_test(base::Bind(
        &NoteTakingHelperTest::LaunchChromeApp, base::Unretained(this)));
  }

  // Creates an extension.
  scoped_refptr<const extensions::Extension> CreateExtension(
      const extensions::ExtensionId& id,
      const std::string& name) {
    std::unique_ptr<base::DictionaryValue> manifest =
        extensions::DictionaryBuilder()
            .Set("name", name)
            .Set("version", "1.0")
            .Set("manifest_version", 2)
            .Set("app",
                 extensions::DictionaryBuilder()
                     .Set("background",
                          extensions::DictionaryBuilder()
                              .Set("scripts", extensions::ListBuilder()
                                                  .Append("background.js")
                                                  .Build())
                              .Build())
                     .Build())
            .Build();
    return extensions::ExtensionBuilder()
        .SetManifest(std::move(manifest))
        .SetID(id)
        .Build();
  }

  // Installs or uninstalls the passed-in extension.
  void InstallExtension(const extensions::Extension* extension) {
    extensions::ExtensionSystem::Get(profile())
        ->extension_service()
        ->AddExtension(extension);
  }
  void UninstallExtension(const extensions::Extension* extension) {
    extensions::ExtensionSystem::Get(profile())
        ->extension_service()
        ->UnloadExtension(extension->id(),
                          extensions::UnloadedExtensionInfo::REASON_UNINSTALL);
  }

  // Should Init() enable the palette feature?
  bool palette_enabled_ = true;

  // Info about launched Chrome apps, in the order they were launched.
  std::vector<ChromeAppLaunchInfo> launched_chrome_apps_;

 private:
  // Callback registered with the helper to record Chrome app launch requests.
  void LaunchChromeApp(Profile* passed_profile,
                       const extensions::Extension* extension,
                       std::unique_ptr<app_runtime::ActionData> action_data,
                       const base::FilePath& path) {
    EXPECT_EQ(profile(), passed_profile);
    EXPECT_EQ(app_runtime::ActionType::ACTION_TYPE_NEW_NOTE,
              action_data->action_type);
    launched_chrome_apps_.push_back(ChromeAppLaunchInfo{extension->id(), path});
  }

  // Has Init() been called?
  bool initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(NoteTakingHelperTest);
};

TEST_F(NoteTakingHelperTest, PaletteNotEnabled) {
  // Without the palette enabled, IsAppAvailable() should return false.
  palette_enabled_ = false;
  Init();
  auto extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(extension.get());
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
}

TEST_F(NoteTakingHelperTest, ListChromeApps) {
  Init();

  // Start out without any note-taking apps installed.
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
  std::vector<NoteTakingAppInfo> apps = helper()->GetAvailableApps(profile());
  EXPECT_TRUE(apps.empty());

  // If only the prod version of the app is installed, it should be returned.
  const std::string kProdName = "Google Keep [prod]";
  auto prod_extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, kProdName);
  InstallExtension(prod_extension.get());
  EXPECT_TRUE(helper()->IsAppAvailable(profile()));
  apps = helper()->GetAvailableApps(profile());
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ(
      GetAppString(NoteTakingHelper::kProdKeepExtensionId, kProdName, false),
      GetAppString(apps[0]));

  // If the dev version is also installed, it should be listed before the prod
  // version.
  const std::string kDevName = "Google Keep [dev]";
  auto dev_extension =
      CreateExtension(NoteTakingHelper::kDevKeepExtensionId, kDevName);
  InstallExtension(dev_extension.get());
  apps = helper()->GetAvailableApps(profile());
  ASSERT_EQ(2u, apps.size());
  EXPECT_EQ(
      GetAppString(NoteTakingHelper::kDevKeepExtensionId, kDevName, false),
      GetAppString(apps[0]));
  EXPECT_EQ(
      GetAppString(NoteTakingHelper::kProdKeepExtensionId, kProdName, false),
      GetAppString(apps[1]));

  // Now install a random extension and check that it's ignored.
  const extensions::ExtensionId kOtherId = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  const std::string kOtherName = "Some Other App";
  auto other_extension = CreateExtension(kOtherId, kOtherName);
  InstallExtension(other_extension.get());
  apps = helper()->GetAvailableApps(profile());
  ASSERT_EQ(2u, apps.size());
  EXPECT_EQ(
      GetAppString(NoteTakingHelper::kDevKeepExtensionId, kDevName, false),
      GetAppString(apps[0]));
  EXPECT_EQ(
      GetAppString(NoteTakingHelper::kProdKeepExtensionId, kProdName, false),
      GetAppString(apps[1]));

  // Mark the prod version as preferred.
  helper()->SetPreferredApp(profile(), NoteTakingHelper::kProdKeepExtensionId);
  apps = helper()->GetAvailableApps(profile());
  ASSERT_EQ(2u, apps.size());
  EXPECT_EQ(
      GetAppString(NoteTakingHelper::kDevKeepExtensionId, kDevName, false),
      GetAppString(apps[0]));
  EXPECT_EQ(
      GetAppString(NoteTakingHelper::kProdKeepExtensionId, kProdName, true),
      GetAppString(apps[1]));
}

TEST_F(NoteTakingHelperTest, LaunchChromeApp) {
  Init();
  auto extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(extension.get());

  // Check the Chrome app is launched with the correct parameters.
  const base::FilePath kPath("/foo/bar/photo.jpg");
  helper()->LaunchAppForNewNote(profile(), kPath);
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  EXPECT_EQ(NoteTakingHelper::kProdKeepExtensionId,
            launched_chrome_apps_[0].id);
  EXPECT_EQ(kPath, launched_chrome_apps_[0].path);
}

TEST_F(NoteTakingHelperTest, FallBackIfPreferredAppUnavailable) {
  Init();
  auto prod_extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "prod");
  InstallExtension(prod_extension.get());
  auto dev_extension =
      CreateExtension(NoteTakingHelper::kDevKeepExtensionId, "dev");
  InstallExtension(dev_extension.get());

  // Set the prod app as the default and check that it's launched.
  helper()->SetPreferredApp(profile(), NoteTakingHelper::kProdKeepExtensionId);
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  ASSERT_EQ(NoteTakingHelper::kProdKeepExtensionId,
            launched_chrome_apps_[0].id);

  // Now uninstall the prod app and check that we fall back to the dev app.
  UninstallExtension(prod_extension.get());
  launched_chrome_apps_.clear();
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  EXPECT_EQ(NoteTakingHelper::kDevKeepExtensionId, launched_chrome_apps_[0].id);
}

}  // namespace chromeos
