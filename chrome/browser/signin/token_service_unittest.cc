// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a unit test for the profile's token service.

#include "chrome/browser/signin/token_service_unittest.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/webdata/token_web_data.h"
#include "chrome/common/chrome_switches.h"
#include "components/webdata/encryptor/encryptor.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/mock_url_fetcher_factory.h"
#include "net/url_request/test_url_fetcher_factory.h"

TokenAvailableTracker::TokenAvailableTracker() {}

TokenAvailableTracker::~TokenAvailableTracker() {}

void TokenAvailableTracker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  TestNotificationTracker::Observe(type, source, details);
  if (type == chrome::NOTIFICATION_TOKEN_AVAILABLE) {
    content::Details<const TokenService::TokenAvailableDetails> full = details;
    details_ = *full.ptr();
  }
}

TokenFailedTracker::TokenFailedTracker() {}

TokenFailedTracker::~TokenFailedTracker() {}

void TokenFailedTracker::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  TestNotificationTracker::Observe(type, source, details);
  if (type == chrome::NOTIFICATION_TOKEN_REQUEST_FAILED) {
    content::Details<const TokenService::TokenRequestFailedDetails> full =
        details;
    details_ = *full.ptr();
  }
}

TokenServiceTestHarness::TokenServiceTestHarness()
    : signin_manager_(NULL), service_(NULL) {
}

TokenServiceTestHarness::~TokenServiceTestHarness() {}

void TokenServiceTestHarness::SetUp() {
#if defined(OS_MACOSX)
  Encryptor::UseMockKeychain(true);
#endif
  credentials_.sid = "sid";
  credentials_.lsid = "lsid";
  credentials_.token = "token";
  credentials_.data = "data";
  oauth_token_ = "oauth";
  oauth_secret_ = "secret";

  profile_ = CreateProfile().Pass();

  profile_->CreateWebDataService();

  // Force the loading of the WebDataService.
  TokenWebData::FromBrowserContext(profile_.get());
  base::RunLoop().RunUntilIdle();

  service_ = TokenServiceFactory::GetForProfile(profile_.get());

  success_tracker_.ListenFor(chrome::NOTIFICATION_TOKEN_AVAILABLE,
                             content::Source<TokenService>(service_));
  failure_tracker_.ListenFor(chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                             content::Source<TokenService>(service_));

  service()->Initialize("test", profile_.get());
}

scoped_ptr<TestingProfile> TokenServiceTestHarness::CreateProfile() {
  return make_scoped_ptr(new TestingProfile());
}

void TokenServiceTestHarness::CreateSigninManager(const std::string& username) {
  signin_manager_ =
      static_cast<FakeSigninManagerBase*>(
          SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
              profile(), FakeSigninManagerBase::Build));
  signin_manager_->Initialize(profile(), NULL);
  signin_manager_->SetAuthenticatedUsername(username);
}

void TokenServiceTestHarness::TearDown() {
  // You have to destroy the profile before the threads are shut down.
  profile_.reset();
}

void TokenServiceTestHarness::UpdateCredentialsOnService() {
  service()->UpdateCredentials(credentials_);
}

class TokenServiceTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    TokenServiceTestHarness::SetUp();
    UpdateCredentialsOnService();
  }
  virtual scoped_ptr<TestingProfile> CreateProfile() OVERRIDE;

 protected:
  void TestLoadSingleToken(
      std::map<std::string, std::string>* db_tokens,
      std::map<std::string, std::string>* memory_tokens,
      const std::string& service_name) {
    std::string token = service_name + "_token";
    (*db_tokens)[service_name] = token;
    size_t prev_success_size = success_tracker()->size();
    service()->LoadTokensIntoMemory(*db_tokens, memory_tokens);

    // Check notification.
    EXPECT_EQ(prev_success_size + 1, success_tracker()->size());
    TokenService::TokenAvailableDetails details = success_tracker()->details();
    EXPECT_EQ(details.service(), service_name);
    EXPECT_EQ(details.token(), token);
    // Check memory tokens.
    EXPECT_EQ(1U, memory_tokens->count(service_name));
    EXPECT_EQ((*memory_tokens)[service_name], token);
  }
};

