// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"

#include "base/message_loop/message_loop.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/cryptohome/mock_cryptohome_library.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_token_service_test_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;

namespace chromeos {

static const int kOAuthTokenServiceUrlFetcherId = 0;
static const int kValidatorUrlFetcherId = gaia::GaiaOAuthClient::kUrlFetcherId;

class TestDeviceOAuth2TokenService : public DeviceOAuth2TokenService {
 public:
  explicit TestDeviceOAuth2TokenService(net::URLRequestContextGetter* getter,
                                        PrefService* local_state)
      : DeviceOAuth2TokenService(getter, local_state) {
  }
  void SetRobotAccountIdPolicyValue(const std::string& id) {
    robot_account_id_ = id;
  }

 protected:
  // Skip calling into the policy subsystem and return our test value.
  virtual std::string GetRobotAccountId() OVERRIDE {
    return robot_account_id_;
  }

 private:
  std::string robot_account_id_;
  DISALLOW_COPY_AND_ASSIGN(TestDeviceOAuth2TokenService);
};

class DeviceOAuth2TokenServiceTest : public testing::Test {
 public:
  DeviceOAuth2TokenServiceTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        scoped_testing_local_state_(TestingBrowserProcess::GetGlobal()),
        request_context_getter_(new net::TestURLRequestContextGetter(
            message_loop_.message_loop_proxy())),
        oauth2_service_(request_context_getter_.get(),
                        scoped_testing_local_state_.Get()) {
    oauth2_service_.max_refresh_token_validation_retries_ = 0;
    oauth2_service_.set_max_authorization_token_fetch_retries_for_testing(0);
  }
  virtual ~DeviceOAuth2TokenServiceTest() {}

  // Most tests just want a noop crypto impl with a dummy refresh token value in
  // Local State (if the value is an empty string, it will be ignored).
  void SetUpDefaultValues() {
    cryptohome_library_.reset(chromeos::CryptohomeLibrary::GetTestImpl());
    chromeos::CryptohomeLibrary::SetForTest(cryptohome_library_.get());
    SetDeviceRefreshTokenInLocalState("device_refresh_token_4_test");
    oauth2_service_.SetRobotAccountIdPolicyValue("service_acct@g.com");
    AssertConsumerTokensAndErrors(0, 0);
  }

  scoped_ptr<OAuth2TokenService::Request> StartTokenRequest() {
    return oauth2_service_.StartRequest(std::set<std::string>(), &consumer_);
  }

  virtual void TearDown() OVERRIDE {
    CryptohomeLibrary::SetForTest(NULL);
    base::RunLoop().RunUntilIdle();
  }

  // Utility method to set a value in Local State for the device refresh token
  // (it must have a non-empty value or it won't be used).
  void SetDeviceRefreshTokenInLocalState(const std::string& refresh_token) {
    scoped_testing_local_state_.Get()->SetManagedPref(
        prefs::kDeviceRobotAnyApiRefreshToken,
        Value::CreateStringValue(refresh_token));
  }

  std::string GetValidTokenInfoResponse(const std::string email) {
    return "{ \"email\": \"" + email + "\","
           "  \"user_id\": \"1234567890\" }";
  }

  // A utility method to return fake URL results, for testing the refresh token
  // validation logic.  For a successful validation attempt, this method will be
  // called three times for the steps listed below (steps 1 and 2 happen in
  // parallel).
  //
  // Step 1a: fetch the access token for the tokeninfo API.
  // Step 1b: call the tokeninfo API.
  // Step 2:  Fetch the access token for the requested scope
  //          (in this case, cloudprint).
  void ReturnOAuthUrlFetchResults(int fetcher_id,
                                  net::HttpStatusCode response_code,
                                  const std::string&  response_string);

