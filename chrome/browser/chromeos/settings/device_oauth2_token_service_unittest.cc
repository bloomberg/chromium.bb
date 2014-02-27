// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"

#include "base/message_loop/message_loop.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/settings/token_encryptor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/fake_cryptohome_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/oauth2_token_service_test_util.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

static const int kOAuthTokenServiceUrlFetcherId = 0;
static const int kValidatorUrlFetcherId = gaia::GaiaOAuthClient::kUrlFetcherId;

class TestDeviceOAuth2TokenService : public DeviceOAuth2TokenService {
 public:
  explicit TestDeviceOAuth2TokenService(net::URLRequestContextGetter* getter,
                                        PrefService* local_state)
      : DeviceOAuth2TokenService(getter, local_state) {}

  void SetRobotAccountIdPolicyValue(const std::string& id) {
    robot_account_id_ = id;
  }

  // Skip calling into the policy subsystem and return our test value.
  virtual std::string GetRobotAccountId() const OVERRIDE {
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
            message_loop_.message_loop_proxy())) {}
  virtual ~DeviceOAuth2TokenServiceTest() {}

  // Most tests just want a noop crypto impl with a dummy refresh token value in
  // Local State (if the value is an empty string, it will be ignored).
  void SetUpDefaultValues() {
    SetDeviceRefreshTokenInLocalState("device_refresh_token_4_test");
    CreateService();
    oauth2_service_->SetRobotAccountIdPolicyValue("service_acct@g.com");
    AssertConsumerTokensAndErrors(0, 0);

    base::RunLoop().RunUntilIdle();
  }

  void SetUpWithPendingSalt() {
    fake_cryptohome_client_->set_system_salt(std::vector<uint8>());
    fake_cryptohome_client_->SetServiceIsAvailable(false);
    SetDeviceRefreshTokenInLocalState("device_refresh_token_4_test");
    CreateService();
    oauth2_service_->SetRobotAccountIdPolicyValue("service_acct@g.com");
  }

  scoped_ptr<OAuth2TokenService::Request> StartTokenRequest() {
    return oauth2_service_->StartRequest(oauth2_service_->GetRobotAccountId(),
                                         std::set<std::string>(),
                                         &consumer_);
  }

  virtual void SetUp() OVERRIDE {
    scoped_ptr<FakeDBusThreadManager> fake_dbus_thread_manager(
        new FakeDBusThreadManager);
    fake_cryptohome_client_ = new FakeCryptohomeClient;
    fake_cryptohome_client_->SetServiceIsAvailable(true);
    fake_cryptohome_client_->set_system_salt(
        FakeCryptohomeClient::GetStubSystemSalt());
    fake_dbus_thread_manager->SetCryptohomeClient(
        scoped_ptr<CryptohomeClient>(fake_cryptohome_client_));
    DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager.release());
    SystemSaltGetter::Initialize();
  }

  virtual void TearDown() OVERRIDE {
    SystemSaltGetter::Shutdown();
    DBusThreadManager::Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  void CreateService() {
    oauth2_service_.reset(new TestDeviceOAuth2TokenService(
        request_context_getter_.get(), scoped_testing_local_state_.Get()));
    oauth2_service_->max_refresh_token_validation_retries_ = 0;
    oauth2_service_->set_max_authorization_token_fetch_retries_for_testing(0);
  }

  // Utility method to set a value in Local State for the device refresh token
  // (it must have a non-empty value or it won't be used).
  void SetDeviceRefreshTokenInLocalState(const std::string& refresh_token) {
    scoped_testing_local_state_.Get()->SetUserPref(
        prefs::kDeviceRobotAnyApiRefreshToken,
        base::Value::CreateStringValue(refresh_token));
  }

  std::string GetValidTokenInfoResponse(const std::string email) {
    return "{ \"email\": \"" + email + "\","
           "  \"user_id\": \"1234567890\" }";
  }

  bool RefreshTokenIsAvailable() {
    return oauth2_service_->RefreshTokenIsAvailable(
        oauth2_service_->GetRobotAccountId());
  }

  std::string GetRefreshToken() {
    if (!RefreshTokenIsAvailable())
      return std::string();

    return oauth2_service_->GetRefreshToken(
        oauth2_service_->GetRobotAccountId());
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

  // Generates URL fetch replies with the specified results for requests
  // generated by the token service.
  void PerformURLFetchesWithResults(
      net::HttpStatusCode tokeninfo_access_token_status,
      const std::string& tokeninfo_access_token_response,
      net::HttpStatusCode tokeninfo_fetch_status,
      const std::string& tokeninfo_fetch_response,
      net::HttpStatusCode service_access_token_status,
      const std::string& service_access_token_response);

  // Generates URL fetch replies for the success path.
  void PerformURLFetches();

  void AssertConsumerTokensAndErrors(int num_tokens, int num_errors);

 protected:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  ScopedTestingLocalState scoped_testing_local_state_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  net::TestURLFetcherFactory factory_;
  FakeCryptohomeClient* fake_cryptohome_client_;
  scoped_ptr<TestDeviceOAuth2TokenService> oauth2_service_;
  TestingOAuth2TokenServiceConsumer consumer_;
};

void DeviceOAuth2TokenServiceTest::ReturnOAuthUrlFetchResults(
    int fetcher_id,
    net::HttpStatusCode response_code,
    const std::string& response_string) {
  net::TestURLFetcher* fetcher = factory_.GetFetcherByID(fetcher_id);
  if (fetcher) {
    factory_.RemoveFetcherFromMap(fetcher_id);
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_string);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    base::RunLoop().RunUntilIdle();
  }
}

