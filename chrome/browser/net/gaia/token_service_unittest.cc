// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a unit test for the profile's token service.

#include "chrome/browser/net/gaia/token_service_unittest.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher_unittest.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/test/test_url_fetcher_factory.h"

using content::BrowserThread;

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
    : ui_thread_(BrowserThread::UI, &message_loop_),
      db_thread_(BrowserThread::DB) {
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

  ASSERT_TRUE(db_thread_.Start());

  profile_.reset(new TestingProfile());
  profile_->CreateWebDataService(false);
  WaitForDBLoadCompletion();

  success_tracker_.ListenFor(chrome::NOTIFICATION_TOKEN_AVAILABLE,
                             content::Source<TokenService>(&service_));
  failure_tracker_.ListenFor(chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
                             content::Source<TokenService>(&service_));

  service_.Initialize("test", profile_.get());
}

void TokenServiceTestHarness::TearDown() {
  // You have to destroy the profile before the db_thread_ stops.
  if (profile_.get()) {
    profile_.reset(NULL);
  }

  db_thread_.Stop();
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  MessageLoop::current()->Run();
}

void TokenServiceTestHarness::WaitForDBLoadCompletion() {
  // The WebDB does all work on the DB thread. This will add an event
  // to the end of the DB thread, so when we reach this task, all DB
  // operations should be complete.
  base::WaitableEvent done(false, false);
  BrowserThread::PostTask(
      BrowserThread::DB,
      FROM_HERE,
      base::Bind(&base::WaitableEvent::Signal, base::Unretained(&done)));
  done.Wait();

  // Notifications should be returned from the DB thread onto the UI thread.
  message_loop_.RunAllPending();
}

class TokenServiceTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() {
    TokenServiceTestHarness::SetUp();
    service_.UpdateCredentials(credentials_);
    service_.UpdateOAuthCredentials(oauth_token_, oauth_secret_);
  }
 protected:
  void TestLoadSingleToken(
      std::map<std::string, std::string>* db_tokens,
      std::map<std::string, std::string>* memory_tokens,
      const std::string& service) {
    std::string token = service + "_token";
    (*db_tokens)[service] = token;
    size_t prev_success_size = success_tracker_.size();
    service_.LoadTokensIntoMemory(*db_tokens, memory_tokens);

    // Check notification.
    EXPECT_EQ(prev_success_size + 1, success_tracker_.size());
    TokenService::TokenAvailableDetails details = success_tracker_.details();
    EXPECT_EQ(details.service(), service);
    EXPECT_EQ(details.token(), token);
    // Check memory tokens.
    EXPECT_EQ(1U, memory_tokens->count(service));
    EXPECT_EQ((*memory_tokens)[service], token);
  }
};

TEST_F(TokenServiceTest, SanityCheck) {
  EXPECT_TRUE(service_.HasLsid());
  EXPECT_EQ(service_.GetLsid(), "lsid");
  EXPECT_FALSE(service_.HasTokenForService("nonexistent service"));
  EXPECT_TRUE(service_.HasOAuthCredentials());
  EXPECT_EQ(service_.GetOAuthToken(), "oauth");
  EXPECT_EQ(service_.GetOAuthSecret(), "secret");
}

TEST_F(TokenServiceTest, NoToken) {
  EXPECT_FALSE(service_.HasTokenForService("nonexistent service"));
  EXPECT_EQ(service_.GetTokenForService("nonexistent service"), std::string());
}

TEST_F(TokenServiceTest, NotificationSuccess) {
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());

  TokenService::TokenAvailableDetails details = success_tracker_.details();
  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncService);
  EXPECT_EQ(details.token(), "token");
}

TEST_F(TokenServiceTest, NotificationOAuthLoginTokenSuccess) {
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());
  service_.OnOAuthLoginTokenSuccess("rt1", "at1", 3600);
  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());

  TokenService::TokenAvailableDetails details = success_tracker_.details();
  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() ==
      GaiaConstants::kGaiaOAuth2LoginRefreshToken);
  EXPECT_EQ(details.token(), "rt1");
}

