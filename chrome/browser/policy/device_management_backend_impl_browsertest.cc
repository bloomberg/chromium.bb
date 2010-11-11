// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_management_backend_impl.h"

#include "base/message_loop.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/policy/device_management_backend_mock.h"
#include "chrome/test/in_process_browser_test.h"
#include "net/test/test_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace policy {

namespace {

const char kServiceUrl[] = "http://example.com/service";

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

#define PROTO_STRING(name) (std::string(name, arraysize(name) - 1))

}  // namespace

// Interceptor implementation that returns test data back to the service.
class CannedResponseInterceptor : public URLRequest::Interceptor {
 public:
  CannedResponseInterceptor(const GURL& service_url,
                            const std::string& response_data)
      : service_url_(service_url),
        response_data_(response_data) {
    URLRequest::RegisterRequestInterceptor(this);
  }

  virtual ~CannedResponseInterceptor() {
    URLRequest::UnregisterRequestInterceptor(this);
  }

 private:
  // URLRequest::Interceptor overrides.
  virtual URLRequestJob* MaybeIntercept(URLRequest* request) {
    if (request->url().GetOrigin() == service_url_.GetOrigin() &&
        request->url().path() == service_url_.path()) {
      return new URLRequestTestJob(request,
                                   URLRequestTestJob::test_headers(),
                                   response_data_,
                                   true);
    }

    return NULL;
  }

  const GURL service_url_;
  const std::string response_data_;
};

class DeviceManagementBackendImplIntegrationTest : public InProcessBrowserTest {
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

IN_PROC_BROWSER_TEST_F(DeviceManagementBackendImplIntegrationTest,
                       CannedResponses) {
  URLFetcher::enable_interception_for_tests(true);
  DeviceManagementBackendImpl service(kServiceUrl);

  {
    CannedResponseInterceptor interceptor(
        GURL(kServiceUrl), PROTO_STRING(kServiceResponseRegister));
    DeviceRegisterResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandleRegisterResponse(_))
        .WillOnce(DoAll(Invoke(this, &DeviceManagementBackendImplIntegrationTest
                                          ::CaptureToken),
                        InvokeWithoutArgs(QuitMessageLoop)));
    em::DeviceRegisterRequest request;
    service.ProcessRegisterRequest("token", "device id", request, &delegate);
    MessageLoop::current()->Run();
  }

  {
    CannedResponseInterceptor interceptor(
        GURL(kServiceUrl), PROTO_STRING(kServiceResponsePolicy));
    DevicePolicyResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandlePolicyResponse(_))
        .WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    em::DevicePolicyRequest request;
    request.set_policy_scope("chrome");
    em::DevicePolicySettingRequest* setting_request =
        request.add_setting_request();
    setting_request->set_key("policy");
    service.ProcessPolicyRequest(token_, request, &delegate);

    MessageLoop::current()->Run();
  }

  {
    CannedResponseInterceptor interceptor(
        GURL(kServiceUrl), PROTO_STRING(kServiceResponseUnregister));
    DeviceUnregisterResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandleUnregisterResponse(_))
        .WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    em::DeviceUnregisterRequest request;
    service.ProcessUnregisterRequest(token_, request, &delegate);

    MessageLoop::current()->Run();
  }
}

IN_PROC_BROWSER_TEST_F(DeviceManagementBackendImplIntegrationTest,
                       WithTestServer) {
  net::TestServer test_server(
      net::TestServer::TYPE_HTTP,
      FilePath(FILE_PATH_LITERAL("chrome/test/data/policy")));
  ASSERT_TRUE(test_server.Start());
  DeviceManagementBackendImpl service(
      test_server.GetURL("device_management").spec());

  {
    DeviceRegisterResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandleRegisterResponse(_))
        .WillOnce(DoAll(Invoke(this, &DeviceManagementBackendImplIntegrationTest
                                          ::CaptureToken),
                        InvokeWithoutArgs(QuitMessageLoop)));
    em::DeviceRegisterRequest request;
    service.ProcessRegisterRequest("token", "device id", request, &delegate);
    MessageLoop::current()->Run();
  }

  {
    em::DevicePolicyResponse expected_response;

    DevicePolicyResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandlePolicyResponse(_))
        .WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    em::DevicePolicyRequest request;
    request.set_policy_scope("chrome");
    em::DevicePolicySettingRequest* setting_request =
        request.add_setting_request();
    setting_request->set_key("policy");
    service.ProcessPolicyRequest(token_, request, &delegate);

    MessageLoop::current()->Run();
  }

  {
    DeviceUnregisterResponseDelegateMock delegate;
    EXPECT_CALL(delegate, HandleUnregisterResponse(_))
        .WillOnce(InvokeWithoutArgs(QuitMessageLoop));
    em::DeviceUnregisterRequest request;
    service.ProcessUnregisterRequest(token_, request, &delegate);

    MessageLoop::current()->Run();
  }
}

}  // namespace policy
