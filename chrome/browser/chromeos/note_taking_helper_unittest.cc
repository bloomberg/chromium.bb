// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/note_taking_helper.h"

#include <utility>

#include "ash/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/test/fake_intent_helper_instance.h"
#include "components/crx_file/id_util.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/value_builder.h"
#include "url/gurl.h"

namespace app_runtime = extensions::api::app_runtime;

using arc::mojom::IntentHandlerInfo;
using arc::mojom::IntentHandlerInfoPtr;
using base::HistogramTester;
using HandledIntent = arc::FakeIntentHelperInstance::HandledIntent;

namespace chromeos {

using LaunchResult = NoteTakingHelper::LaunchResult;

namespace {

// Name of default profile.
const char kTestProfileName[] = "test-profile";

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
  void reset_num_updates() { num_updates_ = 0; }

 private:
  // NoteTakingHelper::Observer:
  void OnAvailableNoteTakingAppsUpdated() override { num_updates_++; }

  // Number of times that OnAvailableNoteTakingAppsUpdated() has been called.
  int num_updates_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

}  // namespace

class NoteTakingHelperTest : public BrowserWithTestWindowTest,
                             public ::testing::WithParamInterface<bool> {
 public:
  NoteTakingHelperTest() = default;
  ~NoteTakingHelperTest() override = default;

  void SetUp() override {
    if (GetParam())
      arc::SetArcAlwaysStartForTesting();

    // This is needed to avoid log spam due to ArcSessionManager's
    // RemoveArcData() calls failing.
    if (DBusThreadManager::IsInitialized())
      DBusThreadManager::Shutdown();
    session_manager_client_ = new FakeSessionManagerClient();
    session_manager_client_->set_arc_available(true);
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(session_manager_client_));

    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    BrowserWithTestWindowTest::SetUp();
    InitExtensionService(profile());
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
  enum InitFlags : uint32_t {
    ENABLE_PLAY_STORE = 1 << 0,
    ENABLE_PALETTE = 1 << 1,
  };

  static NoteTakingHelper* helper() { return NoteTakingHelper::Get(); }

  // Initializes ARC and NoteTakingHelper. |flags| contains OR-ed together
  // InitFlags values.
  void Init(uint32_t flags) {
    ASSERT_FALSE(initialized_);
    initialized_ = true;

    profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled,
                                      flags & ENABLE_PLAY_STORE);
    arc_test_.SetUp(profile());
    arc::ArcServiceManager::Get()
        ->arc_bridge_service()
        ->intent_helper()
        ->SetInstance(&intent_helper_);

    if (flags & ENABLE_PALETTE) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          ash::switches::kAshForceEnablePalette);
    }

    // TODO(derat): Sigh, something in ArcAppTest appears to be re-enabling ARC.
    profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled,
                                      flags & ENABLE_PLAY_STORE);
    NoteTakingHelper::Initialize();
    NoteTakingHelper::Get()->set_launch_chrome_app_callback_for_test(base::Bind(
        &NoteTakingHelperTest::LaunchChromeApp, base::Unretained(this)));
  }

  // Creates an extension.
  scoped_refptr<const extensions::Extension> CreateExtension(
      const extensions::ExtensionId& id,
      const std::string& name) {
    return CreateExtension(id, name, nullptr);
  }
  scoped_refptr<const extensions::Extension> CreateExtension(
      const extensions::ExtensionId& id,
      const std::string& name,
      std::unique_ptr<base::Value> action_handlers) {
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

    if (action_handlers)
      manifest->Set("action_handlers", std::move(action_handlers));

    return extensions::ExtensionBuilder()
        .SetManifest(std::move(manifest))
        .SetID(id)
        .Build();
  }

  // Initializes extensions-related objects for |profile|. Tests only need to
  // call this if they create additional profiles of their own.
  void InitExtensionService(Profile* profile) {
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(),
        base::FilePath() /* install_directory */,
        false /* autoupdate_enabled */);
  }

  // Installs or uninstalls |extension| in |profile|.
  void InstallExtension(const extensions::Extension* extension,
                        Profile* profile) {
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->AddExtension(extension);
  }
  void UninstallExtension(const extensions::Extension* extension,
                          Profile* profile) {
    extensions::ExtensionSystem::Get(profile)
        ->extension_service()
        ->UnloadExtension(extension->id(),
                          extensions::UnloadedExtensionInfo::REASON_UNINSTALL);
  }

  // BrowserWithTestWindowTest:
  TestingProfile* CreateProfile() override {
    // Ensure that the profile created by BrowserWithTestWindowTest is
    // registered with |profile_manager_|.
    return profile_manager_->CreateTestingProfile(kTestProfileName);
  }
  void DestroyProfile(TestingProfile* profile) override {
    return profile_manager_->DeleteTestingProfile(kTestProfileName);
  }

  // Info about launched Chrome apps, in the order they were launched.
  std::vector<ChromeAppLaunchInfo> launched_chrome_apps_;

  arc::FakeIntentHelperInstance intent_helper_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

 private:
  // Callback registered with the helper to record Chrome app launch requests.
  void LaunchChromeApp(content::BrowserContext* passed_context,
                       const extensions::Extension* extension,
                       std::unique_ptr<app_runtime::ActionData> action_data,
                       const base::FilePath& path) {
    EXPECT_EQ(profile(), passed_context);
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

INSTANTIATE_TEST_CASE_P(, NoteTakingHelperTest, ::testing::Bool());

TEST_P(NoteTakingHelperTest, PaletteNotEnabled) {
  // Without the palette enabled, IsAppAvailable() should return false.
  Init(0);
  auto extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(extension.get(), profile());
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
}

TEST_P(NoteTakingHelperTest, ListChromeApps) {
  Init(ENABLE_PALETTE);

  // Start out without any note-taking apps installed.
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // If only the prod version of the app is installed, it should be returned.
  const std::string kProdName = "Google Keep [prod]";
  auto prod_extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, kProdName);
  InstallExtension(prod_extension.get(), profile());
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
  InstallExtension(dev_extension.get(), profile());
  apps = helper()->GetAvailableApps(profile());
  ASSERT_EQ(2u, apps.size());
  EXPECT_EQ(
      GetAppString(NoteTakingHelper::kDevKeepExtensionId, kDevName, false),
      GetAppString(apps[0]));
  EXPECT_EQ(
      GetAppString(NoteTakingHelper::kProdKeepExtensionId, kProdName, false),
      GetAppString(apps[1]));

  // Now install a random extension and check that it's ignored.
  const extensions::ExtensionId kOtherId = crx_file::id_util::GenerateId("a");
  const std::string kOtherName = "Some Other App";
  auto other_extension = CreateExtension(kOtherId, kOtherName);
  InstallExtension(other_extension.get(), profile());
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

// Verify the note helper detects apps with "new_note" "action_handler" manifest
// entries.
TEST_P(NoteTakingHelperTest, CustomChromeApps) {
  Init(ENABLE_PALETTE);

  const extensions::ExtensionId kNewNoteId = crx_file::id_util::GenerateId("a");
  const extensions::ExtensionId kEmptyArrayId =
      crx_file::id_util::GenerateId("b");
  const extensions::ExtensionId kEmptyId = crx_file::id_util::GenerateId("c");
  const std::string kName = "Some App";

  // "action_handlers": ["new_note"]
  auto has_new_note = CreateExtension(
      kNewNoteId, kName,
      extensions::ListBuilder()
          .Append(app_runtime::ToString(app_runtime::ACTION_TYPE_NEW_NOTE))
          .Build());
  InstallExtension(has_new_note.get(), profile());
  // "action_handlers": []
  auto empty_array =
      CreateExtension(kEmptyArrayId, kName, extensions::ListBuilder().Build());
  InstallExtension(empty_array.get(), profile());
  // (no action handler entry)
  auto none = CreateExtension(kEmptyId, kName);
  InstallExtension(none.get(), profile());

  // Only the "new_note" extension is returned from GetAvailableApps.
  std::vector<NoteTakingAppInfo> apps = helper()->GetAvailableApps(profile());
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ(GetAppString(kNewNoteId, kName, false), GetAppString(apps[0]));
}

TEST_P(NoteTakingHelperTest, WhitelistedAndCustomAppsShowOnlyOnce) {
  Init(ENABLE_PALETTE);

  auto extension = CreateExtension(
      NoteTakingHelper::kProdKeepExtensionId, "Keep",
      extensions::ListBuilder()
          .Append(app_runtime::ToString(app_runtime::ACTION_TYPE_NEW_NOTE))
          .Build());
  InstallExtension(extension.get(), profile());

  std::vector<NoteTakingAppInfo> apps = helper()->GetAvailableApps(profile());
  ASSERT_EQ(1u, apps.size());
  EXPECT_EQ(GetAppString(NoteTakingHelper::kProdKeepExtensionId, "Keep", false),
            GetAppString(apps[0]));
}

TEST_P(NoteTakingHelperTest, LaunchChromeApp) {
  Init(ENABLE_PALETTE);
  auto extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(extension.get(), profile());

  // Check the Chrome app is launched with the correct parameters.
  HistogramTester histogram_tester;
  const base::FilePath kPath("/foo/bar/photo.jpg");
  helper()->LaunchAppForNewNote(profile(), kPath);
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  EXPECT_EQ(NoteTakingHelper::kProdKeepExtensionId,
            launched_chrome_apps_[0].id);
  EXPECT_EQ(kPath, launched_chrome_apps_[0].path);

  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_SUCCESS), 1);
}

