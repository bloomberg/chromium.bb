// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_fetcher.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Eq;
using testing::InSequence;
using testing::StrictMock;

class FakeOneGoogleBarFetcher : public OneGoogleBarFetcher {
 public:
  void Fetch(OneGoogleCallback callback) override {
    callbacks_.push_back(std::move(callback));
  }

  size_t GetCallbackCount() const { return callbacks_.size(); }

  void RespondToAllCallbacks(Status status,
                             const base::Optional<OneGoogleBarData>& data) {
    for (OneGoogleCallback& callback : callbacks_) {
      std::move(callback).Run(status, data);
    }
    callbacks_.clear();
  }

 private:
  std::vector<OneGoogleCallback> callbacks_;
};

class MockOneGoogleBarServiceObserver : public OneGoogleBarServiceObserver {
 public:
  MOCK_METHOD0(OnOneGoogleBarDataUpdated, void());
};

class OneGoogleBarServiceTest : public testing::Test {
 public:
  OneGoogleBarServiceTest()
      : signin_client_(&pref_service_),
        signin_manager_(&signin_client_, &account_tracker_) {
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());

    auto fetcher = base::MakeUnique<FakeOneGoogleBarFetcher>();
    fetcher_ = fetcher.get();
    service_ = base::MakeUnique<OneGoogleBarService>(&signin_manager_,
                                                     std::move(fetcher));
  }

  FakeOneGoogleBarFetcher* fetcher() { return fetcher_; }
  OneGoogleBarService* service() { return service_.get(); }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestSigninClient signin_client_;
  AccountTrackerService account_tracker_;
  FakeSigninManagerBase signin_manager_;

  // Owned by the service.
  FakeOneGoogleBarFetcher* fetcher_;

  std::unique_ptr<OneGoogleBarService> service_;
};

TEST_F(OneGoogleBarServiceTest, RefreshesOnRequest) {
  ASSERT_THAT(service()->one_google_bar_data(), Eq(base::nullopt));

  // Request a refresh. That should arrive at the fetcher.
  service()->Refresh();
  EXPECT_THAT(fetcher()->GetCallbackCount(), Eq(1u));

  // Fulfill it.
  OneGoogleBarData data;
  data.bar_html = "<div></div>";
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::OK, data);
  EXPECT_THAT(service()->one_google_bar_data(), Eq(data));

  // Request another refresh.
  service()->Refresh();
  EXPECT_THAT(fetcher()->GetCallbackCount(), Eq(1u));

  // For now, the old data should still be there.
  EXPECT_THAT(service()->one_google_bar_data(), Eq(data));

  // Fulfill the second request.
  OneGoogleBarData other_data;
  other_data.bar_html = "<div>Different!</div>";
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::OK, other_data);
  EXPECT_THAT(service()->one_google_bar_data(), Eq(other_data));
}

TEST_F(OneGoogleBarServiceTest, NotifiesObserverOnChanges) {
  InSequence s;

  ASSERT_THAT(service()->one_google_bar_data(), Eq(base::nullopt));

  StrictMock<MockOneGoogleBarServiceObserver> observer;
  service()->AddObserver(&observer);

  // Empty result from a fetch should result in a notification.
  service()->Refresh();
  EXPECT_CALL(observer, OnOneGoogleBarDataUpdated());
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::OK,
                                   base::nullopt);
  EXPECT_THAT(service()->one_google_bar_data(), Eq(base::nullopt));

  // Non-empty response should result in a notification.
  service()->Refresh();
  OneGoogleBarData data;
  data.bar_html = "<div></div>";
  EXPECT_CALL(observer, OnOneGoogleBarDataUpdated());
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::OK, data);
  EXPECT_THAT(service()->one_google_bar_data(), Eq(data));

  // Identical response should still result in a notification.
  service()->Refresh();
  OneGoogleBarData identical_data = data;
  EXPECT_CALL(observer, OnOneGoogleBarDataUpdated());
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::OK,
                                   identical_data);
  EXPECT_THAT(service()->one_google_bar_data(), Eq(data));

  // Different response should result in a notification.
  service()->Refresh();
  OneGoogleBarData other_data;
  data.bar_html = "<div>Different</div>";
  EXPECT_CALL(observer, OnOneGoogleBarDataUpdated());
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::OK, other_data);
  EXPECT_THAT(service()->one_google_bar_data(), Eq(other_data));

  service()->RemoveObserver(&observer);
}