scoped_ptr<TestingProfile> TokenServiceTest::CreateProfile() {
  TestingProfile::Builder builder;
  builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                            FakeProfileOAuth2TokenService::Build);
  return builder.Build();
}

TEST_F(TokenServiceTest, SanityCheck) {
  EXPECT_FALSE(service()->HasTokenForService("nonexistent service"));
  EXPECT_FALSE(service()->TokensLoadedFromDB());
}

TEST_F(TokenServiceTest, NoToken) {
  EXPECT_FALSE(service()->HasTokenForService("nonexistent service"));
  EXPECT_EQ(service()->GetTokenForService("nonexistent service"),
            std::string());
}

TEST_F(TokenServiceTest, NotificationSuccess) {
  EXPECT_EQ(0U, success_tracker()->size());
  EXPECT_EQ(0U, failure_tracker()->size());
  service()->OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_EQ(1U, success_tracker()->size());
  EXPECT_EQ(0U, failure_tracker()->size());

  TokenService::TokenAvailableDetails details = success_tracker()->details();
  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncService);
  EXPECT_EQ(details.token(), "token");
}

TEST_F(TokenServiceTest, NotificationOAuthLoginTokenSuccess) {
  EXPECT_EQ(0U, success_tracker()->size());
  EXPECT_EQ(0U, failure_tracker()->size());
  CreateSigninManager("test@gmail.com");
  service()->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("rt1", "at1", 3600));
  EXPECT_EQ(1U, success_tracker()->size());
  EXPECT_EQ(0U, failure_tracker()->size());

  TokenService::TokenAvailableDetails details = success_tracker()->details();
  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() ==
      GaiaConstants::kGaiaOAuth2LoginRefreshToken);
  EXPECT_EQ(details.token(), "rt1");
}

TEST_F(TokenServiceTest, NotificationFailed) {
  EXPECT_EQ(0U, success_tracker()->size());
  EXPECT_EQ(0U, failure_tracker()->size());
  CreateSigninManager("test@gmail.com");
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  service()->OnIssueAuthTokenFailure(GaiaConstants::kSyncService, error);
  EXPECT_EQ(0U, success_tracker()->size());
  EXPECT_EQ(1U, failure_tracker()->size());

  TokenService::TokenRequestFailedDetails details =
      failure_tracker()->details();
  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncService);
  EXPECT_TRUE(details.error() == error);  // Struct has no print function.
}

TEST_F(TokenServiceTest, NotificationOAuthLoginTokenFailed) {
  EXPECT_EQ(0U, success_tracker()->size());
  EXPECT_EQ(0U, failure_tracker()->size());
  CreateSigninManager("test@gmail.com");
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  service()->OnClientOAuthFailure(error);
  EXPECT_EQ(0U, success_tracker()->size());
  EXPECT_EQ(1U, failure_tracker()->size());

  TokenService::TokenRequestFailedDetails details =
      failure_tracker()->details();

  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() ==
      GaiaConstants::kGaiaOAuth2LoginRefreshToken);
  EXPECT_TRUE(details.error() == error);  // Struct has no print function.
}

TEST_F(TokenServiceTest, OnTokenSuccessUpdate) {
  service()->OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_TRUE(service()->HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service()->GetTokenForService(GaiaConstants::kSyncService),
            "token");

  service()->OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token2");
  EXPECT_TRUE(service()->HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service()->GetTokenForService(GaiaConstants::kSyncService),
            "token2");

  service()->OnIssueAuthTokenSuccess(GaiaConstants::kSyncService,
                                     std::string());
  EXPECT_TRUE(service()->HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service()->GetTokenForService(GaiaConstants::kSyncService), "");
}

