// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a unit test for the profile's token service.

#include "chrome/browser/net/gaia/token_service_unittest.h"

#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher_unittest.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/test_url_fetcher_factory.h"

TokenAvailableTracker::TokenAvailableTracker() {}

TokenAvailableTracker::~TokenAvailableTracker() {}

void TokenAvailableTracker::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  TestNotificationTracker::Observe(type, source, details);
  if (type == NotificationType::TOKEN_AVAILABLE) {
    Details<const TokenService::TokenAvailableDetails> full = details;
    details_ = *full.ptr();
  }
}

TokenFailedTracker::TokenFailedTracker() {}

TokenFailedTracker::~TokenFailedTracker() {}

void TokenFailedTracker::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  TestNotificationTracker::Observe(type, source, details);
  if (type == NotificationType::TOKEN_REQUEST_FAILED) {
    Details<const TokenService::TokenRequestFailedDetails> full = details;
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

  ASSERT_TRUE(db_thread_.Start());

  profile_.reset(new TestingProfile());
  profile_->CreateWebDataService(false);
  WaitForDBLoadCompletion();

  success_tracker_.ListenFor(NotificationType::TOKEN_AVAILABLE,
                             Source<TokenService>(&service_));
  failure_tracker_.ListenFor(NotificationType::TOKEN_REQUEST_FAILED,
                             Source<TokenService>(&service_));

  service_.Initialize("test", profile_.get());

  URLFetcher::set_factory(NULL);
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
      BrowserThread::DB, FROM_HERE, new SignalingTask(&done));
  done.Wait();

  // Notifications should be returned from the DB thread onto the UI thread.
  message_loop_.RunAllPending();
}

class TokenServiceTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() {
    TokenServiceTestHarness::SetUp();
    service_.UpdateCredentials(credentials_);
  }
};

TEST_F(TokenServiceTest, SanityCheck) {
  EXPECT_TRUE(service_.HasLsid());
  EXPECT_EQ(service_.GetLsid(), "lsid");
  EXPECT_FALSE(service_.HasTokenForService("nonexistent service"));
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

TEST_F(TokenServiceTest, OnTokenSuccess) {
  // Don't "start fetching", just go ahead and issue the callback.
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  // Gaia returns the entire result as the token so while this is a shared
  // result with ClientLogin, it doesn't matter, we should still get it back.
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");

  // Check the second service.
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kTalkService, "token2");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kTalkService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kTalkService), "token2");

  // It didn't change.
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");
}

TEST_F(TokenServiceTest, ResetSimple) {
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_TRUE(service_.HasLsid());

  service_.ResetCredentialsInMemory();

  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasLsid());
}

TEST_F(TokenServiceTest, ResetComplex) {
  TestURLFetcherFactory factory;
  URLFetcher::set_factory(&factory);
  service_.StartFetchingTokens();
  // You have to call delegates by hand with the test fetcher,
  // Let's pretend only one returned.

  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "eraseme");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService),
            "eraseme");
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));

  service_.ResetCredentialsInMemory();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));

  // Now start using it again.
  service_.UpdateCredentials(credentials_);
  service_.StartFetchingTokens();

  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kTalkService, "token2");

  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kTalkService), "token2");
}

TEST_F(TokenServiceTest, FullIntegration) {
  MockFactory<MockFetcher> factory;
  std::string result = "SID=sid\nLSID=lsid\nAuth=auth\n";
  factory.set_results(result);
  URLFetcher::set_factory(&factory);
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  service_.StartFetchingTokens();
  URLFetcher::set_factory(NULL);

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

  db_tokens[GaiaConstants::kSyncService] = "token";
  service_.LoadTokensIntoMemory(db_tokens, &memory_tokens);
  EXPECT_EQ(1U, success_tracker_.size());

  TokenService::TokenAvailableDetails details = success_tracker_.details();
  // MSVC doesn't like this comparison as EQ.
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncService);
  EXPECT_EQ(details.token(), "token");
  EXPECT_EQ(1U, memory_tokens.count(GaiaConstants::kSyncService));
  EXPECT_EQ(memory_tokens[GaiaConstants::kSyncService], "token");
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
  db_tokens[GaiaConstants::kTalkService] = "tomato";
  service_.LoadTokensIntoMemory(db_tokens, &memory_tokens);

  EXPECT_EQ(2U, memory_tokens.size());
  EXPECT_EQ(1U, memory_tokens.count(GaiaConstants::kTalkService));
  EXPECT_EQ(memory_tokens[GaiaConstants::kTalkService], "tomato");
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

  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  // Clean slate.
  service_.ResetCredentialsInMemory();
  success_tracker_.Reset();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));

  service_.LoadTokensFromDB();
  WaitForDBLoadCompletion();

  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  EXPECT_FALSE(service_.HasLsid());
}

TEST_F(TokenServiceTest, MultipleLoadResetIntegration) {
  // Should result in DB write.
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  service_.ResetCredentialsInMemory();
  success_tracker_.Reset();
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kSyncService));

  service_.LoadTokensFromDB();
  WaitForDBLoadCompletion();

  service_.LoadTokensFromDB();  // Should do nothing.
  WaitForDBLoadCompletion();

  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
  EXPECT_FALSE(service_.HasTokenForService(GaiaConstants::kTalkService));
  EXPECT_FALSE(service_.HasLsid());

  // Reset it one more time so there's no surprises.
  service_.ResetCredentialsInMemory();
  success_tracker_.Reset();

  service_.LoadTokensFromDB();
  WaitForDBLoadCompletion();

  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kSyncService));
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

    *CommandLine::ForCurrentProcess() = original_cl;
  }
};

TEST_F(TokenServiceCommandLineTest, TestValueOverride) {
  EXPECT_TRUE(service_.HasTokenForService("my_service"));
  EXPECT_EQ("my_value", service_.GetTokenForService("my_service"));
}
#endif   // ifndef NDEBUG
