// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/subscription_manager.h"

#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/ntp_snippets/remote/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "google_apis/gaia/fake_oauth2_token_service_delegate.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

const char kTestEmail[] = "test@email.com";
const char kAPIKey[] = "fakeAPIkey";
const char kSubscriptionUrl[] = "http://valid-url.test/subscribe";
const char kSubscriptionUrlSignedIn[] = "http://valid-url.test/subscribe";
;
const char kSubscriptionUrlSignedOut[] =
    "http://valid-url.test/subscribe?key=fakeAPIkey";
const char kUnsubscriptionUrl[] = "http://valid-url.test/unsubscribe";
const char kUnsubscriptionUrlSignedIn[] = "http://valid-url.test/unsubscribe";
const char kUnsubscriptionUrlSignedOut[] =
    "http://valid-url.test/unsubscribe?key=fakeAPIkey";

class SubscriptionManagerTest : public testing::Test {
 public:
  SubscriptionManagerTest()
      : request_context_getter_(
            new net::TestURLRequestContextGetter(message_loop_.task_runner())) {
  }

  void SetUp() override {
    SubscriptionManager::RegisterProfilePrefs(
        utils_.pref_service()->registry());
  }

  scoped_refptr<net::URLRequestContextGetter> GetRequestContext() {
    return request_context_getter_.get();
  }

  PrefService* GetPrefService() { return utils_.pref_service(); }

  FakeProfileOAuth2TokenService* GetOAuth2TokenService() {
    return utils_.token_service();
  }

  SigninManagerBase* GetSigninManager() { return utils_.fake_signin_manager(); }

  net::TestURLFetcher* GetRunningFetcher() {
    // All created TestURLFetchers have ID 0 by default.
    net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
    DCHECK(url_fetcher);
    return url_fetcher;
  }

  void RespondToSubscriptionRequestSuccessfully(bool is_signed_in) {
    net::TestURLFetcher* url_fetcher = GetRunningFetcher();
    if (is_signed_in) {
      ASSERT_EQ(GURL(kSubscriptionUrlSignedIn), url_fetcher->GetOriginalURL());
    } else {
      ASSERT_EQ(GURL(kSubscriptionUrlSignedOut), url_fetcher->GetOriginalURL());
    }
    RespondSuccessfully();
  }

  void RespondToUnsubscriptionRequestSuccessfully(bool is_signed_in) {
    net::TestURLFetcher* url_fetcher = GetRunningFetcher();
    if (is_signed_in) {
      ASSERT_EQ(GURL(kUnsubscriptionUrlSignedIn),
                url_fetcher->GetOriginalURL());
    } else {
      ASSERT_EQ(GURL(kUnsubscriptionUrlSignedOut),
                url_fetcher->GetOriginalURL());
    }
    RespondSuccessfully();
  }

  void RespondToSubscriptionWithError(bool is_signed_in, int error_code) {
    net::TestURLFetcher* url_fetcher = GetRunningFetcher();
    if (is_signed_in) {
      ASSERT_EQ(GURL(kSubscriptionUrlSignedIn), url_fetcher->GetOriginalURL());
    } else {
      ASSERT_EQ(GURL(kSubscriptionUrlSignedOut), url_fetcher->GetOriginalURL());
    }
    RespondWithError(error_code);
  }

  void RespondToUnsubscriptionWithError(bool is_signed_in, int error_code) {
    net::TestURLFetcher* url_fetcher = GetRunningFetcher();
    if (is_signed_in) {
      ASSERT_EQ(GURL(kUnsubscriptionUrlSignedIn),
                url_fetcher->GetOriginalURL());
    } else {
      ASSERT_EQ(GURL(kUnsubscriptionUrlSignedOut),
                url_fetcher->GetOriginalURL());
    }
    RespondWithError(error_code);
  }

#if !defined(OS_CHROMEOS)
  void SignIn() {
    utils_.fake_signin_manager()->SignIn(kTestEmail, "user", "pass");
  }

  void SignOut() { utils_.fake_signin_manager()->ForceSignOut(); }
#endif  // !defined(OS_CHROMEOS)

  void IssueRefreshToken(FakeProfileOAuth2TokenService* auth_token_service) {
    auth_token_service->GetDelegate()->UpdateCredentials(kTestEmail, "token");
  }

  void IssueAccessToken(FakeProfileOAuth2TokenService* auth_token_service) {
    auth_token_service->IssueAllTokensForAccount(kTestEmail, "access_token",
                                                 base::Time::Max());
  }

 private:
  void RespondSuccessfully() {
    net::TestURLFetcher* url_fetcher = GetRunningFetcher();
    url_fetcher->set_status(net::URLRequestStatus());
    url_fetcher->set_response_code(net::HTTP_OK);
    url_fetcher->SetResponseString(std::string());
    // Call the URLFetcher delegate to continue the test.
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }

