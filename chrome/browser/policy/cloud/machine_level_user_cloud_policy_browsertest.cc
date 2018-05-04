// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/policy/browser_dm_token_storage.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_metrics.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/core/common/policy_switches.h"
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

namespace policy {
namespace {

const char kEnrollmentToken[] = "enrollment_token";
const char kInvalidEnrollmentToken[] = "invalid_enrollment_token";
const char kMachineName[] = "foo";
const char kClientID[] = "fake-client-id";
const char kDMToken[] = "fake-dm-token";
const char kEnrollmentResultMetrics[] =
    "Enterprise.MachineLevelUserCloudPolicyEnrollment.Result";

class ChromeBrowserPolicyConnectorObserver
    : public ChromeBrowserPolicyConnector::Observer {
 public:
  void OnMachineLevelUserCloudPolicyRegisterFinished(bool succeeded) override {
    EXPECT_EQ(should_succeed_, succeeded);
    is_finished_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

  void SetRunLoop(base::RunLoop* run_loop) { run_loop_ = run_loop; }
  void SetShouldSucceed(bool should_succeed) {
    should_succeed_ = should_succeed;
  }

  bool IsFinished() { return is_finished_; }

 private:
  base::RunLoop* run_loop_ = nullptr;
  bool is_finished_ = false;
  bool should_succeed_ = false;
};

class FakeBrowserDMTokenStorage : public BrowserDMTokenStorage {
 public:
  FakeBrowserDMTokenStorage() = default;

  std::string RetrieveClientId() override { return client_id_; }
  std::string RetrieveEnrollmentToken() override { return enrollment_token_; }
  void StoreDMToken(const std::string& dm_token,
                    StoreCallback callback) override {
    // Store the dm token in memory even if storage gonna failed. This is the
    // same behavior of production code.
    dm_token_ = dm_token;
    // Run the callback synchronously to make sure the metrics is recorded
    // before verfication.
    std::move(callback).Run(storage_enabled_);
  }
  std::string RetrieveDMToken() override { return dm_token_; }

  void SetEnrollmentToken(const std::string& enrollment_token) {
    enrollment_token_ = enrollment_token;
  }

  void SetClientId(std::string client_id) { client_id_ = client_id; }

  void EnableStorage(bool storage_enabled) {
    storage_enabled_ = storage_enabled;
  }

 private:
  std::string enrollment_token_;
  std::string client_id_;
  std::string dm_token_;
  bool storage_enabled_ = true;

  DISALLOW_COPY_AND_ASSIGN(FakeBrowserDMTokenStorage);
};

class ChromeBrowserExtraSetUp : public ChromeBrowserMainExtraParts {
 public:
  explicit ChromeBrowserExtraSetUp(
      ChromeBrowserPolicyConnectorObserver* observer)
      : observer_(observer) {}
  void PreMainMessageLoopStart() override {
    g_browser_process->browser_policy_connector()->AddObserver(observer_);
  }

 private:
  ChromeBrowserPolicyConnectorObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserExtraSetUp);
};

}  // namespace

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

    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

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

class MachineLevelUserCloudPolicyEnrollmentTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  MachineLevelUserCloudPolicyEnrollmentTest() {
    BrowserDMTokenStorage::SetForTesting(&storage_);
    storage_.SetEnrollmentToken(is_enrollment_token_valid()
                                    ? kEnrollmentToken
                                    : kInvalidEnrollmentToken);
    storage_.SetClientId("client_id");
    storage_.EnableStorage(storage_enabled());
    observer_.SetShouldSucceed(is_enrollment_token_valid());
  }

  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(test_server_.Start());

    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(switches::kDeviceManagementUrl,
                                    test_server_.GetServiceURL().spec());

    histogram_tester_.ExpectTotalCount(kEnrollmentResultMetrics, 0);
  }

  void CreatedBrowserMainParts(content::BrowserMainParts* parts) override {
    static_cast<ChromeBrowserMainParts*>(parts)->AddParts(
        new ChromeBrowserExtraSetUp(&observer_));
  }

  void TearDownOnMainThread() override {
    g_browser_process->browser_policy_connector()->RemoveObserver(&observer_);
  }

  void WaitForPolicyRegisterFinished() {
    if (!observer_.IsFinished()) {
      base::RunLoop run_loop;
      observer_.SetRunLoop(&run_loop);
      run_loop.Run();
    }
  }

 protected:
  bool is_enrollment_token_valid() const { return std::get<0>(GetParam()); }
  bool storage_enabled() const { return std::get<1>(GetParam()); }

  base::HistogramTester histogram_tester_;

 private:
  LocalPolicyTestServer test_server_;
  FakeBrowserDMTokenStorage storage_;
  ChromeBrowserPolicyConnectorObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(MachineLevelUserCloudPolicyEnrollmentTest);
};

IN_PROC_BROWSER_TEST_P(MachineLevelUserCloudPolicyEnrollmentTest, Test) {
  WaitForPolicyRegisterFinished();

  EXPECT_EQ(is_enrollment_token_valid() ? "fake_device_management_token"
                                        : std::string(),
            BrowserDMTokenStorage::Get()->RetrieveDMToken());

  MachineLevelUserCloudPolicyEnrollmentResult expected_result;
  if (is_enrollment_token_valid() && storage_enabled()) {
    expected_result = MachineLevelUserCloudPolicyEnrollmentResult::kSuccess;
  } else if (is_enrollment_token_valid() && !storage_enabled()) {
    expected_result =
        MachineLevelUserCloudPolicyEnrollmentResult::kFailedToStore;
  } else {
    expected_result =
        MachineLevelUserCloudPolicyEnrollmentResult::kFailedToFetch;
  }
  histogram_tester_.ExpectBucketCount(kEnrollmentResultMetrics, expected_result,
                                      1);
  histogram_tester_.ExpectTotalCount(kEnrollmentResultMetrics, 1);
}

INSTANTIATE_TEST_CASE_P(,
                        MachineLevelUserCloudPolicyEnrollmentTest,
                        ::testing::Combine(::testing::Bool(),
                                           ::testing::Bool()));

}  // namespace policy
