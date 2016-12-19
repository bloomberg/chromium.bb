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
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/value_builder.h"
#include "url/gurl.h"

namespace app_runtime = extensions::api::app_runtime;

using arc::mojom::IntentHandlerInfo;
using arc::mojom::IntentHandlerInfoPtr;
using HandledIntent = arc::FakeIntentHelperInstance::HandledIntent;

namespace chromeos {
namespace {

// Helper functions returning strings that can be used to compare information
// about available note-taking apps.
std::string GetAppString(const std::string& id,
                         const std::string& name,
                         bool preferred) {
  return base::StringPrintf("%s %s %d", id.c_str(), name.c_str(), preferred);
}
std::string GetAppString(const NoteTakingAppInfo& info) {
  return GetAppString(info.app_id, info.name, info.preferred);
}

// Helper functions returning strings that can be used to compare launched
// intents.
std::string GetIntentString(const std::string& package,
                            const std::string& clip_data_uri) {
  return base::StringPrintf(
      "%s %s", package.c_str(),
      clip_data_uri.empty() ? "[unset]" : clip_data_uri.c_str());
}
std::string GetIntentString(const HandledIntent& intent) {
  EXPECT_EQ(NoteTakingHelper::kIntentAction, intent.intent->action);
  return GetIntentString(
      intent.activity->package_name,
      (intent.intent->clip_data_uri ? *intent.intent->clip_data_uri : ""));
}

// Creates an ARC IntentHandlerInfo object.
IntentHandlerInfoPtr CreateIntentHandlerInfo(const std::string& name,
                                             const std::string& package) {
  IntentHandlerInfoPtr handler = IntentHandlerInfo::New();
  handler->name = name;
  handler->package_name = package;
  return handler;
}

// Converts a filesystem path to an ARC URL.
std::string GetArcUrl(const base::FilePath& path) {
  GURL url;
  EXPECT_TRUE(file_manager::util::ConvertPathToArcUrl(path, &url));
  return url.spec();
}

// Implementation of NoteTakingHelper::Observer for testing.
class TestObserver : public NoteTakingHelper::Observer {
 public:
  TestObserver() { NoteTakingHelper::Get()->AddObserver(this); }
  ~TestObserver() override { NoteTakingHelper::Get()->RemoveObserver(this); }

  int num_updates() const { return num_updates_; }

 private:
  // NoteTakingHelper::Observer:
  void OnAvailableNoteTakingAppsUpdated() override { num_updates_++; }

  // Number of times that OnAvailableNoteTakingAppsUpdated() has been called.
  int num_updates_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class NoteTakingHelperTest : public BrowserWithTestWindowTest {
 public:
  NoteTakingHelperTest() = default;
  ~NoteTakingHelperTest() override = default;

  void SetUp() override {
    // This is needed to avoid log spam due to ArcSessionManager's
    // RemoveArcData() calls failing.
    if (DBusThreadManager::IsInitialized())
      DBusThreadManager::Shutdown();
    session_manager_client_ = new FakeSessionManagerClient();
    session_manager_client_->set_arc_available(true);
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(session_manager_client_));

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
      arc_test_.TearDown();
    }
    extensions::ExtensionSystem::Get(profile())->Shutdown();
    BrowserWithTestWindowTest::TearDown();
    DBusThreadManager::Shutdown();
  }

 protected:
  // Information about a Chrome app passed to LaunchChromeApp().
  struct ChromeAppLaunchInfo {
    extensions::ExtensionId id;
    base::FilePath path;
  };

  // Options that can be passed to Init().
  enum InitFlags {
    ENABLE_ARC = 1 << 0,
    ENABLE_PALETTE = 1 << 1,
  };

  static NoteTakingHelper* helper() { return NoteTakingHelper::Get(); }

  // Initializes ARC and NoteTakingHelper. |flags| contains OR-ed together
  // InitFlags values.
  void Init(uint32_t flags) {
    ASSERT_FALSE(initialized_);
    initialized_ = true;

    profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, flags & ENABLE_ARC);
    arc_test_.SetUp(profile());
    arc::ArcServiceManager::Get()
        ->arc_bridge_service()
        ->intent_helper()
        ->SetInstance(&intent_helper_);

