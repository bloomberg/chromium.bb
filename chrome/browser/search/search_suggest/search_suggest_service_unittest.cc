// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_suggest/search_suggest_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/search/search_suggest/search_suggest_data.h"
#include "chrome/browser/search/search_suggest/search_suggest_loader.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/identity/public/cpp/identity_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::InSequence;
using testing::StrictMock;

class FakeSearchSuggestLoader : public SearchSuggestLoader {
 public:
  void Load(SearchSuggestionsCallback callback) override {
    callbacks_.push_back(std::move(callback));
  }

  GURL GetLoadURLForTesting() const override { return GURL(); }

  size_t GetCallbackCount() const { return callbacks_.size(); }

  void RespondToAllCallbacks(Status status,
                             const base::Optional<SearchSuggestData>& data) {
    for (SearchSuggestionsCallback& callback : callbacks_) {
      std::move(callback).Run(status, data);
    }
    callbacks_.clear();
  }

 private:
  std::vector<SearchSuggestionsCallback> callbacks_;
};

class SearchSuggestServiceTest : public testing::Test {
 public:
  SearchSuggestServiceTest()
      : signin_client_(&pref_service_),
        identity_env_(&test_url_loader_factory_) {
    // GaiaCookieManagerService calls static methods of AccountTrackerService
    // which access prefs.
    AccountTrackerService::RegisterPrefs(pref_service_.registry());

    auto loader = std::make_unique<FakeSearchSuggestLoader>();
    loader_ = loader.get();
    service_ = std::make_unique<SearchSuggestService>(
        identity_env_.identity_manager(), std::move(loader));

    identity_env_.MakePrimaryAccountAvailable("example@gmail.com");
    identity_env_.SetAutomaticIssueOfAccessTokens(true);
  }

  FakeSearchSuggestLoader* loader() { return loader_; }
  SearchSuggestService* service() { return service_.get(); }

  void SignIn() {
    AccountInfo account_info =
        identity_env_.MakeAccountAvailable("test@email.com");
    identity_env_.SetCookieAccounts({{account_info.email, account_info.gaia}});
    task_environment_.RunUntilIdle();
  }

  void SignOut() {
    identity_env_.SetCookieAccounts({});
    task_environment_.RunUntilIdle();
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;

  sync_preferences::TestingPrefServiceSyncable pref_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  TestSigninClient signin_client_;
  FakeOAuth2TokenService token_service_;
  identity::IdentityTestEnvironment identity_env_;

  // Owned by the service.
  FakeSearchSuggestLoader* loader_;

  std::unique_ptr<SearchSuggestService> service_;
};

TEST_F(SearchSuggestServiceTest, NoRefreshOnSignedOutRequest) {
  ASSERT_THAT(service()->search_suggest_data(), Eq(base::nullopt));

  // Request a refresh. That should do nothing as no user is signed-in.
  service()->Refresh();
  EXPECT_THAT(loader()->GetCallbackCount(), Eq(0u));
  EXPECT_THAT(service()->search_suggest_data(), Eq(base::nullopt));
}

TEST_F(SearchSuggestServiceTest, RefreshesOnSignedInRequest) {
  ASSERT_THAT(service()->search_suggest_data(), Eq(base::nullopt));
  SignIn();

  // Request a refresh. That should arrive at the loader.
  service()->Refresh();
  EXPECT_THAT(loader()->GetCallbackCount(), Eq(1u));

  // Fulfill it.
  SearchSuggestData data;
  data.suggestions_html = "<div></div>";
  loader()->RespondToAllCallbacks(SearchSuggestLoader::Status::OK, data);
  EXPECT_THAT(service()->search_suggest_data(), Eq(data));

  // Request another refresh.
  service()->Refresh();
  EXPECT_THAT(loader()->GetCallbackCount(), Eq(1u));

  // For now, the old data should still be there.
  EXPECT_THAT(service()->search_suggest_data(), Eq(data));

  // Fulfill the second request.
  SearchSuggestData other_data;
  other_data.suggestions_html = "<div>Different!</div>";
  loader()->RespondToAllCallbacks(SearchSuggestLoader::Status::OK, other_data);
  EXPECT_THAT(service()->search_suggest_data(), Eq(other_data));
}

TEST_F(SearchSuggestServiceTest, KeepsCacheOnTransientError) {
  ASSERT_THAT(service()->search_suggest_data(), Eq(base::nullopt));
  SignIn();

  // Load some data.
  service()->Refresh();
  SearchSuggestData data;
  data.suggestions_html = "<div></div>";
  loader()->RespondToAllCallbacks(SearchSuggestLoader::Status::OK, data);
  ASSERT_THAT(service()->search_suggest_data(), Eq(data));

  // Request a refresh and respond with a transient error.
  service()->Refresh();
  loader()->RespondToAllCallbacks(SearchSuggestLoader::Status::TRANSIENT_ERROR,
                                  base::nullopt);
  // Cached data should still be there.
  EXPECT_THAT(service()->search_suggest_data(), Eq(data));
}

TEST_F(SearchSuggestServiceTest, ClearsCacheOnFatalError) {
  ASSERT_THAT(service()->search_suggest_data(), Eq(base::nullopt));
  SignIn();

  // Load some data.
  service()->Refresh();
  SearchSuggestData data;
  data.suggestions_html = "<div></div>";
  loader()->RespondToAllCallbacks(SearchSuggestLoader::Status::OK, data);
  ASSERT_THAT(service()->search_suggest_data(), Eq(data));

  // Request a refresh and respond with a fatal error.
  service()->Refresh();
  loader()->RespondToAllCallbacks(SearchSuggestLoader::Status::FATAL_ERROR,
                                  base::nullopt);
  // Cached data should be gone now.
  EXPECT_THAT(service()->search_suggest_data(), Eq(base::nullopt));
}

TEST_F(SearchSuggestServiceTest, ResetsOnSignOut) {
  ASSERT_THAT(service()->search_suggest_data(), Eq(base::nullopt));
  SignIn();

  // Load some data.
  service()->Refresh();
  SearchSuggestData data;
  data.suggestions_html = "<div></div>";
  loader()->RespondToAllCallbacks(SearchSuggestLoader::Status::OK, data);
  ASSERT_THAT(service()->search_suggest_data(), Eq(data));

  // Sign out. This should clear the cached data and notify the observer.
  SignOut();
  EXPECT_THAT(service()->search_suggest_data(), Eq(base::nullopt));
}
