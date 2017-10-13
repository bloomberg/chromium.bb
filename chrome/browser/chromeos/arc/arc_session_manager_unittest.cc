// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/optin/arc_terms_of_service_oobe_negotiator.h"
#include "chrome/browser/chromeos/arc/test/arc_data_removed_waiter.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen_view.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen_view_observer.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_session_runner.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/sync/model/fake_sync_change_processor.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class FakeBaseScreen : public chromeos::BaseScreen {
 public:
  explicit FakeBaseScreen(chromeos::OobeScreen screen_id)
      : BaseScreen(nullptr, screen_id) {}

  ~FakeBaseScreen() override = default;

  // chromeos::BaseScreen:
  void Show() override {}
  void Hide() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBaseScreen);
};

class FakeLoginDisplayHost : public chromeos::LoginDisplayHost {
 public:
  FakeLoginDisplayHost() {
    DCHECK(!chromeos::LoginDisplayHost::default_host_);
    chromeos::LoginDisplayHost::default_host_ = this;
  }

  ~FakeLoginDisplayHost() override {
    DCHECK_EQ(chromeos::LoginDisplayHost::default_host_, this);
    chromeos::LoginDisplayHost::default_host_ = nullptr;
  }

  /// chromeos::LoginDisplayHost:
  chromeos::LoginDisplay* CreateLoginDisplay(
      chromeos::LoginDisplay::Delegate* delegate) override {
    return nullptr;
  }
  gfx::NativeWindow GetNativeWindow() const override { return nullptr; }
  chromeos::OobeUI* GetOobeUI() const override { return nullptr; }
  chromeos::WebUILoginView* GetWebUILoginView() const override {
    return nullptr;
  }
  void BeforeSessionStart() override {}
  void Finalize(base::OnceClosure) override {}
  void OpenInternetDetailDialog(const std::string& network_id) override {}
  void SetStatusAreaVisible(bool visible) override {}
  void StartWizard(chromeos::OobeScreen first_screen) override {
    // Reset the controller first since there could only be one wizard
    // controller at any time.
    wizard_controller_.reset();
    wizard_controller_ =
        std::make_unique<chromeos::WizardController>(nullptr, nullptr);

    fake_screen_ = std::make_unique<FakeBaseScreen>(first_screen);
    wizard_controller_->SetCurrentScreenForTesting(fake_screen_.get());
  }
  chromeos::WizardController* GetWizardController() override {
    return wizard_controller_.get();
  }
  chromeos::AppLaunchController* GetAppLaunchController() override {
    return nullptr;
  }
  void StartUserAdding(base::OnceClosure completion_callback) override {}
  void CancelUserAdding() override {}
  void StartSignInScreen(const chromeos::LoginScreenContext& context) override {
  }
  void OnPreferencesChanged() override {}
  void PrewarmAuthentication() override {}
  void StartAppLaunch(const std::string& app_id,
                      bool diagnostic_mode,
                      bool is_auto_launch) override {}
  void StartDemoAppLaunch() override {}
  void StartArcKiosk(const AccountId& account_id) override {}
  void StartVoiceInteractionOobe() override {
    is_voice_interaction_oobe_ = true;
  }
  bool IsVoiceInteractionOobe() override { return is_voice_interaction_oobe_; }

 private:
  bool is_voice_interaction_oobe_ = false;

  // SessionManager is required by the constructor of WizardController.
  std::unique_ptr<session_manager::SessionManager> session_manager_ =
      std::make_unique<session_manager::SessionManager>();
  std::unique_ptr<FakeBaseScreen> fake_screen_;
  std::unique_ptr<chromeos::WizardController> wizard_controller_;

  DISALLOW_COPY_AND_ASSIGN(FakeLoginDisplayHost);
};

}  // namespace

class ArcSessionManagerTestBase : public testing::Test {
 public:
  ArcSessionManagerTestBase()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        user_manager_enabler_(new chromeos::FakeChromeUserManager()) {}
  ~ArcSessionManagerTestBase() override = default;

  void SetUp() override {
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::make_unique<chromeos::FakeSessionManagerClient>());

    chromeos::DBusThreadManager::Initialize();

    SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ArcSessionManager::DisableUIForTesting();
    SetArcBlockedDueToIncompatibleFileSystemForTesting(false);