TEST_F(OneGoogleBarServiceTest, KeepsCacheOnTransientError) {
  // Load some data.
  service()->Refresh();
  OneGoogleBarData data;
  data.bar_html = "<div></div>";
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::OK, data);
  ASSERT_THAT(service()->one_google_bar_data(), Eq(data));

  StrictMock<MockOneGoogleBarServiceObserver> observer;
  service()->AddObserver(&observer);

  // Request a refresh and respond with a transient error.
  service()->Refresh();
  EXPECT_CALL(observer, OnOneGoogleBarDataUpdated());
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::TRANSIENT_ERROR,
                                   base::nullopt);
  // Cached data should still be there.
  EXPECT_THAT(service()->one_google_bar_data(), Eq(data));

  service()->RemoveObserver(&observer);
}

TEST_F(OneGoogleBarServiceTest, ClearsCacheOnFatalError) {
  // Load some data.
  service()->Refresh();
  OneGoogleBarData data;
  data.bar_html = "<div></div>";
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::OK, data);
  ASSERT_THAT(service()->one_google_bar_data(), Eq(data));

  StrictMock<MockOneGoogleBarServiceObserver> observer;
  service()->AddObserver(&observer);

  // Request a refresh and respond with a fatal error.
  service()->Refresh();
  EXPECT_CALL(observer, OnOneGoogleBarDataUpdated());
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::FATAL_ERROR,
                                   base::nullopt);
  // Cached data should be gone now.
  EXPECT_THAT(service()->one_google_bar_data(), Eq(base::nullopt));

  service()->RemoveObserver(&observer);
}

#if !defined(OS_CHROMEOS)

// Like OneGoogleBarServiceTest, but it has a FakeSigninManager (rather than
// FakeSigninManagerBase), so it can simulate sign-in and sign-out.
class OneGoogleBarServiceSignInTest : public testing::Test {
 public:
  OneGoogleBarServiceSignInTest()
      : signin_client_(&pref_service_),
        signin_manager_(&signin_client_,
                        &token_service_,
                        &account_tracker_,
                        nullptr) {
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());
    AccountTrackerService::RegisterPrefs(pref_service_.registry());

    account_tracker_.Initialize(&signin_client_);

    auto fetcher = base::MakeUnique<FakeOneGoogleBarFetcher>();
    fetcher_ = fetcher.get();
    service_ = base::MakeUnique<OneGoogleBarService>(&signin_manager_,
                                                     std::move(fetcher));
  }

  void SignIn() { signin_manager_.SignIn("account", "username", "pass"); }
  void SignOut() { signin_manager_.ForceSignOut(); }

  FakeOneGoogleBarFetcher* fetcher() { return fetcher_; }
  OneGoogleBarService* service() { return service_.get(); }

 private:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  TestSigninClient signin_client_;
  FakeProfileOAuth2TokenService token_service_;
  AccountTrackerService account_tracker_;
  FakeSigninManager signin_manager_;

  // Owned by the service.
  FakeOneGoogleBarFetcher* fetcher_;

  std::unique_ptr<OneGoogleBarService> service_;
};

TEST_F(OneGoogleBarServiceSignInTest, ResetsOnSignIn) {
  // Load some data.
  service()->Refresh();
  OneGoogleBarData data;
  data.bar_html = "<div></div>";
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::OK, data);
  ASSERT_THAT(service()->one_google_bar_data(), Eq(data));

  // Sign in. This should clear the cached data.
  SignIn();
  EXPECT_THAT(service()->one_google_bar_data(), Eq(base::nullopt));
}

TEST_F(OneGoogleBarServiceSignInTest, ResetsOnSignOut) {
  SignIn();

  // Load some data.
  service()->Refresh();
  OneGoogleBarData data;
  data.bar_html = "<div></div>";
  fetcher()->RespondToAllCallbacks(OneGoogleBarFetcher::Status::OK, data);
  ASSERT_THAT(service()->one_google_bar_data(), Eq(data));

  // Sign out. This should clear the cached data.
  SignOut();
  EXPECT_THAT(service()->one_google_bar_data(), Eq(base::nullopt));
}

#endif  // OS_CHROMEOS