TEST_F(TokenServiceTest, OnOAuth2LoginTokenSuccessUpdate) {
  CreateSigninManager("test@gmail.com");
  EXPECT_FALSE(service()->HasOAuthLoginToken());

  service()->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("rt1", "at1", 3600));
  EXPECT_TRUE(service()->HasOAuthLoginToken());
  EXPECT_EQ(service()->GetOAuth2LoginRefreshToken(), "rt1");

  service()->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("rt2", "at2", 3600));
  EXPECT_TRUE(service()->HasOAuthLoginToken());
  EXPECT_EQ(service()->GetOAuth2LoginRefreshToken(), "rt2");

  service()->OnClientOAuthSuccess(
      GaiaAuthConsumer::ClientOAuthResult("rt3", "at3", 3600));
  EXPECT_TRUE(service()->HasOAuthLoginToken());
  EXPECT_EQ(service()->GetOAuth2LoginRefreshToken(), "rt3");
}

TEST_F(TokenServiceTest, OnTokenSuccess) {
  CreateSigninManager("test@gmail.com");
  // Don't "start fetching", just go ahead and issue the callback.
  service()->OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_TRUE(service()->HasTokenForService(GaiaConstants::kSyncService));
  // Gaia returns the entire result as the token so while this is a shared
  // result with ClientLogin, it doesn't matter, we should still get it back.
  EXPECT_EQ(service()->GetTokenForService(GaiaConstants::kSyncService),
            "token");
}

TEST_F(TokenServiceTest, Reset) {
  CreateSigninManager("test@gmail.com");
  net::TestURLFetcherFactory factory;
  service()->StartFetchingTokens();
  // You have to call delegates by hand with the test fetcher,
  // Let's pretend only one returned.

  service()->OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "eraseme");
  EXPECT_TRUE(service()->HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service()->GetTokenForService(GaiaConstants::kSyncService),
            "eraseme");

  service()->ResetCredentialsInMemory();
  EXPECT_FALSE(service()->HasTokenForService(GaiaConstants::kSyncService));

  // Now start using it again.
  UpdateCredentialsOnService();
  service()->StartFetchingTokens();

  service()->OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");

  EXPECT_EQ(service()->GetTokenForService(GaiaConstants::kSyncService),
            "token");
}

TEST_F(TokenServiceTest, FullIntegration) {
  std::string result = "SID=sid\nLSID=lsid\nAuth=auth\n";

  {
    MockURLFetcherFactory<MockFetcher> factory;
    factory.set_results(result);
    EXPECT_FALSE(service()->HasTokenForService(GaiaConstants::kSyncService));
    service()->StartFetchingTokens();
  }

  EXPECT_TRUE(service()->HasTokenForService(GaiaConstants::kSyncService));
  // Gaia returns the entire result as the token so while this is a shared
  // result with ClientLogin, it doesn't matter, we should still get it back.
  EXPECT_EQ(service()->GetTokenForService(GaiaConstants::kSyncService), result);

  service()->ResetCredentialsInMemory();
  EXPECT_FALSE(service()->HasTokenForService(GaiaConstants::kSyncService));
}

TEST_F(TokenServiceTest, LoadTokensIntoMemoryBasic) {
  CreateSigninManager("test@gmail.com");
  // Validate that the method sets proper data in notifications and map.
  std::map<std::string, std::string> db_tokens;
  std::map<std::string, std::string> memory_tokens;

  EXPECT_FALSE(service()->TokensLoadedFromDB());
  service()->LoadTokensIntoMemory(db_tokens, &memory_tokens);
  EXPECT_TRUE(db_tokens.empty());
  EXPECT_TRUE(memory_tokens.empty());
  EXPECT_EQ(0U, success_tracker()->size());

  std::vector<std::string> services;
  TokenService::GetServiceNames(&services);
  for (std::vector<std::string>::const_iterator i = services.begin();
       i != services.end(); ++i) {
    const std::string& service = *i;
    TestLoadSingleToken(&db_tokens, &memory_tokens, service);
  }
  std::string service = GaiaConstants::kGaiaOAuth2LoginRefreshToken;
  TestLoadSingleToken(&db_tokens, &memory_tokens, service);
}