    arc_service_manager_ = std::make_unique<ArcServiceManager>();
    arc_session_manager_ = std::make_unique<ArcSessionManager>(
        std::make_unique<ArcSessionRunner>(base::Bind(FakeArcSession::Create)));

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("user@gmail.com");
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));

    profile_ = profile_builder.Build();
    StartPreferenceSyncing();

    ASSERT_FALSE(arc_session_manager_->enable_requested());
    chromeos::WallpaperManager::Initialize();
  }

  void TearDown() override {
    chromeos::WallpaperManager::Shutdown();
    arc_session_manager_->Shutdown();
    profile_.reset();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

 protected:
  TestingProfile* profile() { return profile_.get(); }

  ArcSessionManager* arc_session_manager() {
    return arc_session_manager_.get();
  }

  bool WaitForDataRemoved(ArcSessionManager::State expected_state) {
    if (arc_session_manager()->state() !=
        ArcSessionManager::State::REMOVING_DATA_DIR)
      return false;

    base::RunLoop().RunUntilIdle();
    if (arc_session_manager()->state() != expected_state)
      return false;

    return true;
  }

 private:
  void StartPreferenceSyncing() const {
    PrefServiceSyncableFromProfile(profile_.get())
        ->GetSyncableService(syncer::PREFERENCES)
        ->MergeDataAndStartSyncing(
            syncer::PREFERENCES, syncer::SyncDataList(),
            std::make_unique<syncer::FakeSyncChangeProcessor>(),
            std::make_unique<syncer::SyncErrorFactoryMock>());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcSessionManager> arc_session_manager_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerTestBase);
};

class ArcSessionManagerTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerTest() = default;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();

    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    ASSERT_EQ(ArcSessionManager::State::NOT_INITIALIZED,
              arc_session_manager()->state());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerTest);
};

TEST_F(ArcSessionManagerTest, BaseWorkflow) {
  EXPECT_TRUE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->arc_start_time().is_null());

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // By default ARC is not enabled.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  // Enables ARC. First time, ToS negotiation should start.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  ASSERT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  arc_session_manager()->StartArcForTesting();

  EXPECT_TRUE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_FALSE(arc_session_manager()->arc_start_time().is_null());

  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, IncompatibleFileSystemBlocksTermsOfService) {
  SetArcBlockedDueToIncompatibleFileSystemForTesting(true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // Enables ARC first time. ToS negotiation should NOT happen.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, IncompatibleFileSystemBlocksArcStart) {
  SetArcBlockedDueToIncompatibleFileSystemForTesting(true);

  // Set up the situation that provisioning is successfully done in the
  // previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // Enables ARC second time. ARC should NOT start.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, CancelFetchingDisablesArc) {
  SetArcPlayStoreEnabledForProfile(profile(), true);

  // Starts ARC.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  // Emulate to cancel the ToS UI (e.g. closing the window).
  arc_session_manager()->CancelAuthCode();

  // Google Play Store enabled preference should be set to false, too.
  EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));

  // Emulate the preference handling.
  arc_session_manager()->RequestDisable();

  // Wait until data is removed.
  ASSERT_TRUE(WaitForDataRemoved(ArcSessionManager::State::STOPPED));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, CloseUIKeepsArcEnabled) {
  // Starts ARC.
  SetArcPlayStoreEnabledForProfile(profile(), true);
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  ASSERT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  arc_session_manager()->StartArcForTesting();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // When ARC is properly started, closing UI should be no-op.
  arc_session_manager()->CancelAuthCode();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, Provisioning_Success) {
  PrefService* const prefs = profile()->GetPrefs();

  EXPECT_TRUE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->arc_start_time().is_null());
  EXPECT_FALSE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());

  ASSERT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  // Emulate to accept the terms of service.
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  ASSERT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Here, provisining is not yet completed, so kArcSignedIn should be false.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_FALSE(arc_session_manager()->arc_start_time().is_null());
  EXPECT_FALSE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());

  // Emulate successful provisioning.
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_TRUE(arc_session_manager()->sign_in_start_time().is_null());
  EXPECT_TRUE(arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());
}