TEST_P(NoteTakingHelperTest, FallBackIfPreferredAppUnavailable) {
  Init(ENABLE_PALETTE);
  auto prod_extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "prod");
  InstallExtension(prod_extension.get(), profile());
  auto dev_extension =
      CreateExtension(NoteTakingHelper::kDevKeepExtensionId, "dev");
  InstallExtension(dev_extension.get(), profile());

  // Set the prod app as preferred and check that it's launched.
  std::unique_ptr<HistogramTester> histogram_tester(new HistogramTester());
  helper()->SetPreferredApp(profile(), NoteTakingHelper::kProdKeepExtensionId);
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  ASSERT_EQ(NoteTakingHelper::kProdKeepExtensionId,
            launched_chrome_apps_[0].id);

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_SUCCESS), 1);
  histogram_tester->ExpectTotalCount(
      NoteTakingHelper::kDefaultLaunchResultHistogramName, 0);

  // Now uninstall the prod app and check that we fall back to the dev app.
  UninstallExtension(prod_extension.get(), profile());
  launched_chrome_apps_.clear();
  histogram_tester.reset(new HistogramTester());
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, launched_chrome_apps_.size());
  EXPECT_EQ(NoteTakingHelper::kDevKeepExtensionId, launched_chrome_apps_[0].id);

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_APP_MISSING), 1);
  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::CHROME_SUCCESS), 1);
}

