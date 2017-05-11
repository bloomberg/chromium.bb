// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_service_impl.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/features.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_store.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

using sync_preferences::TestingPrefServiceSyncable;
using syncer::SyncServiceObserver;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::Eq;
using testing::Mock;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace {

const char kAccountId[] = "account";
const char kSuggestionsUrlPath[] = "/chromesuggestions";
const char kBlacklistUrlPath[] = "/chromesuggestions/blacklist";
const char kBlacklistClearUrlPath[] = "/chromesuggestions/blacklist/clear";
const char kTestTitle[] = "a title";
const char kTestUrl[] = "http://go.com";
const char kTestFaviconUrl[] =
    "https://s2.googleusercontent.com/s2/favicons?domain_url="
    "http://go.com&alt=s&sz=32";
const char kBlacklistedUrl[] = "http://blacklist.com";
const int64_t kTestSetExpiry = 12121212;  // This timestamp lies in the past.

// GMock matcher for protobuf equality.
MATCHER_P(EqualsProto, message, "") {
  // This implementation assumes protobuf serialization is deterministic, which
  // is true in practice but technically not something that code is supposed
  // to rely on.  However, it vastly simplifies the implementation.
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

}  // namespace

namespace suggestions {

SuggestionsProfile CreateSuggestionsProfile() {
  SuggestionsProfile profile;
  profile.set_timestamp(123);
  ChromeSuggestion* suggestion = profile.add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);
  return profile;
}

class MockSyncService : public syncer::FakeSyncService {
 public:
  MockSyncService() {}
  virtual ~MockSyncService() {}
  MOCK_CONST_METHOD0(CanSyncStart, bool());
  MOCK_CONST_METHOD0(IsSyncActive, bool());
  MOCK_CONST_METHOD0(ConfigurationDone, bool());
  MOCK_CONST_METHOD0(GetActiveDataTypes, syncer::ModelTypeSet());
};

class TestSuggestionsStore : public suggestions::SuggestionsStore {
 public:
  TestSuggestionsStore() { cached_suggestions = CreateSuggestionsProfile(); }
  bool LoadSuggestions(SuggestionsProfile* suggestions) override {
    suggestions->CopyFrom(cached_suggestions);
    return cached_suggestions.suggestions_size();
  }
  bool StoreSuggestions(const SuggestionsProfile& suggestions) override {
    cached_suggestions.CopyFrom(suggestions);
    return true;
  }
  void ClearSuggestions() override {
    cached_suggestions = SuggestionsProfile();
  }

  SuggestionsProfile cached_suggestions;
};

class MockImageManager : public suggestions::ImageManager {
 public:
  MockImageManager() {}
  virtual ~MockImageManager() {}
  MOCK_METHOD1(Initialize, void(const SuggestionsProfile&));
  MOCK_METHOD2(GetImageForURL,
               void(const GURL&,
                    base::Callback<void(const GURL&, const gfx::Image&)>));
  MOCK_METHOD2(AddImageURL, void(const GURL&, const GURL&));
};

class MockBlacklistStore : public suggestions::BlacklistStore {
 public:
  MOCK_METHOD1(BlacklistUrl, bool(const GURL&));
  MOCK_METHOD0(ClearBlacklist, void());
  MOCK_METHOD1(GetTimeUntilReadyForUpload, bool(base::TimeDelta*));
  MOCK_METHOD2(GetTimeUntilURLReadyForUpload,
               bool(const GURL&, base::TimeDelta*));
  MOCK_METHOD1(GetCandidateForUpload, bool(GURL*));
  MOCK_METHOD1(RemoveUrl, bool(const GURL&));
  MOCK_METHOD1(FilterSuggestions, void(SuggestionsProfile*));
};

class SuggestionsServiceTest : public testing::Test {
 protected:
  SuggestionsServiceTest()
      : signin_client_(&pref_service_),
        signin_manager_(&signin_client_, &account_tracker_),
        request_context_(new net::TestURLRequestContextGetter(
            io_message_loop_.task_runner())),
        mock_thumbnail_manager_(nullptr),
        mock_blacklist_store_(nullptr),
        test_suggestions_store_(nullptr) {
    SigninManagerBase::RegisterProfilePrefs(pref_service_.registry());
    SigninManagerBase::RegisterPrefs(pref_service_.registry());

    signin_manager_.SignIn(kAccountId);
    token_service_.UpdateCredentials(kAccountId, "refresh_token");
    token_service_.set_auto_post_fetch_response_on_message_loop(true);
  }

  ~SuggestionsServiceTest() override {}

  void SetUp() override {
    EXPECT_CALL(*sync_service(), CanSyncStart())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*sync_service(), IsSyncActive())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*sync_service(), ConfigurationDone())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*sync_service(), GetActiveDataTypes())
        .Times(AnyNumber())
        .WillRepeatedly(
            Return(syncer::ModelTypeSet(syncer::HISTORY_DELETE_DIRECTIVES)));
    // These objects are owned by the SuggestionsService, but we keep the
    // pointers around for testing.
    test_suggestions_store_ = new TestSuggestionsStore();
    mock_thumbnail_manager_ = new StrictMock<MockImageManager>();
    mock_blacklist_store_ = new StrictMock<MockBlacklistStore>();
    suggestions_service_ = base::MakeUnique<SuggestionsServiceImpl>(
        &signin_manager_, &token_service_, &mock_sync_service_,
        request_context_.get(), base::WrapUnique(test_suggestions_store_),
        base::WrapUnique(mock_thumbnail_manager_),
        base::WrapUnique(mock_blacklist_store_));
  }

  GURL GetCurrentlyQueriedUrl() {
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    if (!fetcher) {
      return GURL();
    }
    return fetcher->GetOriginalURL();
  }

  void RespondToFetch(const std::string& response_body,
                      net::HttpStatusCode response_code,
                      net::URLRequestStatus status) {
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher) << "Tried to respond to fetch that is not ongoing!";
    fetcher->SetResponseString(response_body);
    fetcher->set_response_code(response_code);
    fetcher->set_status(status);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void RespondToFetchWithProfile(const SuggestionsProfile& suggestions) {
    RespondToFetch(
        suggestions.SerializeAsString(), net::HTTP_OK,
        net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK));
  }

  FakeProfileOAuth2TokenService* token_service() { return &token_service_; }

  MockSyncService* sync_service() { return &mock_sync_service_; }

  MockImageManager* thumbnail_manager() { return mock_thumbnail_manager_; }

  MockBlacklistStore* blacklist_store() { return mock_blacklist_store_; }

  TestSuggestionsStore* suggestions_store() { return test_suggestions_store_; }

  SuggestionsServiceImpl* suggestions_service() {
    return suggestions_service_.get();
  }

 private:
  base::MessageLoopForIO io_message_loop_;
  TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_;
  TestSigninClient signin_client_;
  FakeSigninManagerBase signin_manager_;
  net::TestURLFetcherFactory factory_;
  FakeProfileOAuth2TokenService token_service_;
  MockSyncService mock_sync_service_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  // Owned by the SuggestionsService.
  MockImageManager* mock_thumbnail_manager_;
  MockBlacklistStore* mock_blacklist_store_;
  TestSuggestionsStore* test_suggestions_store_;

  std::unique_ptr<SuggestionsServiceImpl> suggestions_service_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsServiceTest);
};

