// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/directory_add_request.h"
#include "chrome/test/testing_profile.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
class MockDoneCallback {
 public:
  MOCK_METHOD2(OnDone,
               void(DirectoryAddRequest::Result,
                    const std::string& message));
};
}  // namespace

class DirectoryAddRequestTest : public testing::Test {
 protected:
  virtual void SetUp() {
    target_.reset(new DirectoryAddRequest(profile_.GetRequestContext()));
  }

  void TestResult(int response_code, const std::string& data,
                  DirectoryAddRequest::Result expected_result,
                  const std::string& expected_message) {
    MockDoneCallback callback;
    EXPECT_CALL(callback, OnDone(expected_result, expected_message))
        .Times(1);

    target_->done_callback_.reset(
        NewCallback(&callback, &MockDoneCallback::OnDone));

    GURL url;
    net::URLRequestStatus status_ok;
    ResponseCookies cookies;
    target_->OnURLFetchComplete(NULL, url, status_ok, response_code,
                               cookies, data);
  }

  TestingProfile profile_;
  scoped_ptr<DirectoryAddRequest> target_;
};

TEST_F(DirectoryAddRequestTest, Success) {
  TestResult(200, "{\"data\":{\"kind\":\"chromoting#host\","
             "\"hostId\":\"e64906c9-fdc9-4921-80cb-563cf7f564f3\","
             "\"hostName\":\"host_name\",\"publicKey\":\"PUBLIC+KEY\"}}",
             DirectoryAddRequest::SUCCESS, "");
}

TEST_F(DirectoryAddRequestTest, Duplicate) {
  TestResult(400, "{\"error\":{\"errors\":[{\"domain\":\"global\","
             "\"reason\":\"invalid\",\"message\":\"Attempt to register "
             "a duplicate host.\"}],\"code\":400,\"message\":\"Attempt to "
             "register a duplicate host.\"}}",
             DirectoryAddRequest::ERROR_EXISTS,
             "Attempt to register a duplicate host.");
}

TEST_F(DirectoryAddRequestTest, InvalidRequest) {
  TestResult(400, "{\"error\":{\"errors\":[{\"domain\":\"global\","
             "\"reason\":\"invalid\",\"message\":\"Invalid Value\"}],"
             "\"code\":400,\"message\":\"Invalid Value\"}}",
             DirectoryAddRequest::ERROR_INVALID_REQUEST,
             "Invalid Value");
}

TEST_F(DirectoryAddRequestTest, InvalidToken) {
  TestResult(401, "{\"error\":{\"errors\":[{\"domain\":\"global\","
             "\"reason\":\"invalid\",\"message\":\"Token invalid\","
             "\"locationType\":\"header\",\"location\":\"Authorization\"}],"
             "\"code\":401,\"message\":\"Token invalid\"}}",
             DirectoryAddRequest::ERROR_AUTH,
             "Token invalid");
}

}  // namespace remoting
