// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/subscription_manager.h"

#include "base/message_loop/message_loop.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

class SubscriptionManagerTest : public testing::Test {
 public:
  SubscriptionManagerTest()
      : request_context_getter_(
            new net::TestURLRequestContextGetter(message_loop_.task_runner())),
        pref_service_(base::MakeUnique<TestingPrefServiceSimple>()) {}

  void SetUp() override {
    SubscriptionManager::RegisterProfilePrefs(pref_service_->registry());
  }

  scoped_refptr<net::URLRequestContextGetter> GetRequestContext() {
    return request_context_getter_.get();
  }

  PrefService* GetPrefService() { return pref_service_.get(); }

  net::TestURLFetcher* GetRunningFetcher() {
    // All created TestURLFetchers have ID 0 by default.
    net::TestURLFetcher* url_fetcher = url_fetcher_factory_.GetFetcherByID(0);
    DCHECK(url_fetcher);
    return url_fetcher;
  }

  void RespondWithData(const std::string& data) {
    net::TestURLFetcher* url_fetcher = GetRunningFetcher();
    url_fetcher->set_status(net::URLRequestStatus());
    url_fetcher->set_response_code(net::HTTP_OK);
    url_fetcher->SetResponseString(data);
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

  const std::string url{"http://valid-url.test"};

 private:
  base::MessageLoop message_loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(SubscriptionManagerTest, SubscribeSuccessfully) {
  std::string token = "1234567890";
  SubscriptionManager manager(GetRequestContext(), GetPrefService(), GURL(url),
                              GURL(url));
  manager.Subscribe(token);
  RespondWithData("");
  EXPECT_TRUE(manager.IsSubscribed());
  EXPECT_EQ(GetPrefService()->GetString(
                ntp_snippets::prefs::kBreakingNewsSubscriptionDataToken),
            token);
}

TEST_F(SubscriptionManagerTest, SubscribeWithErrors) {
  std::string token = "1234567890";
  SubscriptionManager manager(GetRequestContext(), GetPrefService(), GURL(url),
                              GURL(url));
  manager.Subscribe(token);
  RespondWithError(net::ERR_TIMED_OUT);
  EXPECT_FALSE(manager.IsSubscribed());
  EXPECT_FALSE(GetPrefService()->HasPrefPath(
      ntp_snippets::prefs::kBreakingNewsSubscriptionDataToken));
}

TEST_F(SubscriptionManagerTest, UnsubscribeSuccessfully) {
  std::string token = "1234567890";
  GetPrefService()->SetString(
      ntp_snippets::prefs::kBreakingNewsSubscriptionDataToken, token);
  SubscriptionManager manager(GetRequestContext(), GetPrefService(), GURL(url),
                              GURL(url));
  manager.Unsubscribe(token);
  RespondWithData("");
  EXPECT_FALSE(manager.IsSubscribed());
  EXPECT_FALSE(GetPrefService()->HasPrefPath(
      ntp_snippets::prefs::kBreakingNewsSubscriptionDataToken));
}

TEST_F(SubscriptionManagerTest, UnsubscribeWithErrors) {
  std::string token = "1234567890";
  GetPrefService()->SetString(
      ntp_snippets::prefs::kBreakingNewsSubscriptionDataToken, token);
  SubscriptionManager manager(GetRequestContext(), GetPrefService(), GURL(url),
                              GURL(url));
  manager.Unsubscribe(token);
  RespondWithError(net::ERR_TIMED_OUT);
  EXPECT_TRUE(manager.IsSubscribed());
  EXPECT_EQ(GetPrefService()->GetString(
                ntp_snippets::prefs::kBreakingNewsSubscriptionDataToken),
            token);
}

}  // namespace ntp_snippets