TEST_F(SuggestionsServiceTest, FetchSuggestionsData) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  EXPECT_CALL(*thumbnail_manager(), Initialize(_));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. The data should be returned to the callback.
  suggestions_service()->FetchSuggestionsData();

  EXPECT_CALL(callback, Run(_));

  // Wait for the eventual network request.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(GetCurrentlyQueriedUrl().is_valid());
  EXPECT_EQ(GetCurrentlyQueriedUrl().path(), kSuggestionsUrlPath);
  RespondToFetchWithProfile(CreateSuggestionsProfile());

  SuggestionsProfile suggestions;
  suggestions_store()->LoadSuggestions(&suggestions);
  ASSERT_EQ(1, suggestions.suggestions_size());
  EXPECT_EQ(kTestTitle, suggestions.suggestions(0).title());
  EXPECT_EQ(kTestUrl, suggestions.suggestions(0).url());
  EXPECT_EQ(kTestFaviconUrl, suggestions.suggestions(0).favicon_url());
}

TEST_F(SuggestionsServiceTest, IgnoresNoopSyncChange) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // An no-op change should not result in a suggestions refresh.
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  // Wait for eventual (but unexpected) network requests.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->has_pending_request_for_testing());
}

TEST_F(SuggestionsServiceTest, IgnoresUninterestingSyncChange) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // An uninteresting change should not result in a network request (the
  // SyncState is INITIALIZED_ENABLED_HISTORY before and after).
  EXPECT_CALL(*sync_service(), GetActiveDataTypes())
      .Times(AnyNumber())
      .WillRepeatedly(Return(syncer::ModelTypeSet(
          syncer::HISTORY_DELETE_DIRECTIVES, syncer::BOOKMARKS)));
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  // Wait for eventual (but unexpected) network requests.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->has_pending_request_for_testing());
}

