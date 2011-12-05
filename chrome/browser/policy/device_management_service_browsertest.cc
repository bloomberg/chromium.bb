// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/policy/device_management_backend_mock.h"
#include "chrome/browser/policy/device_management_service.h"
#include "chrome/browser/policy/proto/device_management_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/url_fetcher.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::_;

namespace policy {

// Dummy service URL for testing with request interception enabled.
const char kServiceUrl[] = "http://example.com/device_management";

// Binary representation of successful register response containing a token.
const char kServiceResponseRegister[] =
    "\x08\x00\x1a\x22\x0a\x20\x64\x64\x32\x63\x38\x63\x33\x65\x64\x61"
    "\x63\x63\x34\x61\x33\x32\x38\x31\x66\x33\x38\x62\x36\x35\x31\x31"
    "\x36\x64\x61\x62\x66\x63";
// Contains a single policy setting, namely HomepageIsNewTabPage: false.
const char kServiceResponsePolicy[] =
    "\x08\x00\x2a\x2a\x0a\x28\x0a\x06\x70\x6f\x6c\x69\x63\x79\x12\x1e"
    "\x0a\x1c\x0a\x14\x48\x6f\x6d\x65\x70\x61\x67\x65\x49\x73\x4e\x65"
    "\x77\x54\x61\x62\x50\x61\x67\x65\x12\x04\x08\x01\x10\x00";
// Successful unregister response.
const char kServiceResponseUnregister[] =
    "\x08\x00\x22\x00";
// Auto-enrollment response with no modulus and no hashes.
const char kServiceResponseAutoEnrollment[] = "\x42\x00";


#define PROTO_STRING(name) (std::string(name, arraysize(name) - 1))

// Interceptor implementation that returns test data back to the service.
class CannedResponseInterceptor : public net::URLRequest::Interceptor {
 public:
  CannedResponseInterceptor(const GURL& service_url,
                            const std::string& response_data)
      : service_url_(service_url),
        response_data_(response_data) {
    net::URLRequest::Deprecated::RegisterRequestInterceptor(this);
  }

  virtual ~CannedResponseInterceptor() {
    net::URLRequest::Deprecated::UnregisterRequestInterceptor(this);
  }

 private:
  // net::URLRequest::Interceptor overrides.
  virtual net::URLRequestJob* MaybeIntercept(net::URLRequest* request) {
    if (request->url().GetOrigin() == service_url_.GetOrigin() &&
        request->url().path() == service_url_.path()) {
      return new net::URLRequestTestJob(request,
                                        net::URLRequestTestJob::test_headers(),
                                        response_data_,
                                        true);
    }

    return NULL;
  }

  const GURL service_url_;
  const std::string response_data_;
};

class DeviceManagementServiceIntegrationTest : public InProcessBrowserTest {
 public:
  void CaptureToken(const em::DeviceRegisterResponse& response) {
    token_ = response.device_management_token();
  }

 protected:
  std::string token_;
};

static void QuitMessageLoop() {
  MessageLoop::current()->Quit();
}

IN_PROC_BROWSER_TEST_F(DeviceManagementServiceIntegrationTest,
                       CannedResponses) {
  content::URLFetcher::SetEnableInterceptionForTests(true);
  DeviceManagementService service(kServiceUrl);
  service.ScheduleInitialization(0);
  scoped_ptr<DeviceManagementBackend> backend(service.CreateBackend());

  {
    CannedResponseInterceptor interceptor(
        GURL(kServiceUrl), PROTO_STRING(kServiceResponseRegister));
    DeviceRegisterResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandleRegisterResponse(_))
        .WillOnce(DoAll(Invoke(this, &DeviceManagementServiceIntegrationTest
                                          ::CaptureToken),
                        InvokeWithoutArgs(QuitMessageLoop)));
    em::DeviceRegisterRequest request;
    backend->ProcessRegisterRequest("gaia_auth_token", "oauth_token",
                                    "testid", request, &delegate);
    MessageLoop::current()->Run();
  }

