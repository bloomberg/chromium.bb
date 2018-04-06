// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::_;

namespace em = enterprise_management;

namespace {
const char kEnrollmentToken[] = "enrollment_token";
const char kMachineName[] = "foo";
const char kClientID[] = "fake-client-id";
const char kDMToken[] = "fake-dm-token";
}  // namespace

namespace policy {

MATCHER_P(MatchProto, expected, "matches protobuf") {
  return arg.SerializePartialAsString() == expected.SerializePartialAsString();
}

class MachineLevelUserCloudPolicyServiceIntegrationTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::string (
          MachineLevelUserCloudPolicyServiceIntegrationTest::*)(void)> {
 public:
  MOCK_METHOD3(OnJobDone,
               void(DeviceManagementStatus,
                    int,
                    const em::DeviceManagementResponse&));

  std::string InitTestServer() {
    StartTestServer();
    return test_server_->GetServiceURL().spec();
  }

 protected:
  void PerformRegistration(const std::string& enrollment_token,
                           const std::string& machine_name,
                           bool expect_success) {
    base::RunLoop run_loop;
    if (expect_success) {
      EXPECT_CALL(*this, OnJobDone(testing::Eq(DM_STATUS_SUCCESS), _, _))
          .WillOnce(DoAll(
              Invoke(this, &MachineLevelUserCloudPolicyServiceIntegrationTest::
                               RecordToken),
              InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle)));
    } else {
      EXPECT_CALL(*this, OnJobDone(testing::Ne(DM_STATUS_SUCCESS), _, _))
          .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle));
    }
    std::unique_ptr<DeviceManagementRequestJob> job(
        service_->CreateJob(DeviceManagementRequestJob::TYPE_TOKEN_ENROLLMENT,
                            g_browser_process->system_request_context()));
    job->GetRequest()->mutable_register_browser_request();
    if (!machine_name.empty()) {
      job->GetRequest()->mutable_register_browser_request()->set_machine_name(
          machine_name);
    }
    if (!enrollment_token.empty())
      job->SetEnrollmentToken(enrollment_token);
    job->SetClientID(kClientID);
    job->Start(base::Bind(
        &MachineLevelUserCloudPolicyServiceIntegrationTest::OnJobDone,
        base::Unretained(this)));
    run_loop.Run();
  }

  void UploadChromeDesktopReport(
      const em::ChromeDesktopReportRequest* chrome_desktop_report) {
    base::RunLoop run_loop;
    em::DeviceManagementResponse chrome_desktop_report_response;
    chrome_desktop_report_response.mutable_chrome_desktop_report_response();
    EXPECT_CALL(*this, OnJobDone(testing::Eq(DM_STATUS_SUCCESS), _,
                                 MatchProto(chrome_desktop_report_response)))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::QuitWhenIdle));

    std::unique_ptr<DeviceManagementRequestJob> job(service_->CreateJob(
        DeviceManagementRequestJob::TYPE_CHROME_DESKTOP_REPORT,
        g_browser_process->system_request_context()));

    em::DeviceManagementRequest* request = job->GetRequest();
    if (chrome_desktop_report) {
      *request->mutable_chrome_desktop_report_request() =
          *chrome_desktop_report;
    }

    job->SetDMToken(kDMToken);
    job->SetClientID(kClientID);
    job->Start(base::Bind(
        &MachineLevelUserCloudPolicyServiceIntegrationTest::OnJobDone,
        base::Unretained(this)));
    run_loop.Run();
  }

  void SetUpOnMainThread() override {
    std::string service_url((this->*(GetParam()))());
    service_.reset(new DeviceManagementService(
        std::unique_ptr<DeviceManagementService::Configuration>(
            new MockDeviceManagementServiceConfiguration(service_url))));
    service_->ScheduleInitialization(0);
  }

  void TearDownOnMainThread() override {
    service_.reset();
    test_server_.reset();
  }

  void StartTestServer() {
    test_server_.reset(new LocalPolicyTestServer(
        "machine_level_user_cloud_policy_service_browsertest"));
    ASSERT_TRUE(test_server_->Start());
  }

  void RecordToken(DeviceManagementStatus status,
                   int net_error,
                   const em::DeviceManagementResponse& response) {
    token_ = response.register_response().device_management_token();
  }

  std::string token_;
  std::unique_ptr<DeviceManagementService> service_;
  std::unique_ptr<LocalPolicyTestServer> test_server_;
};