TEST_F(TokenServiceTest, NotificationSuccessOAuth) {
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());
  service_.OnOAuthWrapBridgeSuccess(
      GaiaConstants::kSyncServiceOAuth, "token", "3600");
  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());

  TokenService::TokenAvailableDetails details = success_tracker_.details();
  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncServiceOAuth);
  EXPECT_EQ(details.token(), "token");
}

TEST_F(TokenServiceTest, NotificationFailed) {
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  service_.OnIssueAuthTokenFailure(GaiaConstants::kSyncService, error);
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(1U, failure_tracker_.size());

  TokenService::TokenRequestFailedDetails details = failure_tracker_.details();

  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncService);
  EXPECT_TRUE(details.error() == error);  // Struct has no print function.
}

TEST_F(TokenServiceTest, NotificationOAuthLoginTokenFailed) {
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  service_.OnOAuthLoginTokenFailure(error);
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(1U, failure_tracker_.size());

  TokenService::TokenRequestFailedDetails details = failure_tracker_.details();

  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() ==
      GaiaConstants::kGaiaOAuth2LoginRefreshToken);
  EXPECT_TRUE(details.error() == error);  // Struct has no print function.
}

TEST_F(TokenServiceTest, NotificationFailedOAuth) {
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  service_.OnOAuthWrapBridgeFailure(GaiaConstants::kSyncServiceOAuth, error);
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(1U, failure_tracker_.size());

  TokenService::TokenRequestFailedDetails details = failure_tracker_.details();

  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncServiceOAuth);
  EXPECT_TRUE(details.error() == error);  // Struct has no print function.
}

TEST_F(TokenServiceTest, OnTokenSuccessUpdate) {
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");

  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token2");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token2");

  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "");
}

TEST_F(TokenServiceTest, OnOAuth2LoginTokenSuccessUpdate) {
  std::string service = GaiaConstants::kGaiaOAuth2LoginRefreshToken;
  service_.OnOAuthLoginTokenSuccess("rt1", "at1", 3600);
  EXPECT_TRUE(service_.HasOAuthLoginToken());
  EXPECT_EQ(service_.GetOAuth2LoginRefreshToken(), "rt1");

  service_.OnOAuthLoginTokenSuccess("rt2", "at2", 3600);
  EXPECT_TRUE(service_.HasOAuthLoginToken());
  EXPECT_EQ(service_.GetOAuth2LoginRefreshToken(), "rt2");

  service_.OnOAuthLoginTokenSuccess("rt3", "at3", 3600);
  EXPECT_TRUE(service_.HasOAuthLoginToken());
  EXPECT_EQ(service_.GetOAuth2LoginRefreshToken(), "rt3");
}

TEST_F(TokenServiceTest, OnTokenSuccess) {
  // Don't "start fetching", just go ahead and issue the callback.
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  // Gaia returns the entire result as the token so while this is a shared
  // result with ClientLogin, it doesn't matter, we should still get it back.
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");

  // Try the OAuth service.
  service_.OnOAuthWrapBridgeSuccess(
      GaiaConstants::kSyncServiceOAuth, "token2", "3600");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncServiceOAuth),
            "token2");

  // First didn't change.
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");
}

TEST_F(TokenServiceTest, ResetSimple) {
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_TRUE(service_.HasLsid());
  service_.OnOAuthGetAccessTokenSuccess("token2", "secret");
  service_.OnOAuthWrapBridgeSuccess(
      GaiaConstants::kSyncServiceOAuth, "token3", "4321");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  EXPECT_TRUE(service_.HasOAuthCredentials());

  service_.ResetCredentialsInMemory();

  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasLsid());
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  EXPECT_FALSE(service_.HasOAuthCredentials());
}