    if (flags & ENABLE_PALETTE) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          ash::switches::kAshEnablePalette);
    }

    // TODO(derat): Sigh, something in ArcAppTest appears to be re-enabling ARC.
    profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, flags & ENABLE_ARC);
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

  // Info about launched Chrome apps, in the order they were launched.
  std::vector<ChromeAppLaunchInfo> launched_chrome_apps_;

  arc::FakeIntentHelperInstance intent_helper_;

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

  FakeSessionManagerClient* session_manager_client_ = nullptr;  // Not owned.

  ArcAppTest arc_test_;

  DISALLOW_COPY_AND_ASSIGN(NoteTakingHelperTest);
};

TEST_F(NoteTakingHelperTest, PaletteNotEnabled) {
  // Without the palette enabled, IsAppAvailable() should return false.
  Init(0);
  auto extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(extension.get());
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
}

TEST_F(NoteTakingHelperTest, ListChromeApps) {
  Init(ENABLE_PALETTE);

  // Start out without any note-taking apps installed.
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // If only the prod version of the app is installed, it should be returned.
  const std::string kProdName = "Google Keep [prod]";
  auto prod_extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, kProdName);
  InstallExtension(prod_extension.get());
  EXPECT_TRUE(helper()->IsAppAvailable(profile()));
  std::vector<NoteTakingAppInfo> apps = helper()->GetAvailableApps(profile());
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
  Init(ENABLE_PALETTE);
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
  Init(ENABLE_PALETTE);
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

TEST_F(NoteTakingHelperTest, ArcInitiallyDisabled) {
  Init(ENABLE_PALETTE);
  EXPECT_FALSE(helper()->android_enabled());
  EXPECT_FALSE(helper()->android_apps_received());

  // When ARC is enabled, the helper's members should be updated accordingly.
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  EXPECT_TRUE(helper()->android_enabled());
  EXPECT_FALSE(helper()->android_apps_received());

  // After the callback to receive intent handlers has run, the "apps received"
  // member should be updated (even if there aren't any apps).
  helper()->OnIntentFiltersUpdated();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper()->android_enabled());
  EXPECT_TRUE(helper()->android_apps_received());
}

TEST_F(NoteTakingHelperTest, ListAndroidApps) {
  // Add two Android apps.
  std::vector<IntentHandlerInfoPtr> handlers;
  const std::string kName1 = "App 1";
  const std::string kPackage1 = "org.chromium.package1";
  handlers.emplace_back(CreateIntentHandlerInfo(kName1, kPackage1));
  const std::string kName2 = "App 2";
  const std::string kPackage2 = "org.chromium.package2";
  handlers.emplace_back(CreateIntentHandlerInfo(kName2, kPackage2));
  intent_helper_.SetIntentHandlers(NoteTakingHelper::kIntentAction,
                                   std::move(handlers));

  // NoteTakingHelper should make an async request for Android apps when
  // constructed.
  Init(ENABLE_PALETTE | ENABLE_ARC);
  EXPECT_TRUE(helper()->android_enabled());
  EXPECT_FALSE(helper()->android_apps_received());
  EXPECT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // The apps should be listed after the callback has had a chance to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper()->android_enabled());
  EXPECT_TRUE(helper()->android_apps_received());
  EXPECT_TRUE(helper()->IsAppAvailable(profile()));
  std::vector<NoteTakingAppInfo> apps = helper()->GetAvailableApps(profile());
  ASSERT_EQ(2u, apps.size());
  EXPECT_EQ(GetAppString(kPackage1, kName1, false), GetAppString(apps[0]));
  EXPECT_EQ(GetAppString(kPackage2, kName2, false), GetAppString(apps[1]));

  // Disable ARC and check that the apps are no longer returned.
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
  EXPECT_FALSE(helper()->android_enabled());
  EXPECT_FALSE(helper()->android_apps_received());
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(helper()->GetAvailableApps(profile()).empty());
}

