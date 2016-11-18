// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/policy/cloud/test_request_interceptor.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/auth.mojom.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_switches.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

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

class ArcRobotAuthBrowserTest : public InProcessBrowserTest {
 protected:
  ArcRobotAuthBrowserTest() = default;

  // InProcessBrowserTest:
  ~ArcRobotAuthBrowserTest() override = default;

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
    ArcSessionManager::Get()->Shutdown();
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

  DISALLOW_COPY_AND_ASSIGN(ArcRobotAuthBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ArcRobotAuthBrowserTest, RequestAccountInfoSuccess) {
  interceptor_->PushJobCallback(base::Bind(&ResponseJob));

  auth_instance_.callback =
      base::Bind([](const mojom::AccountInfoPtr& account_info) {
        EXPECT_EQ(kFakeAuthCode, account_info->auth_code.value());
        EXPECT_EQ(mojom::ChromeAccountType::ROBOT_ACCOUNT,
                  account_info->account_type);
        EXPECT_FALSE(account_info->is_managed);
      });

  ArcAuthService::GetForTest()->RequestAccountInfo();
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(ArcRobotAuthBrowserTest, RequestAccountInfoError) {
  interceptor_->PushJobCallback(
      policy::TestRequestInterceptor::BadRequestJob());

  auth_instance_.callback =
      base::Bind([](const mojom::AccountInfoPtr&) { FAIL(); });

  ArcAuthService::GetForTest()->RequestAccountInfo();
  // This MessageLoop will be stopped by AttemptUserExit(), that is called as
  // a result of error of auth code fetching.
  base::RunLoop().Run();
}

}  // namespace arc