// During startup, the state changes from NOT_INITIALIZED_ENABLED to
// INITIALIZED_ENABLED_HISTORY (for a signed-in user with history sync enabled).
// This should *not* result in an automatic fetch.
TEST_F(SuggestionsServiceTest, DoesNotFetchOnStartup) {
  // The sync service starts out inactive.
  EXPECT_CALL(*sync_service(), IsSyncActive()).WillRepeatedly(Return(false));
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(suggestions_service()->has_pending_request_for_testing());

  // Sync getting enabled should not result in a fetch.
  EXPECT_CALL(*sync_service(), IsSyncActive()).WillRepeatedly(Return(true));
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  // Wait for eventual (but unexpected) network requests.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->has_pending_request_for_testing());
}

TEST_F(SuggestionsServiceTest, BuildUrlWithDefaultMinZeroParamForFewFeature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kUseSuggestionsEvenIfFewFeature);

  EXPECT_CALL(*thumbnail_manager(), Initialize(_));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. The data should be returned to the callback.
  suggestions_service()->FetchSuggestionsData();

  // Wait for the eventual network request.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(GetCurrentlyQueriedUrl().is_valid());
  EXPECT_EQ(GetCurrentlyQueriedUrl().path(), kSuggestionsUrlPath);
  std::string min_suggestions;
  EXPECT_TRUE(net::GetValueForKeyInQuery(GetCurrentlyQueriedUrl(), "min",
                                         &min_suggestions));
  EXPECT_EQ(min_suggestions, "0");
  RespondToFetchWithProfile(CreateSuggestionsProfile());
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataSyncNotInitializedEnabled) {
  EXPECT_CALL(*sync_service(), IsSyncActive()).WillRepeatedly(Return(false));
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Try to fetch suggestions. Since sync is not active, no network request
  // should be sent.
  suggestions_service()->FetchSuggestionsData();

  // Wait for eventual (but unexpected) network requests.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->has_pending_request_for_testing());

  // |suggestions_store()| should still contain the default values.
  SuggestionsProfile suggestions;
  suggestions_store()->LoadSuggestions(&suggestions);
  EXPECT_THAT(suggestions, EqualsProto(CreateSuggestionsProfile()));
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataSyncDisabled) {
  EXPECT_CALL(*sync_service(), CanSyncStart()).WillRepeatedly(Return(false));

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Tell SuggestionsService that the sync state changed. The cache should be
  // cleared and empty data returned to the callback.
  EXPECT_CALL(callback, Run(EqualsProto(SuggestionsProfile())));
  static_cast<SyncServiceObserver*>(suggestions_service())
      ->OnStateChanged(sync_service());

  // Try to fetch suggestions. Since sync is not active, no network request
  // should be sent.
  suggestions_service()->FetchSuggestionsData();

  // Wait for eventual (but unexpected) network requests.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->has_pending_request_for_testing());
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataNoAccessToken) {
  token_service()->set_auto_post_fetch_response_on_message_loop(false);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  suggestions_service()->FetchSuggestionsData();

  token_service()->IssueErrorForAllPendingRequests(GoogleServiceAuthError(
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  // Wait for eventual (but unexpected) network requests.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(suggestions_service()->has_pending_request_for_testing());
}

TEST_F(SuggestionsServiceTest, FetchingSuggestionsIgnoresRequestFailure) {
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  suggestions_service()->FetchSuggestionsData();

  // Wait for the eventual network request.
  base::RunLoop().RunUntilIdle();
  RespondToFetch("irrelevant", net::HTTP_OK,
                 net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                       net::ERR_INVALID_RESPONSE));
}

TEST_F(SuggestionsServiceTest, FetchingSuggestionsClearsStoreIfResponseNotOK) {
  suggestions_store()->StoreSuggestions(CreateSuggestionsProfile());

  // Expect that an upload to the blacklist is scheduled.
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. Empty data will be returned to the callback.
  suggestions_service()->FetchSuggestionsData();

  // Wait for the eventual network request.
  base::RunLoop().RunUntilIdle();
  RespondToFetch(
      "irrelevant", net::HTTP_BAD_REQUEST,
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK));

  SuggestionsProfile empty_suggestions;
  EXPECT_FALSE(suggestions_store()->LoadSuggestions(&empty_suggestions));
}