TEST_F(ArcSessionManagerTest, Provisioning_Restart) {
  // Set up the situation that provisioning is successfully done in the
  // previous session.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();

  // Second start, no fetching code is expected.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Report failure.
  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::GMS_NETWORK_ERROR);
  // On error, UI to send feedback is showing. In that case,
  // the ARC is still necessary to run on background for gathering the logs.
  EXPECT_TRUE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Correctly stop service.
  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, RemoveDataDir) {
  // Emulate the situation where the initial Google Play Store enabled
  // preference is false for managed user, i.e., data dir is being removed at
  // beginning.
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestArcDataRemoval();
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());

  // Enable ARC. Data is removed asyncronously. At this moment session manager
  // should be in REMOVING_DATA_DIR state.
  arc_session_manager()->RequestEnable();
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::REMOVING_DATA_DIR,
            arc_session_manager()->state());
  // Wait until data is removed.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  ASSERT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Request to remove data and stop session manager.
  arc_session_manager()->RequestArcDataRemoval();
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  arc_session_manager()->Shutdown();
  base::RunLoop().RunUntilIdle();
  // Request should persist.
  ASSERT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
}

TEST_F(ArcSessionManagerTest, RemoveDataDir_Restart) {
  // Emulate second sign-in. Data should be removed first and ARC started after.
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcDataRemoveRequested, true);
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_TRUE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));
  ASSERT_TRUE(WaitForDataRemoved(
      ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE));
  EXPECT_FALSE(
      profile()->GetPrefs()->GetBoolean(prefs::kArcDataRemoveRequested));

  arc_session_manager()->Shutdown();
}

TEST_F(ArcSessionManagerTest, IgnoreSecondErrorReporting) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Report some failure that does not stop the bridge.
  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::GMS_SIGN_IN_FAILED);
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Try to send another error that stops the bridge if sent first. It should
  // be ignored.
  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  arc_session_manager()->Shutdown();
}

// Test case when directly started flag is not set during the ARC boot.
TEST_F(ArcSessionManagerTest, IsDirectlyStartedFalse) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // On initial start directy started flag is not set.
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  arc_session_manager()->Shutdown();
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
}

// Test case when directly started flag is set during the ARC boot.
// Preconditions are: ToS accepted and ARC was signed in.
TEST_F(ArcSessionManagerTest, IsDirectlyStartedTrue) {
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcTermsAccepted, true);
  prefs->SetBoolean(prefs::kArcSignedIn, true);

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(arc_session_manager()->is_directly_started());
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Disabling ARC turns directy started flag off.
  arc_session_manager()->RequestDisable();
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  arc_session_manager()->Shutdown();
}

// Test case when directly started flag is preserved during the internal ARC
// restart.
TEST_F(ArcSessionManagerTest, IsDirectlyStartedOnInternalRestart) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_session_manager()->is_directly_started());

  // Simualate internal restart.
  arc_session_manager()->StopAndEnableArc();
  // Fake ARC session implementation synchronously calls stop callback and
  // session manager should be reactivated at this moment.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  // directy started flag should be preserved.
  EXPECT_FALSE(arc_session_manager()->is_directly_started());
  arc_session_manager()->Shutdown();
}

class ArcSessionManagerArcAlwaysStartTest : public ArcSessionManagerTest {
 public:
  ArcSessionManagerArcAlwaysStartTest() = default;

  void SetUp() override {
    SetArcAlwaysStartForTesting(true);
    ArcSessionManagerTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerArcAlwaysStartTest);
};

TEST_F(ArcSessionManagerArcAlwaysStartTest, BaseWorkflow) {
  // TODO(victorhsieh): Consider also tracking sign-in activity, which is
  // initiated from the Android side.
  EXPECT_TRUE(arc_session_manager()->arc_start_time().is_null());

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();

  // By default ARC is not enabled.
  EXPECT_EQ(ArcSessionManager::State::STOPPED, arc_session_manager()->state());

  // When ARC is always started, ArcSessionManager should always be in ACTIVE
  // state.
  arc_session_manager()->RequestEnable();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(arc_session_manager()->arc_start_time().is_null());

  arc_session_manager()->Shutdown();
}

class ArcSessionManagerPolicyTest
    : public ArcSessionManagerTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool, int, int>> {
 public:
  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();
    AccountId account_id;
    if (is_active_directory_user()) {
      account_id = AccountId(AccountId::AdFromUserEmailObjGuid(
          profile()->GetProfileUserName(), "1234567890"));
    } else {
      account_id = AccountId(AccountId::FromUserEmailGaiaId(
          profile()->GetProfileUserName(), "1234567890"));
    }
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

  bool arc_enabled_pref_managed() const { return std::get<0>(GetParam()); }

  bool is_active_directory_user() const { return std::get<1>(GetParam()); }

  base::Value backup_restore_pref_value() const {
    switch (std::get<2>(GetParam())) {
      case 0:
        return base::Value();
      case 1:
        return base::Value(false);
      case 2:
        return base::Value(true);
    }
    NOTREACHED();
    return base::Value();
  }

  base::Value location_service_pref_value() const {
    switch (std::get<3>(GetParam())) {
      case 0:
        return base::Value();
      case 1:
        return base::Value(false);
      case 2:
        return base::Value(true);
    }
    NOTREACHED();
    return base::Value();
  }
};