TEST_F(TokenServiceTest, LoadTokensIntoMemoryAdvanced) {
  CreateSigninManager("test@gmail.com");
  // LoadTokensIntoMemory should avoid setting tokens already in the
  // token map.
  std::map<std::string, std::string> db_tokens;
  std::map<std::string, std::string> memory_tokens;

  db_tokens["ignore"] = "token";

  service()->LoadTokensIntoMemory(db_tokens, &memory_tokens);
  EXPECT_TRUE(memory_tokens.empty());
  db_tokens[GaiaConstants::kSyncService] = "pepper";

  service()->LoadTokensIntoMemory(db_tokens, &memory_tokens);
  EXPECT_EQ(1U, memory_tokens.count(GaiaConstants::kSyncService));
  EXPECT_EQ(memory_tokens[GaiaConstants::kSyncService], "pepper");
  EXPECT_EQ(1U, success_tracker()->size());
  success_tracker()->Reset();

  // SyncService token is already in memory. Pretend we got it off
  // the disk as well, but an older token.
  db_tokens[GaiaConstants::kSyncService] = "ignoreme";
  service()->LoadTokensIntoMemory(db_tokens, &memory_tokens);

  EXPECT_EQ(1U, memory_tokens.size());
  EXPECT_EQ(0U, success_tracker()->size());
  EXPECT_EQ(1U, memory_tokens.count(GaiaConstants::kSyncService));
  EXPECT_EQ(memory_tokens[GaiaConstants::kSyncService], "pepper");
}

TEST_F(TokenServiceTest, WebDBLoadIntegration) {
  CreateSigninManager("test@gmail.com");
  service()->LoadTokensFromDB();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(service()->TokensLoadedFromDB());
  EXPECT_EQ(0U, success_tracker()->size());

  // Should result in DB write.
  service()->OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_EQ(1U, success_tracker()->size());

  EXPECT_TRUE(service()->HasTokenForService(GaiaConstants::kSyncService));
  // Clean slate.
  service()->ResetCredentialsInMemory();
  success_tracker()->Reset();
  EXPECT_FALSE(service()->HasTokenForService(GaiaConstants::kSyncService));

  service()->LoadTokensFromDB();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, success_tracker()->size());
  EXPECT_TRUE(service()->HasTokenForService(GaiaConstants::kSyncService));
}

TEST_F(TokenServiceTest, MultipleLoadResetIntegration) {
  CreateSigninManager("test@gmail.com");
  // Should result in DB write.
  service()->OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  service()->ResetCredentialsInMemory();
  success_tracker()->Reset();
  EXPECT_FALSE(service()->HasTokenForService(GaiaConstants::kSyncService));

  EXPECT_FALSE(service()->TokensLoadedFromDB());
  service()->LoadTokensFromDB();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(service()->TokensLoadedFromDB());

  service()->LoadTokensFromDB();  // Should do nothing.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(service()->TokensLoadedFromDB());

  EXPECT_EQ(1U, success_tracker()->size());
  EXPECT_TRUE(service()->HasTokenForService(GaiaConstants::kSyncService));

  // Reset it one more time so there's no surprises.
  service()->ResetCredentialsInMemory();
  EXPECT_FALSE(service()->TokensLoadedFromDB());
  success_tracker()->Reset();

  service()->LoadTokensFromDB();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(service()->TokensLoadedFromDB());

  EXPECT_EQ(1U, success_tracker()->size());
  EXPECT_TRUE(service()->HasTokenForService(GaiaConstants::kSyncService));
}

#ifndef NDEBUG
class TokenServiceCommandLineTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() {
    CommandLine original_cl(*CommandLine::ForCurrentProcess());
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSetToken, "my_service:my_value");
    TokenServiceTestHarness::SetUp();
    UpdateCredentialsOnService();

    *CommandLine::ForCurrentProcess() = original_cl;
  }
};

TEST_F(TokenServiceCommandLineTest, TestValueOverride) {
  EXPECT_TRUE(service()->HasTokenForService("my_service"));
  EXPECT_EQ("my_value", service()->GetTokenForService("my_service"));
}
#endif   // ifndef NDEBUG