IN_PROC_BROWSER_TEST_P(MachineLevelUserCloudPolicyServiceIntegrationTest,
                       Registration) {
  ASSERT_TRUE(token_.empty());
  PerformRegistration(kEnrollmentToken, kMachineName, /*expect_success=*/true);
  EXPECT_FALSE(token_.empty());
}

IN_PROC_BROWSER_TEST_P(MachineLevelUserCloudPolicyServiceIntegrationTest,
                       RegistrationNoEnrollmentToken) {
  ASSERT_TRUE(token_.empty());
  PerformRegistration(std::string(), kMachineName, /*expect_success=*/false);
  EXPECT_TRUE(token_.empty());
}

IN_PROC_BROWSER_TEST_P(MachineLevelUserCloudPolicyServiceIntegrationTest,
                       RegistrationNoMachineName) {
  ASSERT_TRUE(token_.empty());
  PerformRegistration(kEnrollmentToken, std::string(),
                      /*expect_success=*/false);
  EXPECT_TRUE(token_.empty());
}

IN_PROC_BROWSER_TEST_P(MachineLevelUserCloudPolicyServiceIntegrationTest,
                       ChromeDesktopReport) {
  em::ChromeDesktopReportRequest chrome_desktop_report;
  UploadChromeDesktopReport(&chrome_desktop_report);
}

INSTANTIATE_TEST_CASE_P(
    MachineLevelUserCloudPolicyServiceIntegrationTestInstance,
    MachineLevelUserCloudPolicyServiceIntegrationTest,
    testing::Values(
        &MachineLevelUserCloudPolicyServiceIntegrationTest::InitTestServer));

class CloudPolicyStoreObserverStub : public CloudPolicyStore::Observer {
 public:
  CloudPolicyStoreObserverStub() {}

  bool was_called() const { return on_loaded_ || on_error_; }

 private:
  // CloudPolicyStore::Observer
  void OnStoreLoaded(CloudPolicyStore* store) override { on_loaded_ = true; }
  void OnStoreError(CloudPolicyStore* store) override { on_error_ = true; }

  bool on_loaded_ = false;
  bool on_error_ = false;

  DISALLOW_COPY_AND_ASSIGN(CloudPolicyStoreObserverStub);
};

class MachineLevelUserCloudPolicyManagerTest : public InProcessBrowserTest {
 protected:
  bool CreateAndInitManager(const std::string& dm_token) {
    base::ScopedAllowBlockingForTesting scope_for_testing;
    std::string client_id("client_id");
    base::FilePath user_data_dir;
    CombinedSchemaRegistry schema_registry;
    CloudPolicyStoreObserverStub observer;

    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    std::unique_ptr<MachineLevelUserCloudPolicyStore> policy_store =
        MachineLevelUserCloudPolicyStore::Create(
            dm_token, client_id, user_data_dir,
            base::CreateSequencedTaskRunnerWithTraits(
                {base::MayBlock(), base::TaskPriority::BACKGROUND}));
    policy_store->AddObserver(&observer);

    base::FilePath policy_dir =
        user_data_dir.Append(ChromeBrowserPolicyConnector::kPolicyDir);

    std::unique_ptr<MachineLevelUserCloudPolicyManager> manager =
        std::make_unique<MachineLevelUserCloudPolicyManager>(
            std::move(policy_store), nullptr, policy_dir,
            base::ThreadTaskRunnerHandle::Get(),
            content::BrowserThread::GetTaskRunnerForThread(
                content::BrowserThread::IO));
    manager->Init(&schema_registry);

    manager->store()->RemoveObserver(&observer);
    manager->Shutdown();
    return observer.was_called();
  }
};

IN_PROC_BROWSER_TEST_F(MachineLevelUserCloudPolicyManagerTest, NoDmToken) {
  EXPECT_FALSE(CreateAndInitManager(std::string()));
}

IN_PROC_BROWSER_TEST_F(MachineLevelUserCloudPolicyManagerTest, WithDmToken) {
  EXPECT_TRUE(CreateAndInitManager("dummy_dm_token"));
}

}  // namespace policy