  void RespondWithError(int error_code) {
    net::TestURLFetcher* url_fetcher = GetRunningFetcher();
    url_fetcher->set_status(net::URLRequestStatus::FromError(error_code));
    url_fetcher->SetResponseString(std::string());
    // Call the URLFetcher delegate to continue the test.
    url_fetcher->delegate()->OnURLFetchComplete(url_fetcher);
  }


  base::MessageLoop message_loop_;
  test::RemoteSuggestionsTestUtils utils_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  net::TestURLFetcherFactory url_fetcher_factory_;
};

TEST_F(SubscriptionManagerTest, SubscribeSuccessfully) {
  std::string subscription_token = "1234567890";
  SubscriptionManager manager(GetRequestContext(), GetPrefService(),
                              GetSigninManager(), GetOAuth2TokenService(),
                              kAPIKey, GURL(kSubscriptionUrl),
                              GURL(kUnsubscriptionUrl));
  manager.Subscribe(subscription_token);
  RespondToSubscriptionRequestSuccessfully(/*is_signed_in=*/false);
  ASSERT_TRUE(manager.IsSubscribed());
  EXPECT_EQ(subscription_token, GetPrefService()->GetString(
                                    prefs::kBreakingNewsSubscriptionDataToken));
  EXPECT_FALSE(GetPrefService()->GetBoolean(
      prefs::kBreakingNewsSubscriptionDataIsAuthenticated));
}

// This test is relevant only on non-ChromeOS platforms, as the flow being
// tested here is not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(SubscriptionManagerTest,
       ShouldSubscribeWithAuthenticationWhenAuthenticated) {
  // Sign in.
  FakeProfileOAuth2TokenService* auth_token_service = GetOAuth2TokenService();
  SignIn();
  IssueRefreshToken(auth_token_service);

  // Create manager and subscribe.
  std::string subscription_token = "1234567890";
  SubscriptionManager manager(GetRequestContext(), GetPrefService(),
                              GetSigninManager(), auth_token_service, kAPIKey,
                              GURL(kSubscriptionUrl), GURL(kUnsubscriptionUrl));
  manager.Subscribe(subscription_token);

  // Make sure that subscription is pending an access token.
  ASSERT_FALSE(manager.IsSubscribed());
  ASSERT_EQ(1u, auth_token_service->GetPendingRequests().size());

  // Issue the access token and respond to the subscription request.
  IssueAccessToken(auth_token_service);
  ASSERT_FALSE(manager.IsSubscribed());
  RespondToSubscriptionRequestSuccessfully(/*is_signed_in=*/true);
  ASSERT_TRUE(manager.IsSubscribed());

  // Check that we are now subscribed correctly with authentication.
  EXPECT_EQ(subscription_token, GetPrefService()->GetString(
                                    prefs::kBreakingNewsSubscriptionDataToken));
  EXPECT_TRUE(GetPrefService()->GetBoolean(
      prefs::kBreakingNewsSubscriptionDataIsAuthenticated));
}
#endif

TEST_F(SubscriptionManagerTest, ShouldNotSubscribeIfError) {
  std::string subscription_token = "1234567890";
  SubscriptionManager manager(GetRequestContext(), GetPrefService(),
                              GetSigninManager(), GetOAuth2TokenService(),
                              kAPIKey, GURL(kSubscriptionUrl),
                              GURL(kUnsubscriptionUrl));

  manager.Subscribe(subscription_token);
  RespondToSubscriptionWithError(/*is_signed_in=*/false, net::ERR_TIMED_OUT);
  EXPECT_FALSE(manager.IsSubscribed());
}

TEST_F(SubscriptionManagerTest, UnsubscribeSuccessfully) {
  std::string subscription_token = "1234567890";
  SubscriptionManager manager(GetRequestContext(), GetPrefService(),
                              GetSigninManager(), GetOAuth2TokenService(),
                              kAPIKey, GURL(kSubscriptionUrl),
                              GURL(kUnsubscriptionUrl));
  manager.Subscribe(subscription_token);
  RespondToSubscriptionRequestSuccessfully(/*is_signed_in=*/false);
  ASSERT_TRUE(manager.IsSubscribed());
  manager.Unsubscribe();
  RespondToUnsubscriptionRequestSuccessfully(/*is_signed_in=*/false);
  EXPECT_FALSE(manager.IsSubscribed());
  EXPECT_FALSE(
      GetPrefService()->HasPrefPath(prefs::kBreakingNewsSubscriptionDataToken));
}