  {
    CannedResponseInterceptor interceptor(
        GURL(kServiceUrl), PROTO_STRING(kServiceResponsePolicy));
    DevicePolicyResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandlePolicyResponse(_))
        .WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    em::DevicePolicyRequest request;
    request.set_policy_scope(kChromePolicyScope);
    em::DevicePolicySettingRequest* setting_request =
        request.add_setting_request();
    setting_request->set_key(kChromeDevicePolicySettingKey);
    backend->ProcessPolicyRequest(token_, "testid",
                                  CloudPolicyDataStore::USER_AFFILIATION_NONE,
                                  request, &delegate);

    MessageLoop::current()->Run();
  }

  {
    CannedResponseInterceptor interceptor(
        GURL(kServiceUrl), PROTO_STRING(kServiceResponseUnregister));
    DeviceUnregisterResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandleUnregisterResponse(_))
        .WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    em::DeviceUnregisterRequest request;
    backend->ProcessUnregisterRequest(token_, "testid", request, &delegate);

    MessageLoop::current()->Run();
  }

  {
    CannedResponseInterceptor interceptor(
        GURL(kServiceUrl), PROTO_STRING(kServiceResponseAutoEnrollment));
    DeviceAutoEnrollmentResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandleAutoEnrollmentResponse(_))
        .WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    em::DeviceAutoEnrollmentRequest request;
    request.set_remainder(0);
    request.set_modulus(1);
    backend->ProcessAutoEnrollmentRequest("testid", request, &delegate);

    MessageLoop::current()->Run();
  }
}

IN_PROC_BROWSER_TEST_F(DeviceManagementServiceIntegrationTest,
                       WithTestServer) {
  net::TestServer test_server_(
      net::TestServer::TYPE_HTTP,
      FilePath(FILE_PATH_LITERAL("chrome/test/data/policy")));
  ASSERT_TRUE(test_server_.Start());
  DeviceManagementService service(
      test_server_.GetURL("device_management").spec());
  service.ScheduleInitialization(0);
  scoped_ptr<DeviceManagementBackend> backend(service.CreateBackend());

  {
    DeviceRegisterResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandleRegisterResponse(_))
        .WillOnce(DoAll(Invoke(this, &DeviceManagementServiceIntegrationTest
                                          ::CaptureToken),
                        InvokeWithoutArgs(QuitMessageLoop)));
    em::DeviceRegisterRequest request;
    backend->ProcessRegisterRequest("gaia_auth_token", "oauth_token",
                                    "testid", request, &delegate);
    MessageLoop::current()->Run();
  }

  {
    em::DevicePolicyResponse expected_response;

    DevicePolicyResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandlePolicyResponse(_))
        .WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    em::DevicePolicyRequest request;
    em::PolicyFetchRequest* fetch_request = request.add_request();
    fetch_request->set_signature_type(em::PolicyFetchRequest::SHA1_RSA);
    fetch_request->set_policy_type(kChromeUserPolicyType);
    backend->ProcessPolicyRequest(token_, "testid",
                                  CloudPolicyDataStore::USER_AFFILIATION_NONE,
                                  request, &delegate);

    MessageLoop::current()->Run();
  }

  {
    DeviceUnregisterResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandleUnregisterResponse(_))
        .WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    em::DeviceUnregisterRequest request;
    backend->ProcessUnregisterRequest(token_, "testid", request, &delegate);

    MessageLoop::current()->Run();
  }

  {
    DeviceAutoEnrollmentResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandleAutoEnrollmentResponse(_))
        .WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    em::DeviceAutoEnrollmentRequest request;
    request.set_modulus(1);
    request.set_remainder(0);
    backend->ProcessAutoEnrollmentRequest("testid", request, &delegate);

    MessageLoop::current()->Run();
  }
}

}  // namespace policy