TEST_F(NoteTakingHelperTest, LaunchAndroidApp) {
  const std::string kPackage1 = "org.chromium.package1";
  std::vector<IntentHandlerInfoPtr> handlers;
  handlers.emplace_back(CreateIntentHandlerInfo("App 1", kPackage1));
  intent_helper_.SetIntentHandlers(NoteTakingHelper::kIntentAction,
                                   std::move(handlers));

  Init(ENABLE_PALETTE | ENABLE_ARC);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(helper()->IsAppAvailable(profile()));

  // The installed app should be launched.
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, intent_helper_.handled_intents().size());
  EXPECT_EQ(GetIntentString(kPackage1, ""),
            GetIntentString(intent_helper_.handled_intents()[0]));

  // Install a second app and set it as the preferred app.
  const std::string kPackage2 = "org.chromium.package2";
  handlers.emplace_back(CreateIntentHandlerInfo("App 1", kPackage1));
  handlers.emplace_back(CreateIntentHandlerInfo("App 2", kPackage2));
  intent_helper_.SetIntentHandlers(NoteTakingHelper::kIntentAction,
                                   std::move(handlers));
  helper()->OnIntentFiltersUpdated();
  base::RunLoop().RunUntilIdle();
  helper()->SetPreferredApp(profile(), kPackage2);

  // The second app should be launched now.
  intent_helper_.clear_handled_intents();
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, intent_helper_.handled_intents().size());
  EXPECT_EQ(GetIntentString(kPackage2, ""),
            GetIntentString(intent_helper_.handled_intents()[0]));
}

TEST_F(NoteTakingHelperTest, LaunchAndroidAppWithPath) {
  const std::string kPackage = "org.chromium.package";
  std::vector<IntentHandlerInfoPtr> handlers;
  handlers.emplace_back(CreateIntentHandlerInfo("App", kPackage));
  intent_helper_.SetIntentHandlers(NoteTakingHelper::kIntentAction,
                                   std::move(handlers));

  Init(ENABLE_PALETTE | ENABLE_ARC);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(helper()->IsAppAvailable(profile()));

  const base::FilePath kDownloadedPath(
      file_manager::util::GetDownloadsFolderForProfile(profile()).Append(
          "image.jpg"));
  helper()->LaunchAppForNewNote(profile(), kDownloadedPath);
  ASSERT_EQ(1u, intent_helper_.handled_intents().size());
  EXPECT_EQ(GetIntentString(kPackage, GetArcUrl(kDownloadedPath)),
            GetIntentString(intent_helper_.handled_intents()[0]));

  const base::FilePath kRemovablePath =
      base::FilePath(file_manager::util::kRemovableMediaPath)
          .Append("image.jpg");
  intent_helper_.clear_handled_intents();
  helper()->LaunchAppForNewNote(profile(), kRemovablePath);
  ASSERT_EQ(1u, intent_helper_.handled_intents().size());
  EXPECT_EQ(GetIntentString(kPackage, GetArcUrl(kRemovablePath)),
            GetIntentString(intent_helper_.handled_intents()[0]));

  // When a path that isn't accessible to ARC is passed, the request should be
  // dropped.
  intent_helper_.clear_handled_intents();
  helper()->LaunchAppForNewNote(profile(), base::FilePath("/bad/path.jpg"));
  EXPECT_TRUE(intent_helper_.handled_intents().empty());
}

TEST_F(NoteTakingHelperTest, NotifyObserver) {
  Init(ENABLE_PALETTE | ENABLE_ARC);
  TestObserver observer;

  // Let the app-fetching callback run and check that the observer is notified.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.num_updates());

  // Disabling and enabling ARC should also notify the observer (and enabling
  // should request apps again).
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
  EXPECT_EQ(2, observer.num_updates());
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  EXPECT_EQ(3, observer.num_updates());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, observer.num_updates());

  // Update intent filters and check that the observer is notified again after
  // apps are received.
  helper()->OnIntentFiltersUpdated();
  EXPECT_EQ(4, observer.num_updates());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5, observer.num_updates());
}

}  // namespace chromeos