TEST_P(NoteTakingHelperTest, PlayStoreInitiallyDisabled) {
  Init(ENABLE_PALETTE);
  EXPECT_FALSE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());
  // TODO(victorhsieh): Implement opt-in.
  if (arc::ShouldArcAlwaysStart())
    return;
  // When Play Store is enabled, the helper's members should be updated
  // accordingly.
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());

  // After the callback to receive intent handlers has run, the "apps received"
  // member should be updated (even if there aren't any apps).
  helper()->OnIntentFiltersUpdated();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_TRUE(helper()->android_apps_received());
}

TEST_P(NoteTakingHelperTest, AddProfileWithPlayStoreEnabled) {
  Init(ENABLE_PALETTE);
  EXPECT_FALSE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());

  TestObserver observer;
  ASSERT_EQ(0, observer.num_updates());

  // Add a second profile with the ARC-enabled pref already set. The Play Store
  // should be immediately regarded as being enabled and the observer should be
  // notified, since OnArcPlayStoreEnabledChanged() apparently isn't called in
  // this case: http://crbug.com/700554
  const char kSecondProfileName[] = "second-profile";
  auto prefs = base::MakeUnique<sync_preferences::TestingPrefServiceSyncable>();
  chrome::RegisterUserProfilePrefs(prefs->registry());
  prefs->SetBoolean(prefs::kArcEnabled, true);
  profile_manager_->CreateTestingProfile(
      kSecondProfileName, std::move(prefs), base::ASCIIToUTF16("Second User"),
      1 /* avatar_id */, std::string() /* supervised_user_id */,
      TestingProfile::TestingFactories());
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());
  EXPECT_EQ(1, observer.num_updates());

  // Notification of updated intent filters should result in the apps being
  // refreshed.
  helper()->OnIntentFiltersUpdated();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_TRUE(helper()->android_apps_received());
  EXPECT_EQ(2, observer.num_updates());

  profile_manager_->DeleteTestingProfile(kSecondProfileName);
}

