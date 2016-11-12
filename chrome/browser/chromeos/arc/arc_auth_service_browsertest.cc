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
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
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
constexpr char kFakeAuthCode[] = "fake-auth-code";

// JobCallback for the interceptor.
net::URLRequestJob* ResponseJob(net::URLRequest* request,
                                net::NetworkDelegate* network_delegate) {
  const net::UploadDataStream* upload = request->get_upload();
  if (!upload || !upload->GetElementReaders() ||
      upload->GetElementReaders()->size() != 1 ||
      !(*upload->GetElementReaders())[0]->AsBytesReader())
    return nullptr;

  const net::UploadBytesElementReader* bytes_reader =
      (*upload->GetElementReaders())[0]->AsBytesReader();

  enterprise_management::DeviceManagementRequest parsed_request;
  EXPECT_TRUE(parsed_request.ParseFromArray(bytes_reader->bytes(),
                                            bytes_reader->length()));
  // Check if auth code is requested.
  EXPECT_TRUE(parsed_request.has_service_api_access_request());

  enterprise_management::DeviceManagementResponse response;
  response.mutable_service_api_access_response()->set_auth_code(kFakeAuthCode);

  std::string response_data;
  EXPECT_TRUE(response.SerializeToString(&response_data));

  return new net::URLRequestTestJob(request, network_delegate,
                                    net::URLRequestTestJob::test_headers(),
                                    response_data, true);
}

class FakeAuthInstance : public arc::mojom::AuthInstance {
 public:
  void Init(arc::mojom::AuthHostPtr host_ptr) override {}
  void OnAccountInfoReady(arc::mojom::AccountInfoPtr account_info) override {
    ASSERT_FALSE(callback.is_null());
    callback.Run(account_info);
  }
  base::Callback<void(const arc::mojom::AccountInfoPtr&)> callback;
};

}  // namespace

namespace arc {

// Observer of ARC bridge shutdown.
class ArcAuthServiceShutdownObserver : public ArcAuthService::Observer {
 public:
  ArcAuthServiceShutdownObserver() { ArcAuthService::Get()->AddObserver(this); }

  ~ArcAuthServiceShutdownObserver() override {
    ArcAuthService::Get()->RemoveObserver(this);
  }

  void Wait() {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
    run_loop_.reset();
  }

  // ArcAuthService::Observer:
  void OnShutdownBridge() override {
    if (!run_loop_)
      return;
    run_loop_->Quit();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;

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
    // Init ArcAuthService for testing.
    ArcAuthService::DisableUIForTesting();
    ArcAuthService::EnableCheckAndroidManagementForTesting();

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
    std::unique_ptr<BooleanPrefMember> arc_enabled_pref =
        base::MakeUnique<BooleanPrefMember>();
    arc_enabled_pref->Init(prefs::kArcEnabled, profile()->GetPrefs());
    ArcServiceManager::Get()->OnPrimaryUserProfilePrepared(
        multi_user_util::GetAccountIdFromProfile(profile()),
        std::move(arc_enabled_pref));
    ArcAuthService::Get()->OnPrimaryUserProfilePrepared(profile());
  }

  void TearDownOnMainThread() override {
    ArcAuthService::Get()->Shutdown();
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

  DISALLOW_COPY_AND_ASSIGN(ArcAuthServiceTest);
};

IN_PROC_BROWSER_TEST_F(ArcAuthServiceTest, ConsumerAccount) {
  PrefService* const prefs = profile()->GetPrefs();
  prefs->SetBoolean(prefs::kArcEnabled, true);
  token_service()->IssueTokenForAllPendingRequests(kUnmanagedAuthToken,
                                                   base::Time::Max());
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

class KioskArcAuthServiceTest : public InProcessBrowserTest {
 protected:
  KioskArcAuthServiceTest() = default;

  // InProcessBrowserTest:
  ~KioskArcAuthServiceTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                    "http://localhost");
    command_line->AppendSwitch(chromeos::switches::kEnableArc);
  }

  void SetUpOnMainThread() override {
    interceptor_.reset(new policy::TestRequestInterceptor(
        "localhost", content::BrowserThread::GetTaskRunnerForThread(
                         content::BrowserThread::IO)));

    user_manager_enabler_.reset(new chromeos::ScopedUserManagerEnabler(
        new chromeos::FakeChromeUserManager));

    const AccountId account_id(AccountId::FromUserEmail(kFakeUserName));
    GetFakeUserManager()->AddArcKioskAppUser(account_id);
    GetFakeUserManager()->LoginUser(account_id);

    policy::BrowserPolicyConnectorChromeOS* const connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    policy::DeviceCloudPolicyManagerChromeOS* const cloud_policy_manager =
        connector->GetDeviceCloudPolicyManager();

    cloud_policy_manager->StartConnection(
        base::MakeUnique<policy::MockCloudPolicyClient>(),
        connector->GetInstallAttributes());

    policy::MockCloudPolicyClient* const cloud_policy_client =
        static_cast<policy::MockCloudPolicyClient*>(
            cloud_policy_manager->core()->client());
    cloud_policy_client->SetDMToken("fake-dm-token");
    cloud_policy_client->client_id_ = "client-id";

    ArcBridgeService::Get()->auth()->SetInstance(&auth_instance_);
  }

  void TearDownOnMainThread() override {
    ArcBridgeService::Get()->auth()->SetInstance(nullptr);
    ArcAuthService::Get()->Shutdown();
    ArcServiceManager::Get()->Shutdown();
    user_manager_enabler_.reset();

    // Verify that all the expected requests were handled.
    EXPECT_EQ(0u, interceptor_->GetPendingSize());
    interceptor_.reset();
  }

  chromeos::FakeChromeUserManager* GetFakeUserManager() const {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  std::unique_ptr<policy::TestRequestInterceptor> interceptor_;
  FakeAuthInstance auth_instance_;

 private:
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(KioskArcAuthServiceTest);
};

IN_PROC_BROWSER_TEST_F(KioskArcAuthServiceTest, RequestAccountInfoSuccess) {
  interceptor_->PushJobCallback(base::Bind(&ResponseJob));

  auth_instance_.callback =
      base::Bind([](const mojom::AccountInfoPtr& account_info) {
        EXPECT_EQ(kFakeAuthCode, account_info->auth_code.value());
        EXPECT_EQ(mojom::ChromeAccountType::ROBOT_ACCOUNT,
                  account_info->account_type);
        EXPECT_FALSE(account_info->is_managed);
      });

  ArcAuthService::Get()->RequestAccountInfo();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(KioskArcAuthServiceTest, RequestAccountInfoError) {
  interceptor_->PushJobCallback(
      policy::TestRequestInterceptor::BadRequestJob());

  auth_instance_.callback =
      base::Bind([](const mojom::AccountInfoPtr&) { FAIL(); });

  ArcAuthService::Get()->RequestAccountInfo();
  // This MessageLoop will be stopped by AttemptUserExit(), that is called as
  // a result of error of auth code fetching.
  base::RunLoop().Run();
}

}  // namespace arc
