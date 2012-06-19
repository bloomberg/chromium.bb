// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "net/base/upload_data.h"
#include "net/test/test_server.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::_;

namespace em = enterprise_management;

namespace policy {

// Dummy service URL for testing with request interception enabled.
const char kServiceUrl[] = "http://example.com/device_management";

// Interceptor implementation that returns test data back to the service.
class CannedResponseInterceptor : public net::URLRequest::Interceptor {
 public:
  explicit CannedResponseInterceptor(const GURL& service_url)
      : service_url_(service_url) {
    net::URLRequest::Deprecated::RegisterRequestInterceptor(this);
  }

  virtual ~CannedResponseInterceptor() {
    net::URLRequest::Deprecated::UnregisterRequestInterceptor(this);
  }

  // net::URLRequest::Interceptor overrides.
  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request) OVERRIDE {
    em::DeviceManagementRequest dm_request;
    net::UploadData* upload = request->get_upload();
    if (request->url().GetOrigin() == service_url_.GetOrigin() &&
        request->url().path() == service_url_.path() &&
        upload != NULL &&
        upload->elements()->size() == 1) {
      std::string response_data;
      ConstructResponse(upload->elements()->at(0).bytes(), &response_data);
      return new net::URLRequestTestJob(request,
                                        net::URLRequestTestJob::test_headers(),
                                        response_data,
                                        true);
    }

    return NULL;
  }

 private:
  void ConstructResponse(const std::vector<char>& request_data,
                         std::string* response_data) {
    em::DeviceManagementRequest request;
    ASSERT_TRUE(request.ParseFromArray(vector_as_array(&request_data),
                                       request_data.size()));
    em::DeviceManagementResponse response;
    if (request.has_register_request()) {
      response.mutable_register_response()->set_device_management_token(
          "fake_token");
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

  const GURL service_url_;
};

class DeviceManagementServiceIntegrationTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<
          std::string (DeviceManagementServiceIntegrationTest::*)(void)> {
 public:
  MOCK_METHOD2(OnJobDone, void(DeviceManagementStatus,
                               const em::DeviceManagementResponse&));

  std::string InitCannedResponse() {
    net::URLFetcher::SetEnableInterceptionForTests(true);
    interceptor_.reset(new CannedResponseInterceptor(GURL(kServiceUrl)));
    return kServiceUrl;
  }

  std::string InitTestServer() {
    StartTestServer();
    return test_server_->GetURL("device_management").spec();
  }

 protected:
  void PerformRegistration() {
    EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _))
        .WillOnce(
            DoAll(Invoke(this,
                         &DeviceManagementServiceIntegrationTest::RecordToken),
                  InvokeWithoutArgs(MessageLoop::current(),
                                    &MessageLoop::Quit)));
    scoped_ptr<DeviceManagementRequestJob> job(
        service_->CreateJob(DeviceManagementRequestJob::TYPE_REGISTRATION));
    job->SetGaiaToken("gaia_auth_token");
    job->SetOAuthToken("oauth_token");
    job->SetClientID("testid");
    job->GetRequest()->mutable_register_request();
    job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                          base::Unretained(this)));
    MessageLoop::current()->Run();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    std::string service_url((this->*(GetParam()))());
    service_.reset(new DeviceManagementService(service_url));
    service_->ScheduleInitialization(0);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    service_.reset();
    test_server_.reset();
    interceptor_.reset();
  }

  void StartTestServer() {
    test_server_.reset(
        new net::TestServer(
            net::TestServer::TYPE_HTTP,
            net::TestServer::kLocalhost,
            FilePath(FILE_PATH_LITERAL("chrome/test/data/policy"))));
    ASSERT_TRUE(test_server_->Start());
  }

  void RecordToken(DeviceManagementStatus status,
                   const em::DeviceManagementResponse& response) {
    token_ = response.register_response().device_management_token();
  }

  std::string token_;
  scoped_ptr<DeviceManagementService> service_;
  scoped_ptr<net::TestServer> test_server_;
  scoped_ptr<CannedResponseInterceptor> interceptor_;
};

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, Registration) {
  PerformRegistration();
  EXPECT_FALSE(token_.empty());
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, PolicyFetch) {
  PerformRegistration();

  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _))
      .WillOnce(InvokeWithoutArgs(MessageLoop::current(), &MessageLoop::Quit));
  scoped_ptr<DeviceManagementRequestJob> job(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_POLICY_FETCH));
  job->SetDMToken(token_);
  job->SetUserAffiliation(USER_AFFILIATION_NONE);
  job->SetClientID("testid");
  em::DevicePolicyRequest* request =
      job->GetRequest()->mutable_policy_request();
  request->add_request()->set_policy_type(dm_protocol::kChromeUserPolicyType);
  job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                        base::Unretained(this)));
  MessageLoop::current()->Run();
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, Unregistration) {
  PerformRegistration();

  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _))
      .WillOnce(InvokeWithoutArgs(MessageLoop::current(), &MessageLoop::Quit));
  scoped_ptr<DeviceManagementRequestJob> job(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_UNREGISTRATION));
  job->SetDMToken(token_);
  job->SetClientID("testid");
  job->GetRequest()->mutable_unregister_request();
  job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                        base::Unretained(this)));
  MessageLoop::current()->Run();
}

IN_PROC_BROWSER_TEST_P(DeviceManagementServiceIntegrationTest, AutoEnrollment) {
  EXPECT_CALL(*this, OnJobDone(DM_STATUS_SUCCESS, _))
      .WillOnce(InvokeWithoutArgs(MessageLoop::current(), &MessageLoop::Quit));
  scoped_ptr<DeviceManagementRequestJob> job(
      service_->CreateJob(DeviceManagementRequestJob::TYPE_AUTO_ENROLLMENT));
  job->SetClientID("testid");
  job->GetRequest()->mutable_auto_enrollment_request()->set_remainder(0);
  job->GetRequest()->mutable_auto_enrollment_request()->set_modulus(1);
  job->Start(base::Bind(&DeviceManagementServiceIntegrationTest::OnJobDone,
                        base::Unretained(this)));
  MessageLoop::current()->Run();
}

INSTANTIATE_TEST_CASE_P(
    DeviceManagementServiceIntegrationTestInstance,
    DeviceManagementServiceIntegrationTest,
    testing::Values(&DeviceManagementServiceIntegrationTest::InitCannedResponse,
                    &DeviceManagementServiceIntegrationTest::InitTestServer));

}  // namespace policy
