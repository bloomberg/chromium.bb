// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a unit test for the profile's token service.

#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/common/net/gaia/gaia_authenticator2_unittest.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/test/test_notification_tracker.h"

// TestNotificationTracker doesn't do a deep copy on the notification details.
// We have to in order to read it out, or we have a bad ptr, since the details
// are a reference on the stack.
class TokenAvailableTracker : public TestNotificationTracker {
 public:
  const TokenService::TokenAvailableDetails& get_last_token_details() {
    return details_;
  }

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    TestNotificationTracker::Observe(type, source, details);
    if (type == NotificationType::TOKEN_AVAILABLE) {
      Details<const TokenService::TokenAvailableDetails> full = details;
      details_ = *full.ptr();
    }
  }

  TokenService::TokenAvailableDetails details_;
};

class TokenFailedTracker : public TestNotificationTracker {
 public:
  const TokenService::TokenRequestFailedDetails& get_last_token_details() {
    return details_;
  }

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    TestNotificationTracker::Observe(type, source, details);
    if (type == NotificationType::TOKEN_REQUEST_FAILED) {
      Details<const TokenService::TokenRequestFailedDetails> full = details;
      details_ = *full.ptr();
    }
  }

  TokenService::TokenRequestFailedDetails details_;
};

class TokenServiceTest : public testing::Test {
 public:
  TokenServiceTest() {
    credentials_.sid = "sid";
    credentials_.lsid = "lsid";
    credentials_.token = "token";
    credentials_.data = "data";

    success_tracker_.ListenFor(NotificationType::TOKEN_AVAILABLE,
                               Source<TokenService>(&service_));
    failure_tracker_.ListenFor(NotificationType::TOKEN_REQUEST_FAILED,
                               Source<TokenService>(&service_));

    service_.Initialize("test", profile_.GetRequestContext(), credentials_);
  }

  TokenService service_;
  TokenAvailableTracker success_tracker_;
  TokenFailedTracker failure_tracker_;
  GaiaAuthConsumer::ClientLoginResult credentials_;

 private:
  TestingProfile profile_;
};

TEST_F(TokenServiceTest, SanityCheck) {
  EXPECT_TRUE(service_.HasLsid());
  EXPECT_EQ(service_.GetLsid(), "lsid");
  EXPECT_FALSE(service_.HasTokenForService("nonexistant service"));
}

TEST_F(TokenServiceTest, NoToken) {
  EXPECT_FALSE(service_.HasTokenForService("nonexistant service"));
  EXPECT_EQ(service_.GetTokenForService("nonexistant service"), std::string());
}

TEST_F(TokenServiceTest, NotificationSuccess) {
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kSyncService, "token");
  EXPECT_EQ(1U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());

  TokenService::TokenAvailableDetails details =
      success_tracker_.get_last_token_details();
  // MSVC doesn't like this comparison as EQ
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncService);
  EXPECT_EQ(details.token(), "token");
}

TEST_F(TokenServiceTest, NotificationFailed) {
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(0U, failure_tracker_.size());
  GaiaAuthConsumer::GaiaAuthError error;
  error.code = GaiaAuthConsumer::REQUEST_CANCELED;
  service_.OnIssueAuthTokenFailure(GaiaConstants::kSyncService, error);
  EXPECT_EQ(0U, success_tracker_.size());
  EXPECT_EQ(1U, failure_tracker_.size());

  TokenService::TokenRequestFailedDetails details =
      failure_tracker_.get_last_token_details();

  // MSVC doesn't like this comparison as EQ
  EXPECT_TRUE(details.service() == GaiaConstants::kSyncService);
  EXPECT_TRUE(details.error() == error);  // Struct has no print function
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

  // Check 2nd service
  service_.OnIssueAuthTokenSuccess(GaiaConstants::kTalkService, "token2");
  EXPECT_TRUE(service_.HasTokenForService(GaiaConstants::kTalkService));
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kTalkService), "token2");

  // Didn't change
  EXPECT_EQ(service_.GetTokenForService(GaiaConstants::kSyncService), "token");
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
}
