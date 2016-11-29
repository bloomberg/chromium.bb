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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_service_launcher.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/policy/cloud/test_request_interceptor.h"
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
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/arc/arc_bridge_service_impl.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

constexpr char kRefreshToken[] = "fake-refresh-token";
// Set managed auth token for Android managed accounts.
constexpr char kManagedAuthToken[] = "managed-auth-token";
// Set unmanaged auth token for other Android unmanaged accounts.
constexpr char kUnmanagedAuthToken[] = "unmanaged-auth-token";
constexpr char kWellKnownConsumerName[] = "test@gmail.com";
constexpr char kFakeUserName[] = "test@example.com";

}  // namespace

namespace arc {

// Observer of ARC bridge shutdown.
class ArcSessionManagerShutdownObserver : public ArcSessionManager::Observer {
 public:
  ArcSessionManagerShutdownObserver() {
    ArcSessionManager::Get()->AddObserver(this);
  }

  ~ArcSessionManagerShutdownObserver() override {
    ArcSessionManager::Get()->RemoveObserver(this);
  }

  void Wait() {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    run_loop_.reset();
  }

  // ArcSessionManager::Observer:
  void OnShutdownBridge() override {
    if (!run_loop_)
      return;
    run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerShutdownObserver);
};

class ArcSessionManagerTest : public InProcessBrowserTest {
 protected:
  ArcSessionManagerTest() {}

  // InProcessBrowserTest:
  ~ArcSessionManagerTest() override {}

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
    command_line->AppendSwitch(chromeos::switches::kEnableArc);
    chromeos::FakeSessionManagerClient* const fake_session_manager_client =
        new chromeos::FakeSessionManagerClient;
    fake_session_manager_client->set_arc_available(true);
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<chromeos::SessionManagerClient>(
            fake_session_manager_client));

    // Mock out ARC bridge.
    // Here inject FakeArcSession so blocking task runner is not needed.
    auto service = base::MakeUnique<ArcBridgeServiceImpl>(nullptr);
    service->SetArcSessionFactoryForTesting(base::Bind(FakeArcSession::Create));
    ArcServiceManager::SetArcBridgeServiceForTesting(std::move(service));
  }

  void SetUpOnMainThread() override {
    user_manager_enabler_.reset(new chromeos::ScopedUserManagerEnabler(
        new chromeos::FakeChromeUserManager));
    // Init ArcSessionManager for testing.
    ArcSessionManager::DisableUIForTesting();
    ArcSessionManager::EnableCheckAndroidManagementForTesting();

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Create test profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetPath(temp_dir_.GetPath().AppendASCII("TestArcProfile"));
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
    ArcServiceLauncher::Get()->OnPrimaryUserProfilePrepared(profile());
  }

  void TearDownOnMainThread() override {
    ArcSessionManager::Get()->Shutdown();
    ArcServiceManager::Get()->Shutdown();
    profile_.reset();
    user_manager_enabler_.reset();
    test_server_.reset();
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
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestingProfile> profile_;
  FakeProfileOAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(ArcSessionManagerTest);
};

IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, ConsumerAccount) {
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcEnabled, true);
  token_service()->IssueTokenForAllPendingRequests(kUnmanagedAuthToken,
                                                   base::Time::Max());
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());
}

IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, WellKnownConsumerAccount) {
  set_profile_name(kWellKnownConsumerName);
  PrefService* const prefs = profile()->GetPrefs();

  prefs->SetBoolean(prefs::kArcEnabled, true);
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());
}

IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, ManagedChromeAccount) {
  policy::ProfilePolicyConnector* const connector =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(profile());
  connector->OverrideIsManagedForTesting(true);

  PrefService* const pref = profile()->GetPrefs();

  pref->SetBoolean(prefs::kArcEnabled, true);
  ASSERT_EQ(ArcSessionManager::State::ACTIVE,
            ArcSessionManager::Get()->state());
}

IN_PROC_BROWSER_TEST_F(ArcSessionManagerTest, ManagedAndroidAccount) {
  PrefService* const prefs = profile()->GetPrefs();

  prefs->SetBoolean(prefs::kArcEnabled, true);
  token_service()->IssueTokenForAllPendingRequests(kManagedAuthToken,
                                                   base::Time::Max());
  ArcSessionManagerShutdownObserver observer;
  observer.Wait();
  ASSERT_EQ(ArcSessionManager::State::STOPPED,
            ArcSessionManager::Get()->state());
}

}  // namespace arc