TEST_F(TokenServiceTest, ResetComplex) {
  TestURLFetcherFactory factory;
  service_.StartFetchingTokens();
  // You have to call delegates by hand with the test fetcher,
  // Let's pretend only one returned.

  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "eraseme");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService),
            "eraseme");
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));

  service_.OnOAuthWrapBridgeSuccess(
      GaiaConstants::kSyncServiceOAuth, "erasemetoo", "1234");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService),
            "eraseme");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncServiceOAuth),
            "erasemetoo");
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));

  service_.ResetCredentialsInMemory();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  EXPECT_FALSE(service_.HasLsid());
  EXPECT_FALSE(service_.HasOAuthCredentials());

  // Now start using it again.
  service_.UpdateCredentials(credentials_);
  EXPECT_TRUE(service_.HasLsid());
  service_.StartFetchingTokens();
  service_.UpdateOAuthCredentials(oauth_token_, oauth_secret_);
  EXPECT_TRUE(service_.HasOAuthCredentials());
  service_.StartFetchingOAuthTokens();

  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  service_.OnOAuthWrapBridgeSuccess(
      GaiaConstants::kSyncServiceOAuth, "token2", "3600");
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kTalkService, "token3");

  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncServiceOAuth),
            "token2");
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kTalkService), "token3");
}

TEST_F(TokenServiceTest, FullIntegration) {
  std::string result = "SID=sid\nLSID=lsid\nAuth=auth\n";

  {
    MockFactory<MockFetcher> factory;
    factory.set_results(result);
    EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
    EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
    EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
    service_.StartFetchingTokens();
  }

  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kTalkService));
  // Gaia returns the entire result as the token so while this is a shared
  // result with ClientLogin, it doesn't matter, we should still get it back.
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), result);
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kTalkService), result);

  service_.ResetCredentialsInMemory();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
}

TEST_F(TokenServiceTest, LoadTokensIntoMemoryBasic) {
  // Validate that the method sets proper data in notifications and map.
  std::map<std::string, std::string> db_tokens;
  std::map<std::string, std::string> memory_tokens;

  service_.LoadTokensIntoMemory(db_tokens, &memory_tokens);
  EXPECT_TRUE(db_tokens.empty());
  EXPECT_TRUE(memory_tokens.empty());
  EXPECT_EQ(0U, success_tracker_.size());

  std::string service;
  std::string token;
  for (int i = 0; i < TokenService::kNumServices; ++i) {
    service = TokenService::kServices[i];
    TestLoadSingleToken(&db_tokens, &memory_tokens, service);
  }
  for (int i = 0; i < TokenService::kNumOAuthServices; ++i) {
    service = TokenService::kOAuthServices[i];
    TestLoadSingleToken(&db_tokens, &memory_tokens, service);
  }
  service = GaiaConstants::kGaiaOAuth2LoginRefreshToken;
  TestLoadSingleToken(&db_tokens, &memory_tokens, service);
  service = GaiaConstants::kGaiaOAuth2LoginAccessToken;
  TestLoadSingleToken(&db_tokens, &memory_tokens, service);
}