TEST_P(ArcSessionManagerPolicyTest, SkippingTerms) {
  sync_preferences::TestingPrefServiceSyncable* const prefs =
      profile()->GetTestingPrefService();

  // Backup-restore and location-service prefs are off by default.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcSignedIn));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcTermsAccepted));

  EXPECT_EQ(is_active_directory_user(),
            IsActiveDirectoryUserForProfile(profile()));

  // Enable ARC through user pref or by policy, according to the test parameter.
  if (arc_enabled_pref_managed())
    prefs->SetManagedPref(prefs::kArcEnabled,
                          std::make_unique<base::Value>(true));
  else
    prefs->SetBoolean(prefs::kArcEnabled, true);
  EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));

  // Assign test values to the prefs.
  if (backup_restore_pref_value().is_bool()) {
    prefs->SetManagedPref(prefs::kArcBackupRestoreEnabled,
                          backup_restore_pref_value().CreateDeepCopy());
  }
  if (location_service_pref_value().is_bool()) {
    prefs->SetManagedPref(prefs::kArcLocationServiceEnabled,
                          location_service_pref_value().CreateDeepCopy());
  }

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();

  // Terms of Service are skipped iff ARC is enabled by policy and both policies
  // are either managed or unused (for Active Directory users a LaForge account
  // is created, not a full Dasher account, where the policies have no meaning).
  const bool prefs_managed = backup_restore_pref_value().is_bool() &&
                             location_service_pref_value().is_bool();
  const bool prefs_unused = is_active_directory_user();
  const bool expected_terms_skipping =
      (arc_enabled_pref_managed() && (prefs_managed || prefs_unused));
  EXPECT_EQ(expected_terms_skipping
                ? ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT
                : ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());

  // Complete provisioning if it's not done yet.
  if (!expected_terms_skipping)
    arc_session_manager()->OnTermsOfServiceNegotiatedForTesting(true);
  arc_session_manager()->StartArcForTesting();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  arc_session_manager()->OnProvisioningFinished(ProvisioningResult::SUCCESS);

  // Play Store app is launched unless the Terms screen was suppressed.
  EXPECT_NE(expected_terms_skipping,
            arc_session_manager()->IsPlaystoreLaunchRequestedForTesting());

  // Managed values for the prefs are unset.
  prefs->RemoveManagedPref(prefs::kArcBackupRestoreEnabled);
  prefs->RemoveManagedPref(prefs::kArcLocationServiceEnabled);

  // The ARC state is preserved. The prefs return to the default false values.
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcBackupRestoreEnabled));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kArcLocationServiceEnabled));

  // Stop ARC and shutdown the service.
  prefs->RemoveManagedPref(prefs::kArcEnabled);
  WaitForDataRemoved(ArcSessionManager::State::STOPPED);
  arc_session_manager()->Shutdown();
}

TEST_P(ArcSessionManagerPolicyTest, ReenableManagedArc) {
  sync_preferences::TestingPrefServiceSyncable* const prefs =
      profile()->GetTestingPrefService();

  // Set ARC to be managed.
  prefs->SetManagedPref(prefs::kArcEnabled,
                        std::make_unique<base::Value>(true));
  EXPECT_TRUE(arc::IsArcPlayStoreEnabledForProfile(profile()));

  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_TRUE(arc_session_manager()->enable_requested());

  // Simulate close OptIn. Session manager should stop.
  SetArcPlayStoreEnabledForProfile(profile(), false);
  EXPECT_TRUE(arc::IsArcPlayStoreEnabledForProfile(profile()));
  EXPECT_FALSE(arc_session_manager()->enable_requested());

  // Restart ARC again
  SetArcPlayStoreEnabledForProfile(profile(), true);
  EXPECT_TRUE(arc::IsArcPlayStoreEnabledForProfile(profile()));
  EXPECT_TRUE(arc_session_manager()->enable_requested());

  arc_session_manager()->Shutdown();
}