TEST_P(NoteTakingHelperTest, ListAndroidApps) {
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
  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());
  EXPECT_TRUE(helper()->GetAvailableApps(profile()).empty());

  // The apps should be listed after the callback has had a chance to run.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(helper()->play_store_enabled());
  EXPECT_TRUE(helper()->android_apps_received());
  EXPECT_TRUE(helper()->IsAppAvailable(profile()));
  std::vector<NoteTakingAppInfo> apps = helper()->GetAvailableApps(profile());
  ASSERT_EQ(2u, apps.size());
  EXPECT_EQ(GetAppString(kPackage1, kName1, false), GetAppString(apps[0]));
  EXPECT_EQ(GetAppString(kPackage2, kName2, false), GetAppString(apps[1]));

  // TODO(victorhsieh): Opt-out on Persistent ARC is special.  Skip until
  // implemented.
  if (arc::ShouldArcAlwaysStart())
    return;
  // Disable Play Store and check that the apps are no longer returned.
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
  EXPECT_FALSE(helper()->play_store_enabled());
  EXPECT_FALSE(helper()->android_apps_received());
  EXPECT_FALSE(helper()->IsAppAvailable(profile()));
  EXPECT_TRUE(helper()->GetAvailableApps(profile()).empty());
}

TEST_P(NoteTakingHelperTest, LaunchAndroidApp) {
  const std::string kPackage1 = "org.chromium.package1";
  std::vector<IntentHandlerInfoPtr> handlers;
  handlers.emplace_back(CreateIntentHandlerInfo("App 1", kPackage1));
  intent_helper_.SetIntentHandlers(NoteTakingHelper::kIntentAction,
                                   std::move(handlers));

  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(helper()->IsAppAvailable(profile()));

  // The installed app should be launched.
  std::unique_ptr<HistogramTester> histogram_tester(new HistogramTester());
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, intent_helper_.handled_intents().size());
  EXPECT_EQ(GetIntentString(kPackage1, ""),
            GetIntentString(intent_helper_.handled_intents()[0]));

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::ANDROID_SUCCESS), 1);

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
  histogram_tester.reset(new HistogramTester());
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  ASSERT_EQ(1u, intent_helper_.handled_intents().size());
  EXPECT_EQ(GetIntentString(kPackage2, ""),
            GetIntentString(intent_helper_.handled_intents()[0]));

  histogram_tester->ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::ANDROID_SUCCESS), 1);
  histogram_tester->ExpectTotalCount(
      NoteTakingHelper::kDefaultLaunchResultHistogramName, 0);
}