TEST_F(TokenServiceTest, LoadTokensIntoMemoryAdvanced) {
  // LoadTokensIntoMemory should avoid setting tokens already in the
  // token map.
  std::map<std::string, std::string> db_tokens;
  std::map<std::string, std::string> memory_tokens;

  db_tokens["ignore"] = "token";

  service_.LoadTokensIntoMemory(db_tokens, &memory_tokens);
  EXPECT_TRUE(memory_tokens.empty());
  db_tokens[GaiaConstants::kSyncService] = "pepper";

  service_.LoadTokensIntoMemory(db_tokens, &memory_tokens);
  EXPECT_EQ(1U, memory_tokens.count(GaiaConstants::kSyncService));
  EXPECT_EQ(memory_tokens[GaiaConstants::kSyncService], "pepper");
  EXPECT_EQ(1U, success_tracker_.size());
  success_tracker_.Reset();

  // SyncService token is already in memory. Pretend we got it off
  // the disk as well, but an older token.
  db_tokens[GaiaConstants::kSyncService] = "ignoreme";
  db_tokens[GaiaConstants::kSyncServiceOAuth] = "tomato";
  service_.LoadTokensIntoMemory(db_tokens, &memory_tokens);

  EXPECT_EQ(2U, memory_tokens.size());
  EXPECT_EQ(1U, memory_tokens.count(GaiaConstants::kSyncServiceOAuth));
  EXPECT_EQ(memory_tokens[GaiaConstants::kSyncServiceOAuth], "tomato");
  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_EQ(1U, memory_tokens.count(GaiaConstants::kSyncService));
  EXPECT_EQ(memory_tokens[GaiaConstants::kSyncService], "pepper");
}

TEST_F(TokenServiceTest, WebDBLoadIntegration) {
  service_.LoadTokensFromDB();
  WaitForDBLoadCompletion();
  EXPECT_EQ(0U, success_tracker_.size());

  // Should result in DB write.
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_EQ(1U, success_tracker_.size());
  service_.OnOAuthGetAccessTokenSuccess("token1", "secret");
  service_.OnOAuthWrapBridgeSuccess(
      GaiaConstants::kSyncServiceOAuth, "token2", "3600");
  EXPECT_EQ(2U, success_tracker_.size());

  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  // Clean slate.
  service_.ResetCredentialsInMemory();
  success_tracker_.Reset();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));

  service_.LoadTokensFromDB();
  WaitForDBLoadCompletion();

  EXPECT_EQ(2U, success_tracker_.size());
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  EXPECT_TRUE(service_.HasLsid());
  EXPECT_TRUE(service_.HasOAuthCredentials());
}

TEST_F(TokenServiceTest, MultipleLoadResetIntegration) {
  // Should result in DB write.
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  service_.ResetCredentialsInMemory();
  success_tracker_.Reset();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasLsid());

  service_.OnOAuthGetAccessTokenSuccess("token2", "secret");
  service_.OnOAuthWrapBridgeSuccess(
      GaiaConstants::kSyncServiceOAuth, "token3", "3600");
  service_.ResetCredentialsInMemory();
  success_tracker_.Reset();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  EXPECT_FALSE(service_.HasOAuthCredentials());

  service_.LoadTokensFromDB();
  WaitForDBLoadCompletion();

  service_.LoadTokensFromDB();  // Should do nothing.
  WaitForDBLoadCompletion();

  EXPECT_EQ(2U, success_tracker_.size());
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  EXPECT_TRUE(service_.HasLsid());
  EXPECT_TRUE(service_.HasOAuthCredentials());

  // Reset it one more time so there's no surprises.
  service_.ResetCredentialsInMemory();
  success_tracker_.Reset();

  service_.LoadTokensFromDB();
  WaitForDBLoadCompletion();

  EXPECT_EQ(2U, success_tracker_.size());
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncServiceOAuth));
  EXPECT_TRUE(service_.HasLsid());
  EXPECT_TRUE(service_.HasOAuthCredentials());
}

#ifndef NDEBUG
class TokenServiceCommandLineTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() {
    CommandLine original_cl(*CommandLine::ForCurrentProcess());
    CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kSetToken, "my_service:my_value");
    TokenServiceTestHarness::SetUp();
    service_.UpdateCredentials(credentials_);
    service_.UpdateOAuthCredentials(oauth_token_, oauth_secret_);

    *CommandLine::ForCurrentProcess() = original_cl;
  }
};

TEST_F(TokenServiceCommandLineTest, TestValueOverride) {
  EXPECT_TRUE(service_.HasTokenForService("my_service"));
  EXPECT_EQ("my_value", service_.GetTokenForService("my_service"));
}
#endif   // ifndef NDEBUG