TEST_F(SuggestionsServiceTest, BlacklistURL) {
  // Calling RunUntilIdle on the RunLoop only works when the task is not posted
  // for the future.
  const base::TimeDelta no_delay = base::TimeDelta::FromSeconds(0);
  suggestions_service()->set_blacklist_delay_for_testing(no_delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  EXPECT_CALL(*thumbnail_manager(), Initialize(_)).Times(2);
  EXPECT_CALL(*blacklist_store(), BlacklistUrl(Eq(GURL(kBlacklistedUrl))))
      .WillOnce(Return(true));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_)).Times(2);
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*blacklist_store(), GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(GURL(kBlacklistedUrl)), Return(true)));
  EXPECT_CALL(*blacklist_store(), RemoveUrl(Eq(GURL(kBlacklistedUrl))))
      .WillOnce(Return(true));

  EXPECT_CALL(callback, Run(_)).Times(2);

  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));

  // Wait on the upload task, the blacklist request and the next blacklist
  // scheduling task. This only works when the scheduling task is not for future
  // execution (note how both the SuggestionsService's scheduling delay and the
  // BlacklistStore's candidacy delay are zero).
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GetCurrentlyQueriedUrl().path(), kBlacklistUrlPath);
  RespondToFetchWithProfile(CreateSuggestionsProfile());

  SuggestionsProfile suggestions;
  suggestions_store()->LoadSuggestions(&suggestions);
  ASSERT_EQ(1, suggestions.suggestions_size());
  EXPECT_EQ(kTestTitle, suggestions.suggestions(0).title());
  EXPECT_EQ(kTestUrl, suggestions.suggestions(0).url());
  EXPECT_EQ(kTestFaviconUrl, suggestions.suggestions(0).favicon_url());
}

TEST_F(SuggestionsServiceTest, BlacklistURLFails) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  EXPECT_CALL(*blacklist_store(), BlacklistUrl(Eq(GURL(kBlacklistedUrl))))
      .WillOnce(Return(false));

  EXPECT_FALSE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));
}