void DeviceOAuth2TokenServiceTest::PerformURLFetchesWithResults(
    net::HttpStatusCode tokeninfo_access_token_status,
    const std::string& tokeninfo_access_token_response,
    net::HttpStatusCode tokeninfo_fetch_status,
    const std::string& tokeninfo_fetch_response,
    net::HttpStatusCode service_access_token_status,
    const std::string& service_access_token_response) {
  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      tokeninfo_access_token_status,
      tokeninfo_access_token_response);

  ReturnOAuthUrlFetchResults(
      kValidatorUrlFetcherId,
      tokeninfo_fetch_status,
      tokeninfo_fetch_response);

  ReturnOAuthUrlFetchResults(
      kOAuthTokenServiceUrlFetcherId,
      service_access_token_status,
      service_access_token_response);
}

void DeviceOAuth2TokenServiceTest::PerformURLFetches() {
  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, GetValidTokenResponse("scoped_access_token", 3600));
}

void DeviceOAuth2TokenServiceTest::AssertConsumerTokensAndErrors(
    int num_tokens,
    int num_errors) {
  EXPECT_EQ(num_tokens, consumer_.number_of_successful_tokens_);
  EXPECT_EQ(num_errors, consumer_.number_of_errors_);
}

TEST_F(DeviceOAuth2TokenServiceTest, SaveEncryptedToken) {
  CreateService();

  oauth2_service_->SetAndSaveRefreshToken(
      "test-token", DeviceOAuth2TokenService::StatusCallback());
  EXPECT_EQ("test-token", GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenServiceTest, SaveEncryptedTokenEarly) {
  // Set a new refresh token without the system salt available.
  SetUpWithPendingSalt();

  oauth2_service_->SetAndSaveRefreshToken(
      "test-token", DeviceOAuth2TokenService::StatusCallback());
  EXPECT_EQ("test-token", GetRefreshToken());

  // Make the system salt available.
  fake_cryptohome_client_->set_system_salt(
      FakeCryptohomeClient::GetStubSystemSalt());
  fake_cryptohome_client_->SetServiceIsAvailable(true);
  base::RunLoop().RunUntilIdle();

  // The original token should still be present.
  EXPECT_EQ("test-token", GetRefreshToken());

  // Reloading shouldn't change the token either.
  CreateService();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("test-token", GetRefreshToken());
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Success) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  PerformURLFetches();
  AssertConsumerTokensAndErrors(1, 0);

  EXPECT_EQ("scoped_access_token", consumer_.last_token_);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_SuccessAsyncLoad) {
  SetUpWithPendingSalt();

  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();
  PerformURLFetches();
  AssertConsumerTokensAndErrors(0, 0);

  fake_cryptohome_client_->set_system_salt(
      FakeCryptohomeClient::GetStubSystemSalt());
  fake_cryptohome_client_->SetServiceIsAvailable(true);
  base::RunLoop().RunUntilIdle();

  PerformURLFetches();
  AssertConsumerTokensAndErrors(1, 0);

  EXPECT_EQ("scoped_access_token", consumer_.last_token_);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Cancel) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();
  request.reset();

  PerformURLFetches();

  // Test succeeds if this line is reached without a crash.
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_NoSalt) {
  fake_cryptohome_client_->set_system_salt(std::vector<uint8>());
  fake_cryptohome_client_->SetServiceIsAvailable(true);
  SetUpDefaultValues();

  EXPECT_FALSE(RefreshTokenIsAvailable());

  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();
  base::RunLoop().RunUntilIdle();

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoAccessTokenHttpError) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_UNAUTHORIZED, "",
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoAccessTokenInvalidResponse) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, "invalid response",
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoApiCallHttpError) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_INTERNAL_SERVER_ERROR, "",
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_TokenInfoApiCallInvalidResponse) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, "invalid response",
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_CloudPrintAccessTokenHttpError) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_BAD_REQUEST, "");

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest,
       RefreshTokenValidation_Failure_CloudPrintAccessTokenInvalidResponse) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, "invalid request");

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Failure_BadOwner) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  oauth2_service_->SetRobotAccountIdPolicyValue("WRONG_service_acct@g.com");

  PerformURLFetchesWithResults(
      net::HTTP_OK, GetValidTokenResponse("tokeninfo_access_token", 3600),
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);
}

TEST_F(DeviceOAuth2TokenServiceTest, RefreshTokenValidation_Retry) {
  SetUpDefaultValues();
  scoped_ptr<OAuth2TokenService::Request> request = StartTokenRequest();

  PerformURLFetchesWithResults(
      net::HTTP_INTERNAL_SERVER_ERROR, "",
      net::HTTP_OK, GetValidTokenInfoResponse("service_acct@g.com"),
      net::HTTP_OK, GetValidTokenResponse("ignored", 3600));

  AssertConsumerTokensAndErrors(0, 1);

  // Retry should succeed.
  request = StartTokenRequest();
  PerformURLFetches();
  AssertConsumerTokensAndErrors(1, 1);
}

}  // namespace chromeos