TEST_P(NoteTakingHelperTest, LaunchAndroidAppWithPath) {
  const std::string kPackage = "org.chromium.package";
  std::vector<IntentHandlerInfoPtr> handlers;
  handlers.emplace_back(CreateIntentHandlerInfo("App", kPackage));
  intent_helper_.SetIntentHandlers(NoteTakingHelper::kIntentAction,
                                   std::move(handlers));

  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);
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
  HistogramTester histogram_tester;
  intent_helper_.clear_handled_intents();
  helper()->LaunchAppForNewNote(profile(), base::FilePath("/bad/path.jpg"));
  EXPECT_TRUE(intent_helper_.handled_intents().empty());

  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::ANDROID_FAILED_TO_CONVERT_PATH), 1);
}

TEST_P(NoteTakingHelperTest, NoAppsAvailable) {
  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);

  // When no note-taking apps are installed, the histograms should just be
  // updated.
  HistogramTester histogram_tester;
  helper()->LaunchAppForNewNote(profile(), base::FilePath());
  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kPreferredLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APP_SPECIFIED), 1);
  histogram_tester.ExpectUniqueSample(
      NoteTakingHelper::kDefaultLaunchResultHistogramName,
      static_cast<int>(LaunchResult::NO_APPS_AVAILABLE), 1);
}

TEST_P(NoteTakingHelperTest, NotifyObserverAboutAndroidApps) {
  Init(ENABLE_PALETTE | ENABLE_PLAY_STORE);
  TestObserver observer;

  // Let the app-fetching callback run and check that the observer is notified.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer.num_updates());

  // TODO(victorhsieh): Opt-out on Persistent ARC is special.  Skip until
  // implemented.
  if (arc::ShouldArcAlwaysStart())
    return;

  // Disabling and enabling Play Store should also notify the observer (and
  // enabling should request apps again).
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, false);
  EXPECT_EQ(2, observer.num_updates());
  profile()->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);
  EXPECT_EQ(3, observer.num_updates());
  // Run ARC data removing operation.
  base::RunLoop().RunUntilIdle();

  // Update intent filters and check that the observer is notified again after
  // apps are received.
  helper()->OnIntentFiltersUpdated();
  EXPECT_EQ(3, observer.num_updates());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, observer.num_updates());
}

TEST_P(NoteTakingHelperTest, NotifyObserverAboutChromeApps) {
  Init(ENABLE_PALETTE);
  TestObserver observer;
  ASSERT_EQ(0, observer.num_updates());

  // Notify that the prod Keep app was installed for the initial profile. Chrome
  // extensions are queried dynamically when GetAvailableApps() is called, so we
  // don't need to actually install it.
  auto keep_extension =
      CreateExtension(NoteTakingHelper::kProdKeepExtensionId, "Keep");
  InstallExtension(keep_extension.get(), profile());
  EXPECT_EQ(1, observer.num_updates());

  // Unloading the extension should also trigger a notification.
  UninstallExtension(keep_extension.get(), profile());
  EXPECT_EQ(2, observer.num_updates());

  // Non-whitelisted apps shouldn't trigger notifications.
  auto other_extension =
      CreateExtension(crx_file::id_util::GenerateId("a"), "Some Other App");
  InstallExtension(other_extension.get(), profile());
  EXPECT_EQ(2, observer.num_updates());
  UninstallExtension(other_extension.get(), profile());
  EXPECT_EQ(2, observer.num_updates());

  // Add a second profile and check that it triggers notifications too.
  observer.reset_num_updates();
  const std::string kSecondProfileName = "second-profile";
  TestingProfile* second_profile =
      profile_manager_->CreateTestingProfile(kSecondProfileName);
  InitExtensionService(second_profile);
  EXPECT_EQ(0, observer.num_updates());
  InstallExtension(keep_extension.get(), second_profile);
  EXPECT_EQ(1, observer.num_updates());
  UninstallExtension(keep_extension.get(), second_profile);
  EXPECT_EQ(2, observer.num_updates());
  profile_manager_->DeleteTestingProfile(kSecondProfileName);
}

}  // namespace chromeos
