// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/cloud/test_request_interceptor.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
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

namespace policy {

namespace {

// Parses the DeviceManagementRequest in |request_data| and writes a serialized
// DeviceManagementResponse to |response_data|.
void ConstructResponse(const char* request_data,
                       uint64 request_data_length,
                       std::string* response_data) {
  em::DeviceManagementRequest request;
  ASSERT_TRUE(request.ParseFromArray(request_data, request_data_length));
  em::DeviceManagementResponse response;
  if (request.has_register_request()) {
    response.mutable_register_response()->set_device_management_token(
        "fake_token");
  } else if (request.has_service_api_access_request()) {
    response.mutable_service_api_access_response()->set_auth_code(
        "fake_auth_code");
  } else if (request.has_unregister_request()) {
    response.mutable_unregister_response();
  } else if (request.has_policy_request()) {
    response.mutable_policy_response()->add_response();
  } else if (request.has_auto_enrollment_request()) {
    response.mutable_auto_enrollment_response();
  } else {
    FAIL() << "Failed to parse request.";
  }
  ASSERT_TRUE(response.SerializeToString(response_data));
}

// JobCallback for the interceptor.
net::URLRequestJob* ResponseJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) {
  const net::UploadDataStream* upload = request->get_upload();
  if (upload != NULL &&
      upload->element_readers().size() == 1 &&
      upload->element_readers()[0]->AsBytesReader()) {
    std::string response_data;
    const net::UploadBytesElementReader* bytes_reader =
        upload->element_readers()[0]->AsBytesReader();
    ConstructResponse(bytes_reader->bytes(),
                      bytes_reader->length(),
                      &response_data);
    return new net::URLRequestTestJob(
        request,
        network_delegate,
        net::URLRequestTestJob::test_headers(),
        response_data,
        true);
  }

  return NULL;
}

}  // namespace

class DeviceManagementServiceIntegrationTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<
          std::string (DeviceManagementServiceIntegrationTest::*)(void)> {
 public:
  MOCK_METHOD3(OnJobDone, void(DeviceManagementStatus, int,
                               const em::DeviceManagementResponse&));

  std::string InitCannedResponse() {
    interceptor_.reset(new TestRequestInterceptor(
        "localhost",
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
    return "http://localhost";
  }

  std::string InitTestServer() {
    StartTestServer();
    return test_server_->GetServiceURL().spec();
  }

  void RecordAuthCode(DeviceManagementStatus status,
                      int net_error,
                      const em::DeviceManagementResponse& response) {
    robot_auth_code_ = response.service_api_access_response().auth_code();
  }

 protected:
  void ExpectRequest() {
    if (interceptor_)
      interceptor_->PushJobCallback(base::Bind(&ResponseJob));
  }

  void PerformRegistration() {
    ExpectRequest();
    EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _, _))
        .WillOnce(
            DoAll(Invoke(this,
                         &DeviceManagementServiceIntegrationTest::RecordToken),
                  InvokeWithoutArgs(base::MessageLoop::current(),
                                    &base::MessageLoop::Quit)));
    scoped_ptr<DeviceManagementRequestJob> job(
        service_->CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION,
                            g_browser_process->system_request_context()));
    job->SetGaiaToken("gaia_auth_token");
    job->SetOAuthToken("oauth_token");
    job->SetClientID("testid");
    job->GetRequest()->mutable_register_request();
    job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                          base::Unretained(this)));
    base::MessageLoop::current()->Run();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    std::string service_url((this->*(GetParam()))());
    service_.reset(new DeviceManagementService(
        scoped_ptr<DeviceManagementService::Configuration>(
            new MockDeviceManagementServiceConfiguration(service_url))));
    service_->ScheduleInitialization(0);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    service_.reset();
    test_server_.reset();
    interceptor_.reset();
  }

  void StartTestServer() {
    test_server_.reset(
        new LocalPolicyTestServer("device_management_service_browsertest"));
    ASSERT_TRUE(test_server_->Start());
  }

  void RecordToken(DeviceManagementStatus status,
                   int net_error,
                   const em::DeviceManagementResponse& response) {
    token_ = response.register_response().device_management_token();
  }

  std::string token_;
  std::string robot_auth_code_;
  scoped_ptr<DeviceManagementService> service_;
  scoped_ptr<LocalPolicyTestServer> test_server_;
  scoped_ptr<TestRequestInterceptor> interceptor_;
};

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, Registration) {
  PerformRegistration();
  EXPECT_FALSE(token_.empty());
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest,
                       ApiAuthCodeFetch) {
  PerformRegistration();

  ExpectRequest();
  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _, _))
      .WillOnce(
          DoAll(Invoke(this,
                       &DeviceManagementServiceIntegrationTest::RecordAuthCode),
                InvokeWithoutArgs(base::MessageLoop::current(),
                                  &base::MessageLoop::Quit)));
  scoped_ptr<DeviceManagementRequestJob> job(service_->CreateJob(
      DeviceManagementRequestJob::TYPE_API_AUTH_CODE_FETCH,
      g_browser_process->system_request_context()));
  job->SetDMToken(token_);
  job->SetClientID("testid");
  em::DeviceServiceApiAccessRequest* request =
      job->GetRequest()->mutable_service_api_access_request();
  request->add_auth_scope("authScope4Test");
  request->set_oauth2_client_id("oauth2ClientId4Test");
  job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                        base::Unretained(this)));
  base::MessageLoop::current()->Run();
  ASSERT_EQ("fake_auth_code", robot_auth_code_);
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, PolicyFetch) {
  PerformRegistration();

  ExpectRequest();
  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _, _))
      .WillOnce(InvokeWithoutArgs(base::MessageLoop::current(),
                                  &base::MessageLoop::Quit));
  scoped_ptr<DeviceManagementRequestJob> job(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH,
                          g_browser_process->system_request_context()));
  job->SetDMToken(token_);
  job->SetUserAffiliation(USER_AFFILIATION_NONE);
  job->SetClientID("testid");
  em::DevicePolicyRequest* request =
      job->GetRequest()->mutable_policy_request();
  request->add_request()->set_policy_type(dm_protocol::kChromeUserPolicyType);
  job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                        base::Unretained(this)));
  base::MessageLoop::current()->Run();
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, Unregistration) {
  PerformRegistration();

  ExpectRequest();
  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _, _))
      .WillOnce(InvokeWithoutArgs(base::MessageLoop::current(),
                                  &base::MessageLoop::Quit));
  scoped_ptr<DeviceManagementRequestJob> job(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION,
                          g_browser_process->system_request_context()));
  job->SetDMToken(token_);
  job->SetClientID("testid");
  job->GetRequest()->mutable_unregister_request();
  job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                        base::Unretained(this)));
  base::MessageLoop::current()->Run();
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, AutoEnrollment) {
  ExpectRequest();
  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _, _))
      .WillOnce(InvokeWithoutArgs(base::MessageLoop::current(),
                                  &base::MessageLoop::Quit));
  scoped_ptr<DeviceManagementRequestJob> job(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT,
                          g_browser_process->system_request_context()));
  job->SetClientID("testid");
  job->GetRequest()->mutable_auto_enrollment_request()->set_remainder(0);
  job->GetRequest()->mutable_auto_enrollment_request()->set_modulus(1);
  job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                        base::Unretained(this)));
  base::MessageLoop::current()->Run();
}

INSTANTIATE_TEST_CASE_P(
    DeviceManagementServiceIntegrationTestInstance,
    DeviceManagementServiceIntegrationTest,
    testing::Values(&DeviceManagementServiceIntegrationTest::InitCannedResponse,
                    &DeviceManagementServiceIntegrationTest::InitTestServer));

}  // namespace policy
