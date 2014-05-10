// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "net/base/auth.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

const char kInvalidTestRealm[] = "invalid-test-realm";
const char kTestChallenger[] = "https://test-challenger.com:443";
const char kTestRealm[] = "test-realm";
const char kTestToken[] = "test-token";
const char kTestUser[] = "fw-cookie";

}  // namespace

// Test class that overrides underlying calls to see if an auth challenge is
// acceptible for the data reduction proxy, and to get a valid token if so.
class TestDataReductionProxyAuthRequestHandler
    : public DataReductionProxyAuthRequestHandler {
 public:
  TestDataReductionProxyAuthRequestHandler(int time_step_ms,
                                           int64 initial_time_ms)
      : DataReductionProxyAuthRequestHandler(NULL),
        time_step_ms_(time_step_ms),
        now_(base::TimeTicks() +
             base::TimeDelta::FromMilliseconds(initial_time_ms)) {}
 protected:
  // Test implementation.
  virtual bool IsAcceptableAuthChallenge(
      net::AuthChallengeInfo* auth_info) OVERRIDE {
    if (net::HostPortPair::FromString(
        kTestChallenger).Equals(auth_info->challenger) &&
        auth_info->realm == kTestRealm) {
    return true;
    }
    return false;
  }

  // Test implementation.
  virtual base::string16 GetTokenForAuthChallenge(
      net::AuthChallengeInfo* auth_info) OVERRIDE {
    return base::ASCIIToUTF16(kTestToken);
  }

  virtual base::TimeTicks Now() OVERRIDE {
    now_ += base::TimeDelta::FromMilliseconds(time_step_ms_);
    return now_;
  }
  int time_step_ms_;
  base::TimeTicks now_;
};

class DataReductionProxyAuthRequestHandlerTest : public testing::Test {
 public:
  // Checks that |PROCEED| was returned with expected user and password.
  void ExpectProceed(
      DataReductionProxyAuthRequestHandler::TryHandleResult result,
      const base::string16& user,
      const base::string16& password) {
    base::string16 expected_user = base::ASCIIToUTF16(kTestUser);
    base::string16 expected_password = base::ASCIIToUTF16(kTestToken);
    EXPECT_EQ(DataReductionProxyAuthRequestHandler::TRY_HANDLE_RESULT_PROCEED,
              result);
    EXPECT_EQ(expected_user, user);
    EXPECT_EQ(expected_password, password);
  }

  // Checks that |CANCEL| was returned.
  void ExpectCancel(
      DataReductionProxyAuthRequestHandler::TryHandleResult result,
      const base::string16& user,
      const base::string16& password) {
    EXPECT_EQ(DataReductionProxyAuthRequestHandler::TRY_HANDLE_RESULT_CANCEL,
              result);
    EXPECT_EQ(base::string16(), user);
    EXPECT_EQ(base::string16(), password);
  }

  // Checks that |IGNORE| was returned.
  void ExpectIgnore(
      DataReductionProxyAuthRequestHandler::TryHandleResult result,
      const base::string16& user,
      const base::string16& password) {
    EXPECT_EQ(DataReductionProxyAuthRequestHandler::TRY_HANDLE_RESULT_IGNORE,
              result);
    EXPECT_EQ(base::string16(), user);
    EXPECT_EQ(base::string16(), password);
  }
};

TEST_F(DataReductionProxyAuthRequestHandlerTest,
       CancelAfterSuccessiveAuthAttempts) {
  DataReductionProxyAuthRequestHandler::auth_request_timestamp_ = 0;
  DataReductionProxyAuthRequestHandler::back_to_back_failure_count_ = 0;
  DataReductionProxyAuthRequestHandler::auth_token_invalidation_timestamp_ = 0;
  scoped_refptr<net::AuthChallengeInfo> auth_info(new net::AuthChallengeInfo);
  auth_info->realm =  kTestRealm;
  auth_info->challenger = net::HostPortPair::FromString(kTestChallenger);
  TestDataReductionProxyAuthRequestHandler handler(499, 3600001);
  base::string16 user, password;
  DataReductionProxyAuthRequestHandler::TryHandleResult result =
      handler.TryHandleAuthentication(auth_info.get(), &user, &password);
  ExpectProceed(result, user, password);

  // Successive retries should also succeed up to a maximum count.
  for (int i = 0; i < 5; ++i) {
    user = base::string16();
    password = base::string16();
    result = handler.TryHandleAuthentication(auth_info.get(), &user, &password);
    ExpectProceed(result, user, password);
  }

  // Then another retry should fail.
  user = base::string16();
  password = base::string16();
  result = handler.TryHandleAuthentication(auth_info.get(), &user, &password);
  ExpectCancel(result, user, password);

  // After canceling, the next one should proceed.
  user = base::string16();
  password = base::string16();
  result = handler.TryHandleAuthentication(auth_info.get(), &user, &password);
  ExpectProceed(result, user, password);
}

TEST_F(DataReductionProxyAuthRequestHandlerTest, Ignore) {
  scoped_refptr<net::AuthChallengeInfo> auth_info(new net::AuthChallengeInfo);
  auth_info->realm =  kInvalidTestRealm;
  auth_info->challenger = net::HostPortPair::FromString(kTestChallenger);
  TestDataReductionProxyAuthRequestHandler handler(100, 3600001);
  base::string16 user, password;
  DataReductionProxyAuthRequestHandler::TryHandleResult result =
      handler.TryHandleAuthentication(auth_info.get(), &user, &password);
  ExpectIgnore(result, user, password);
}

}  // namespace data_reduction_proxy