  void AssertConsumerTokensAndErrors(int num_tokens, int num_errors);

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  ScopedTestingLocalState scoped_testing_local_state_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  net::TestURLFetcherFactory factory_;
  TestDeviceOAuth2TokenService oauth2_service_;
  TestingOAuth2TokenServiceConsumer consumer_;
  scoped_ptr<chromeos::CryptohomeLibrary> cryptohome_library_;

};

void DeviceOAuth2TokenServiceTest::ReturnOAuthUrlFetchResults(
    int fetcher_id,
    net::HttpStatusCode response_code,
    const std::string&  response_string) {

  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(fetcher_id);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(response_code);
  fetcher->SetResponseString(response_string);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

void DeviceOAuth2TokenServiceTest::AssertConsumerTokensAndErrors(
    int num_tokens,
    int num_errors) {
  ASSERT_EQ(num_tokens, consumer_.number_of_successful_tokens_);
  ASSERT_EQ(num_errors, consumer_.number_of_errors_);
}

TEST_F(DeviceOAuth2TokenServiceTest, SaveEncryptedToken) {
  StrictMock<MockCryptohomeLibrary> mock_cryptohome_library;
  CryptohomeLibrary::SetForTest(&mock_cryptohome_library);

  EXPECT_CALL(mock_cryptohome_library, DecryptWithSystemSalt(StrEq("")))
      .Times(1)
      .WillOnce(Return(""));
  EXPECT_CALL(mock_cryptohome_library,
              EncryptWithSystemSalt(StrEq("test-token")))
      .Times(1)
      .WillOnce(Return("encrypted"));
  EXPECT_CALL(mock_cryptohome_library,
              DecryptWithSystemSalt(StrEq("encrypted")))
      .Times(1)
      .WillOnce(Return("test-token"));

  ASSERT_EQ("", oauth2_service_.GetRefreshToken());
  oauth2_service_.SetAndSaveRefreshToken("test-token");
  ASSERT_EQ("test-token", oauth2_service_.GetRefreshToken());

  // This call won't invoke decrypt again, since the value is cached.
  ASSERT_EQ("test-token", oauth2_service_.GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Success) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("tokeninfo_access_token", 3600));

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenInfoResponse("service_acct@g.com"));

  ReturnOAuthUrlFetchResults(
      kOAuthTokenServiceUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("scoped_access_token", 3600));

  AssertConsumerTokensAndErrors(1, 0);

  EXPECT_EQ("scoped_access_token", consumer_.last_token_);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoAccessTokenHttpError) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_UNAUTHORIZED,
      "");

  // TokenInfo API call skipped (error returned in previous step).

  // CloudPrint access token fetch is successful, but consumer still given error
  // due to bad refresh token.
  ReturnOAuthUrlFetchResults(
      kOAuthTokenServiceUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("ignored_scoped_access_token", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoAccessTokenInvalidResponse) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      "invalid response");

  // TokenInfo API call skipped (error returned in previous step).

  ReturnOAuthUrlFetchResults(
      kOAuthTokenServiceUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("ignored_scoped_access_token", 3600));

  // CloudPrint access token fetch is successful, but consumer still given error
  // due to bad refresh token.
  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoApiCallHttpError) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("tokeninfo_access_token", 3600));

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_INTERNAL_SERVER_ERROR,
      "");

  ReturnOAuthUrlFetchResults(
      kOAuthTokenServiceUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("ignored_scoped_access_token", 3600));

  // CloudPrint access token fetch is successful, but consumer still given error
  // due to bad refresh token.
  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoApiCallInvalidResponse) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("tokeninfo_access_token", 3600));

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      "invalid response");

  ReturnOAuthUrlFetchResults(
      kOAuthTokenServiceUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("ignored_scoped_access_token", 3600));

  // CloudPrint access token fetch is successful, but consumer still given error
  // due to bad refresh token.
  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_CloudPrintAccessTokenHttpError) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("tokeninfo_access_token", 3600));

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenInfoResponse("service_acct@g.com"));

  ReturnOAuthUrlFetchResults(
      kOAuthTokenServiceUrlFetcherId,
      net::HTTP_BAD_REQUEST,
      "");

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_CloudPrintAccessTokenInvalidResponse) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("tokeninfo_access_token", 3600));

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenInfoResponse("service_acct@g.com"));

  ReturnOAuthUrlFetchResults(
      kOAuthTokenServiceUrlFetcherId,
      net::HTTP_OK,
      "invalid request");

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Failure_BadOwner) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  oauth2_service_.SetRobotAccountIdPolicyValue("WRONG_service_acct@g.com");

  // The requested token comes in before any of the validation calls complete,
  // but the consumer still gets an error, since the results don't get returned
  // until validation is over.
  ReturnOAuthUrlFetchResults(
      kOAuthTokenServiceUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("ignored_scoped_access_token", 3600));
  AssertConsumerTokensAndErrors(0, 0);

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenResponse("tokeninfo_access_token", 3600));
  AssertConsumerTokensAndErrors(0, 0);

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      net::HTTP_OK,
      GetValidTokenInfoResponse("service_acct@g.com"));

  // All fetches were successful, but consumer still given error since
  // the token owner doesn't match the policy value.
  AssertConsumerTokensAndErrors(0, 1);
}

}  // namespace chromeos
