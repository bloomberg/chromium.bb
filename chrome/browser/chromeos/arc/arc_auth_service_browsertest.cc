// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service_impl.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/test/fake_arc_bridge_bootstrap.h"
#include "components/arc/test/fake_arc_bridge_instance.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/user_manager/user_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kRefreshToken[] = "fake-refresh-token";
// Set managed auth token for Android managed accounts.
const char kManagedAuthToken[] = "managed-auth-token";
// Set unmanaged auth token for other Android unmanaged accounts.
const char kUnmanagedAuthToken[] = "unmanaged-auth-token";
const char kWellKnownConsumerName[] = "test@gmail.com";
const char kFakeUserName[] = "test@example.com";

}  // namespace

namespace arc {

// Base ArcAuthService observer.
class ArcAuthServiceObserver : public ArcAuthService::Observer {
 public:
  // ArcAuthService::Observer:
  ~ArcAuthServiceObserver() override {
    ArcAuthService::Get()->RemoveObserver(this);
  }

  void Wait() {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    run_loop_.reset();
  }

 protected:
  ArcAuthServiceObserver() { ArcAuthService::Get()->AddObserver(this); }

  std::unique_ptr<base::RunLoop> run_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAuthServiceObserver);
};

// Observer of ArcAuthService state change.
class ArcAuthServiceStateObserver : public ArcAuthServiceObserver {
 public:
  ArcAuthServiceStateObserver() : ArcAuthServiceObserver() {}

  // ArcAuthService::Observer:
  void OnOptInChanged(ArcAuthService::State state) override {
    if (!run_loop_)
      return;
    run_loop_->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAuthServiceStateObserver);
};

// Observer of ARC bridge shutdown.
class ArcAuthServiceShutdownObserver : public ArcAuthServiceObserver {
 public:
  ArcAuthServiceShutdownObserver() : ArcAuthServiceObserver() {}

  // ArcAuthService::Observer:
  void OnShutdownBridge() override {
    if (!run_loop_)
      return;
    run_loop_->Quit();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAuthServiceShutdownObserver);
};

class ArcAuthServiceTest : public InProcessBrowserTest {
 protected:
  ArcAuthServiceTest() {}

  // InProcessBrowserTest:
  ~ArcAuthServiceTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    // Start test device management server.
    test_server_.reset(new policy::LocalPolicyTestServer());
    ASSERT_TRUE(test_server_->Start());

    // Specify device management server URL.
    std::string url = test_server_->GetServiceURL().spec();
    base::CommandLine* const command_line =
        base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    url);

    // Enable ARC.
    chromeos::FakeSessionManagerClient* const fake_session_manager_client =
        new chromeos::FakeSessionManagerClient;
    fake_session_manager_client->set_arc_available(true);
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<chromeos::SessionManagerClient>(
            fake_session_manager_client));

    // Mock out ARC bridge.
    fake_arc_bridge_instance_.reset(new FakeArcBridgeInstance);
    ArcServiceManager::SetArcBridgeServiceForTesting(
        base::WrapUnique(new ArcBridgeServiceImpl(base::WrapUnique(
            new FakeArcBridgeBootstrap(fake_arc_bridge_instance_.get())))));
  }

  void SetUpOnMainThread() override {
    user_manager_enabler_.reset(new chromeos::ScopedUserManagerEnabler(
        new chromeos::FakeChromeUserManager));
    // Init ArcAuthService for testing.
    ArcAuthService::DisableUIForTesting();
    ArcAuthService::EnableCheckAndroidManagementForTesting();

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Create test profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.path().AppendASCII("TestArcProfile"));
    profile_builder.SetProfileName(kFakeUserName);
    profile_builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        BuildFakeProfileOAuth2TokenService);
    profile_ = profile_builder.Build();
    token_service_ = static_cast<FakeProfileOAuth2TokenService*>(
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile()));
    token_service_->UpdateCredentials("", kRefreshToken);

    profile()->GetPrefs()->SetBoolean(prefs::kArcSignedIn, true);

    const AccountId account_id(
        AccountId::FromUserEmailGaiaId(kFakeUserName, "1234567890"));
    GetFakeUserManager()->AddUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    // Set up ARC for test profile.
    ArcServiceManager::Get()->OnPrimaryUserProfilePrepared(
        multi_user_util::GetAccountIdFromProfile(profile()));
    ArcAuthService::Get()->OnPrimaryUserProfilePrepared(profile());
  }

  void TearDownOnMainThread() override {
    ArcAuthService::Get()->Shutdown();
    profile_.reset();
    user_manager_enabler_.reset();
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  void set_profile_name(const std::string& username) {
    profile_->set_profile_name(username);
  }

  Profile* profile() { return profile_.get(); }

  FakeProfileOAuth2TokenService* token_service() { return token_service_; }

 private:
  std::unique_ptr<policy::LocalPolicyTestServer> test_server_;
  std::unique_ptr<FakeArcBridgeInstance> fake_arc_bridge_instance_;
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  FakeProfileOAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthServiceTest);
};

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, ConsumerAccount) {
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcEnabled, true);
  token_service()->IssueTokenForAllPendingRequests(kUnmanagedAuthToken,
                                                   base::Time::Max());
  ArcAuthServiceStateObserver observer;
  observer.Wait();
  ASSERT_EQ(ArcAuthService::State::ACTIVE, ArcAuthService::Get()->state());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, WellKnownConsumerAccount) {
  set_profile_name(kWellKnownConsumerName);
  PrefService* const prefs = profile()->GetPrefs();

  prefs->SetBoolean(prefs::kArcEnabled, true);
  ASSERT_EQ(ArcAuthService::State::ACTIVE, ArcAuthService::Get()->state());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, ManagedChromeAccount) {
  policy::ProfilePolicyConnector* const connector =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile());
  connector->OverrideIsManagedForTesting(true);

  PrefService* const pref = profile()->GetPrefs();

  pref->SetBoolean(prefs::kArcEnabled, true);
  ASSERT_EQ(ArcAuthService::State::ACTIVE, ArcAuthService::Get()->state());
}

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, ManagedAndroidAccount) {
  PrefService* const prefs = profile()->GetPrefs();

  prefs->SetBoolean(prefs::kArcEnabled, true);
  token_service()->IssueTokenForAllPendingRequests(kManagedAuthToken,
                                                   base::Time::Max());
  ArcAuthServiceShutdownObserver observer;
  observer.Wait();
  ASSERT_EQ(ArcAuthService::State::STOPPED, ArcAuthService::Get()->state());
}

}  // namespace arc