INSTANTIATE_TEST_CASE_P(
    ,
    ArcSessionManagerPolicyTest,
    // testing::Values is incompatible with move-only types, hence ints are used
    // as a proxy for base::Value.
    testing::Combine(testing::Bool() /* arc_enabled_pref_managed */,
                     testing::Bool() /* is_active_directory_user */,
                     /* backup_restore_pref_value */
                     testing::Values(0,   // base::Value()
                                     1,   // base::Value(false)
                                     2),  // base::Value(true)
                     /* location_service_pref_value */
                     testing::Values(0,     // base::Value()
                                     1,     // base::Value(false)
                                     2)));  // base::Value(true)

class ArcSessionManagerKioskTest : public ArcSessionManagerTestBase {
 public:
  ArcSessionManagerKioskTest() = default;

  void SetUp() override {
    ArcSessionManagerTestBase::SetUp();
    const AccountId account_id(
        AccountId::FromUserEmail(profile()->GetProfileUserName()));
    GetFakeUserManager()->AddArcKioskAppUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerKioskTest);
};

TEST_F(ArcSessionManagerKioskTest, AuthFailure) {
  arc_session_manager()->SetProfile(profile());
  arc_session_manager()->Initialize();
  arc_session_manager()->RequestEnable();
  EXPECT_EQ(ArcSessionManager::State::ACTIVE, arc_session_manager()->state());

  // Replace chrome::AttemptUserExit() for testing.
  // At the end of test, leave the dangling pointer |terminated|,
  // assuming the callback is invoked exactly once in OnProvisioningFinished()
  // and not invoked then, including TearDown().
  bool terminated = false;
  arc_session_manager()->SetAttemptUserExitCallbackForTesting(
      base::Bind([](bool* terminated) { *terminated = true; }, &terminated));

  arc_session_manager()->OnProvisioningFinished(
      ProvisioningResult::CHROME_SERVER_COMMUNICATION_ERROR);
  EXPECT_TRUE(terminated);
}

class ArcSessionOobeOptInTest : public ArcSessionManagerTest {
 public:
  ArcSessionOobeOptInTest() = default;

 protected:
  void CreateLoginDisplayHost() {
    fake_login_display_host_ = std::make_unique<FakeLoginDisplayHost>();
  }

  FakeLoginDisplayHost* login_display_host() {
    return fake_login_display_host_.get();
  }

  void CloseLoginDisplayHost() { fake_login_display_host_.reset(); }

  void AppendEnableArcOOBEOptInSwitch() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        chromeos::switches::kEnableArcOOBEOptIn);
  }

 private:
  std::unique_ptr<FakeLoginDisplayHost> fake_login_display_host_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionOobeOptInTest);
};

TEST_F(ArcSessionOobeOptInTest, OobeOptInActive) {
  // OOBE OptIn is active in case of OOBE controller is alive and the ARC ToS
  // screen is currently showing.
  EXPECT_FALSE(ArcSessionManager::IsOobeOptInActive());
  CreateLoginDisplayHost();
  EXPECT_FALSE(ArcSessionManager::IsOobeOptInActive());
  GetFakeUserManager()->set_current_user_new(true);
  EXPECT_FALSE(ArcSessionManager::IsOobeOptInActive());
  AppendEnableArcOOBEOptInSwitch();
  EXPECT_TRUE(ArcSessionManager::IsOobeOptInActive());
  login_display_host()->StartVoiceInteractionOobe();
  EXPECT_FALSE(ArcSessionManager::IsOobeOptInActive());
  login_display_host()->StartWizard(
      chromeos::OobeScreen::SCREEN_VOICE_INTERACTION_VALUE_PROP);
  EXPECT_FALSE(ArcSessionManager::IsOobeOptInActive());
  login_display_host()->StartWizard(
      chromeos::OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE);
  EXPECT_TRUE(ArcSessionManager::IsOobeOptInActive());
}