TEST_F(SubscriptionManagerTest,
       ShouldRemainSubscribedIfErrorDuringUnsubscribe) {
  std::string subscription_token = "1234567890";
  SubscriptionManager manager(GetRequestContext(), GetPrefService(),
                              GetSigninManager(), GetOAuth2TokenService(),
                              kAPIKey, GURL(kSubscriptionUrl),
                              GURL(kUnsubscriptionUrl));
  manager.Subscribe(subscription_token);
  RespondToSubscriptionRequestSuccessfully(/*is_signed_in=*/false);
  ASSERT_TRUE(manager.IsSubscribed());
  manager.Unsubscribe();
  RespondToUnsubscriptionWithError(/*is_signed_in=*/false, net::ERR_TIMED_OUT);
  ASSERT_TRUE(manager.IsSubscribed());
  EXPECT_EQ(subscription_token, GetPrefService()->GetString(
                                    prefs::kBreakingNewsSubscriptionDataToken));
}

// This test is relevant only on non-ChromeOS platforms, as the flow being
// tested here is not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(SubscriptionManagerTest, ShouldResubscribeIfSignInAfterSubscription) {
  // Create manager and subscribe.
  FakeProfileOAuth2TokenService* auth_token_service = GetOAuth2TokenService();
  std::string subscription_token = "1234567890";
  SubscriptionManager manager(GetRequestContext(), GetPrefService(),
                              GetSigninManager(), auth_token_service, kAPIKey,
                              GURL(kSubscriptionUrl), GURL(kUnsubscriptionUrl));
  manager.Subscribe(subscription_token);
  RespondToSubscriptionRequestSuccessfully(/*is_signed_in=*/false);
  ASSERT_FALSE(manager.NeedsToResubscribe());

  // Sign in. This should trigger a resubscribe.
  SignIn();
  IssueRefreshToken(auth_token_service);
  ASSERT_TRUE(manager.NeedsToResubscribe());
  ASSERT_EQ(1u, auth_token_service->GetPendingRequests().size());
  IssueAccessToken(auth_token_service);
  RespondToSubscriptionRequestSuccessfully(/*is_signed_in=*/true);

  // Check that we are now subscribed with authentication.
  EXPECT_TRUE(GetPrefService()->GetBoolean(
      prefs::kBreakingNewsSubscriptionDataIsAuthenticated));
}
#endif

// This test is relevant only on non-ChromeOS platforms, as the flow being
// tested here is not possible on ChromeOS.
#if !defined(OS_CHROMEOS)
TEST_F(SubscriptionManagerTest, ShouldResubscribeIfSignOutAfterSubscription) {
  // Signin and subscribe.
  FakeProfileOAuth2TokenService* auth_token_service = GetOAuth2TokenService();
  SignIn();
  IssueRefreshToken(auth_token_service);
  std::string subscription_token = "1234567890";
  SubscriptionManager manager(GetRequestContext(), GetPrefService(),
                              GetSigninManager(), auth_token_service, kAPIKey,
                              GURL(kSubscriptionUrl), GURL(kUnsubscriptionUrl));
  manager.Subscribe(subscription_token);
  ASSERT_EQ(1u, auth_token_service->GetPendingRequests().size());
  IssueAccessToken(auth_token_service);
  RespondToSubscriptionRequestSuccessfully(/*is_signed_in=*/true);

  // Signout, this should trigger a resubscribe.
  SignOut();
  EXPECT_TRUE(manager.NeedsToResubscribe());
  RespondToSubscriptionRequestSuccessfully(/*is_signed_in=*/false);

  // Check that we are now subscribed without authentication.
  EXPECT_FALSE(GetPrefService()->GetBoolean(
      prefs::kBreakingNewsSubscriptionDataIsAuthenticated));
}
#endif

TEST_F(SubscriptionManagerTest,
       ShouldUpdateTokenInPrefWhenResubscribeWithChangeInToken) {
  // Create manager and subscribe.
  std::string old_subscription_token = "1234567890";
  SubscriptionManager manager(GetRequestContext(), GetPrefService(),
                              GetSigninManager(), GetOAuth2TokenService(),
                              kAPIKey, GURL(kSubscriptionUrl),
                              GURL(kUnsubscriptionUrl));
  manager.Subscribe(old_subscription_token);
  RespondToSubscriptionRequestSuccessfully(/*is_signed_in=*/false);
  EXPECT_EQ(
      old_subscription_token,
      GetPrefService()->GetString(prefs::kBreakingNewsSubscriptionDataToken));

  // Resubscribe with a new token.
  std::string new_subscription_token = "0987654321";
  manager.Resubscribe(new_subscription_token);
  // Resubscribe with a new token should issue an unsubscribe request before
  // subscribing.
  RespondToUnsubscriptionRequestSuccessfully(/*is_signed_in=*/false);
  RespondToSubscriptionRequestSuccessfully(/*is_signed_in=*/false);

  // Check we are now subscribed with the new token.
  EXPECT_EQ(
      new_subscription_token,
      GetPrefService()->GetString(prefs::kBreakingNewsSubscriptionDataToken));
}

}  // namespace ntp_snippets