TEST_F(SuggestionsServiceTest, RetryBlacklistURLRequestAfterFailure) {
  // Calling RunUntilIdle on the RunLoop only works when the task is not
  // posted for the future.
  const base::TimeDelta no_delay = base::TimeDelta::FromSeconds(0);
  suggestions_service()->set_blacklist_delay_for_testing(no_delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Set expectations for first, failing request.
  EXPECT_CALL(*thumbnail_manager(), Initialize(_));
  EXPECT_CALL(*blacklist_store(), BlacklistUrl(Eq(GURL(kBlacklistedUrl))))
      .WillOnce(Return(true));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)));
  EXPECT_CALL(*blacklist_store(), GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(GURL(kBlacklistedUrl)), Return(true)));

  EXPECT_CALL(callback, Run(_)).Times(2);

  // Blacklist call, first request attempt.
  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));

  // Wait for the first scheduling receiving a failing response.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(GetCurrentlyQueriedUrl().is_valid());
  EXPECT_EQ(GetCurrentlyQueriedUrl().path(), kBlacklistUrlPath);
  RespondToFetch("irrelevant", net::HTTP_OK,
                 net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                       net::ERR_INVALID_RESPONSE));

  // Assert that the failure was processed as expected.
  Mock::VerifyAndClearExpectations(thumbnail_manager());
  Mock::VerifyAndClearExpectations(blacklist_store());

  // Now expect the retried request to succeed.
  EXPECT_CALL(*thumbnail_manager(), Initialize(_));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));
  EXPECT_CALL(*blacklist_store(), GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(GURL(kBlacklistedUrl)), Return(true)));
  EXPECT_CALL(*blacklist_store(), RemoveUrl(Eq(GURL(kBlacklistedUrl))))
      .WillOnce(Return(true));

  // Wait for the second scheduling followed by a successful response.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(GetCurrentlyQueriedUrl().is_valid());
  EXPECT_EQ(GetCurrentlyQueriedUrl().path(), kBlacklistUrlPath);
  RespondToFetchWithProfile(CreateSuggestionsProfile());

  SuggestionsProfile suggestions;
  suggestions_store()->LoadSuggestions(&suggestions);
  ASSERT_EQ(1, suggestions.suggestions_size());
  EXPECT_EQ(kTestTitle, suggestions.suggestions(0).title());
  EXPECT_EQ(kTestUrl, suggestions.suggestions(0).url());
  EXPECT_EQ(kTestFaviconUrl, suggestions.suggestions(0).favicon_url());
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURL) {
  // Ensure scheduling the request doesn't happen before undo.
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service()->set_blacklist_delay_for_testing(delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Blacklist expectations.
  EXPECT_CALL(*blacklist_store(), BlacklistUrl(Eq(GURL(kBlacklistedUrl))))
      .WillOnce(Return(true));
  EXPECT_CALL(*thumbnail_manager(),
              Initialize(EqualsProto(CreateSuggestionsProfile())))
      .Times(AnyNumber());
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_)).Times(AnyNumber());
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  // Undo expectations.
  EXPECT_CALL(*blacklist_store(),
              GetTimeUntilURLReadyForUpload(Eq(GURL(kBlacklistedUrl)), _))
      .WillOnce(DoAll(SetArgPointee<1>(delay), Return(true)));
  EXPECT_CALL(*blacklist_store(), RemoveUrl(Eq(GURL(kBlacklistedUrl))))
      .WillOnce(Return(true));

  EXPECT_CALL(callback, Run(_)).Times(2);
  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));
  EXPECT_TRUE(suggestions_service()->UndoBlacklistURL(GURL(kBlacklistedUrl)));
}

TEST_F(SuggestionsServiceTest, ClearBlacklist) {
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service()->set_blacklist_delay_for_testing(delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Blacklist expectations.
  EXPECT_CALL(*blacklist_store(), BlacklistUrl(Eq(GURL(kBlacklistedUrl))))
      .WillOnce(Return(true));
  EXPECT_CALL(*thumbnail_manager(),
              Initialize(EqualsProto(CreateSuggestionsProfile())))
      .Times(AnyNumber());
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_)).Times(AnyNumber());
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  EXPECT_CALL(*blacklist_store(), ClearBlacklist());

  EXPECT_CALL(callback, Run(_)).Times(2);
  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));
  suggestions_service()->ClearBlacklist();

  // Wait for the eventual network request.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetCurrentlyQueriedUrl().path(), kBlacklistClearUrlPath);
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURLFailsIfNotInBlacklist) {
  // Ensure scheduling the request doesn't happen before undo.
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service()->set_blacklist_delay_for_testing(delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Blacklist expectations.
  EXPECT_CALL(*blacklist_store(), BlacklistUrl(Eq(GURL(kBlacklistedUrl))))
      .WillOnce(Return(true));
  EXPECT_CALL(*thumbnail_manager(),
              Initialize(EqualsProto(CreateSuggestionsProfile())));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  // Undo expectations.
  // URL is not in local blacklist.
  EXPECT_CALL(*blacklist_store(),
              GetTimeUntilURLReadyForUpload(Eq(GURL(kBlacklistedUrl)), _))
      .WillOnce(Return(false));

  EXPECT_CALL(callback, Run(_));

  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));
  EXPECT_FALSE(suggestions_service()->UndoBlacklistURL(GURL(kBlacklistedUrl)));
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURLFailsIfAlreadyCandidate) {
  // Ensure scheduling the request doesn't happen before undo.
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service()->set_blacklist_delay_for_testing(delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service()->AddCallback(callback.Get());

  // Blacklist expectations.
  EXPECT_CALL(*blacklist_store(), BlacklistUrl(Eq(GURL(kBlacklistedUrl))))
      .WillOnce(Return(true));
  EXPECT_CALL(*thumbnail_manager(),
              Initialize(EqualsProto(CreateSuggestionsProfile())));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));

  // URL is not yet candidate for upload.
  const base::TimeDelta negative_delay = base::TimeDelta::FromHours(-1);
  EXPECT_CALL(*blacklist_store(),
              GetTimeUntilURLReadyForUpload(Eq(GURL(kBlacklistedUrl)), _))
      .WillOnce(DoAll(SetArgPointee<1>(negative_delay), Return(true)));

  EXPECT_CALL(callback, Run(_));

  EXPECT_TRUE(suggestions_service()->BlacklistURL(GURL(kBlacklistedUrl)));
  EXPECT_FALSE(suggestions_service()->UndoBlacklistURL(GURL(kBlacklistedUrl)));
}