class ArcSessionOobeOptInNegotiatorTest
    : public ArcSessionOobeOptInTest,
      public chromeos::ArcTermsOfServiceScreenView,
      public testing::WithParamInterface<bool> {
 public:
  ArcSessionOobeOptInNegotiatorTest() = default;

  void SetUp() override {
    ArcSessionOobeOptInTest::SetUp();

    AppendEnableArcOOBEOptInSwitch();

    ArcTermsOfServiceOobeNegotiator::SetArcTermsOfServiceScreenViewForTesting(
        this);

    GetFakeUserManager()->set_current_user_new(true);

    CreateLoginDisplayHost();

    if (IsManagedUser()) {
      policy::ProfilePolicyConnector* const connector =
          policy::ProfilePolicyConnectorFactory::GetForBrowserContext(
              profile());
      connector->OverrideIsManagedForTesting(true);

      profile()->GetTestingPrefService()->SetManagedPref(
          prefs::kArcEnabled, std::make_unique<base::Value>(true));
    }

    arc_session_manager()->SetProfile(profile());
    arc_session_manager()->Initialize();

    if (IsArcPlayStoreEnabledForProfile(profile()))
      arc_session_manager()->RequestEnable();
  }

  void TearDown() override {
    // Correctly stop service.
    arc_session_manager()->Shutdown();

    ArcTermsOfServiceOobeNegotiator::SetArcTermsOfServiceScreenViewForTesting(
        nullptr);

    ArcSessionOobeOptInTest::TearDown();
  }

 protected:
  bool IsManagedUser() { return GetParam(); }

  void ReportResult(bool accepted) {
    for (auto& observer : observer_list_) {
      if (accepted)
        observer.OnAccept();
      else
        observer.OnSkip();
    }
    base::RunLoop().RunUntilIdle();
  }

  void ReportViewDestroyed() {
    for (auto& observer : observer_list_)
      observer.OnViewDestroyed(this);
    base::RunLoop().RunUntilIdle();
  }

  chromeos::ArcTermsOfServiceScreenView* view() { return this; }

 private:
  // ArcTermsOfServiceScreenView:
  void AddObserver(
      chromeos::ArcTermsOfServiceScreenViewObserver* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(
      chromeos::ArcTermsOfServiceScreenViewObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void Show() override {
    // To match ArcTermsOfServiceScreenHandler logic where Google Play Store
    // enabled preferencee is set to true on showing UI, which eventually
    // triggers to call RequestEnable().
    arc_session_manager()->RequestEnable();
  }

  void Hide() override {}

  base::ObserverList<chromeos::ArcTermsOfServiceScreenViewObserver>
      observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionOobeOptInNegotiatorTest);
};

INSTANTIATE_TEST_CASE_P(,
                        ArcSessionOobeOptInNegotiatorTest,
                        ::testing::Values(true, false));

TEST_P(ArcSessionOobeOptInNegotiatorTest, OobeTermsAccepted) {
  view()->Show();
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  ReportResult(true);
  EXPECT_EQ(ArcSessionManager::State::CHECKING_ANDROID_MANAGEMENT,
            arc_session_manager()->state());
}

TEST_P(ArcSessionOobeOptInNegotiatorTest, OobeTermsRejected) {
  view()->Show();
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  ReportResult(false);
  if (!IsManagedUser()) {
    // ArcPlayStoreEnabledPreferenceHandler is not running, so the state should
    // be kept as is
    EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
              arc_session_manager()->state());
    EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
  } else {
    // For managed case we handle closing outside of
    // ArcPlayStoreEnabledPreferenceHandler. So it session turns to STOPPED.
    EXPECT_EQ(ArcSessionManager::State::STOPPED,
              arc_session_manager()->state());
    // Managed user's preference should not be overwritten.
    EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));
  }
}

TEST_P(ArcSessionOobeOptInNegotiatorTest, OobeTermsViewDestroyed) {
  view()->Show();
  EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
            arc_session_manager()->state());
  CloseLoginDisplayHost();
  ReportViewDestroyed();
  if (!IsManagedUser()) {
    // ArcPlayStoreEnabledPreferenceHandler is not running, so the state should
    // be kept as is.
    EXPECT_EQ(ArcSessionManager::State::NEGOTIATING_TERMS_OF_SERVICE,
              arc_session_manager()->state());
    EXPECT_FALSE(IsArcPlayStoreEnabledForProfile(profile()));
  } else {
    // For managed case we handle closing outside of
    // ArcPlayStoreEnabledPreferenceHandler. So it session turns to STOPPED.
    EXPECT_EQ(ArcSessionManager::State::STOPPED,
              arc_session_manager()->state());
    // Managed user's preference should not be overwritten.
    EXPECT_TRUE(IsArcPlayStoreEnabledForProfile(profile()));
  }
}

}  // namespace arc