TEST_F(SuggestionsServiceTest, TemporarilyIncreasesBlacklistDelayOnFailure) {
  EXPECT_CALL(*thumbnail_manager(), Initialize(_)).Times(AnyNumber());
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_)).Times(AnyNumber());
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .Times(AnyNumber())
      .WillRepeatedly(Return(false));
  const base::TimeDelta initial_delay =
      suggestions_service()->blacklist_delay_for_testing();

  // Delay unchanged on success.
  suggestions_service()->FetchSuggestionsData();
  base::RunLoop().RunUntilIdle();
  RespondToFetchWithProfile(CreateSuggestionsProfile());
  EXPECT_EQ(initial_delay,
            suggestions_service()->blacklist_delay_for_testing());

  // Delay increases on failure.
  suggestions_service()->FetchSuggestionsData();
  base::RunLoop().RunUntilIdle();
  RespondToFetch(
      "irrelevant", net::HTTP_BAD_REQUEST,
      net::URLRequestStatus(net::URLRequestStatus::SUCCESS, net::OK));
  EXPECT_GT(suggestions_service()->blacklist_delay_for_testing(),
            initial_delay);

  // Delay resets on success.
  suggestions_service()->FetchSuggestionsData();
  base::RunLoop().RunUntilIdle();
  RespondToFetchWithProfile(CreateSuggestionsProfile());
  EXPECT_EQ(initial_delay,
            suggestions_service()->blacklist_delay_for_testing());
}

TEST_F(SuggestionsServiceTest, DoesNotOverrideDefaultExpiryTime) {
  EXPECT_CALL(*thumbnail_manager(), Initialize(_));
  EXPECT_CALL(*blacklist_store(), FilterSuggestions(_));
  EXPECT_CALL(*blacklist_store(), GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  suggestions_service()->FetchSuggestionsData();

  base::RunLoop().RunUntilIdle();
  // Creates one suggestion without timestamp and adds a second with timestamp.
  SuggestionsProfile profile = CreateSuggestionsProfile();
  ChromeSuggestion* suggestion = profile.add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);
  suggestion->set_expiry_ts(kTestSetExpiry);
  RespondToFetchWithProfile(profile);

  SuggestionsProfile suggestions;
  suggestions_store()->LoadSuggestions(&suggestions);
  ASSERT_EQ(2, suggestions.suggestions_size());
  // Suggestion[0] had no time stamp and should be ahead of the old suggestion.
  EXPECT_LT(kTestSetExpiry, suggestions.suggestions(0).expiry_ts());
  // Suggestion[1] had a very old time stamp but should not be updated.
  EXPECT_EQ(kTestSetExpiry, suggestions.suggestions(1).expiry_ts());
}

TEST_F(SuggestionsServiceTest, GetPageThumbnail) {
  const GURL test_url(kTestUrl);
  const GURL thumbnail_url("https://www.thumbnails.com/thumb.jpg");
  base::Callback<void(const GURL&, const gfx::Image&)> dummy_callback;

  EXPECT_CALL(*thumbnail_manager(), GetImageForURL(test_url, _));
  suggestions_service()->GetPageThumbnail(test_url, dummy_callback);

  EXPECT_CALL(*thumbnail_manager(), AddImageURL(test_url, thumbnail_url));
  EXPECT_CALL(*thumbnail_manager(), GetImageForURL(test_url, _));
  suggestions_service()->GetPageThumbnailWithURL(test_url, thumbnail_url,
                                                 dummy_callback);
}

}  // namespace suggestions
