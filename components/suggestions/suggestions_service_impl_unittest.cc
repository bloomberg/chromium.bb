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
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_store.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

using sync_preferences::TestingPrefServiceSyncable;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::Eq;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace {

const char kAccountId[] = "account";
const char kTestTitle[] = "a title";
const char kTestUrl[] = "http://go.com";
const char kTestFaviconUrl[] =
    "https://s2.googleusercontent.com/s2/favicons?domain_url="
    "http://go.com&alt=s&sz=32";
const char kBlacklistedUrl[] = "http://blacklist.com";
const char kBlacklistedUrlAlt[] = "http://blacklist-atl.com";
const int64_t kTestDefaultExpiry = 1402200000000000;
const int64_t kTestSetExpiry = 1404792000000000;

std::unique_ptr<net::FakeURLFetcher> CreateURLFetcher(
    const GURL& url,
    net::URLFetcherDelegate* delegate,
    const std::string& response_data,
    net::HttpStatusCode response_code,
    net::URLRequestStatus::Status status) {
  std::unique_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
      url, delegate, response_data, response_code, status));

  if (response_code == net::HTTP_OK) {
    scoped_refptr<net::HttpResponseHeaders> download_headers(
        new net::HttpResponseHeaders(""));
    download_headers->AddHeader("Content-Type: text/html");
    fetcher->set_response_headers(download_headers);
  }
  return fetcher;
}

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
  suggestion->set_expiry_ts(kTestSetExpiry);
  return profile;
}

// Creates one suggestion with expiry timestamp and one without.
SuggestionsProfile CreateSuggestionsProfileWithExpiryTimestamps() {
  SuggestionsProfile profile;
  profile.set_timestamp(123);
  ChromeSuggestion* suggestion = profile.add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);
  suggestion->set_expiry_ts(kTestSetExpiry);

  suggestion = profile.add_suggestions();
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
        factory_(nullptr, base::Bind(&CreateURLFetcher)),
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
    request_context_ =
        new net::TestURLRequestContextGetter(io_message_loop_.task_runner());

    EXPECT_CALL(mock_sync_service_, CanSyncStart())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_sync_service_, IsSyncActive())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_sync_service_, ConfigurationDone())
        .Times(AnyNumber())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_sync_service_, GetActiveDataTypes())
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

  bool HasPendingSuggestionsRequest() const {
    return !!suggestions_service_->pending_request_.get();
  }

 protected:
  base::MessageLoopForIO io_message_loop_;
  TestingPrefServiceSyncable pref_service_;
  AccountTrackerService account_tracker_;
  TestSigninClient signin_client_;
  FakeSigninManagerBase signin_manager_;
  net::FakeURLFetcherFactory factory_;
  FakeProfileOAuth2TokenService token_service_;
  MockSyncService mock_sync_service_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  // Owned by the SuggestionsService.
  MockImageManager* mock_thumbnail_manager_;
  MockBlacklistStore* mock_blacklist_store_;
  TestSuggestionsStore* test_suggestions_store_;

  std::unique_ptr<SuggestionsServiceImpl> suggestions_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SuggestionsServiceTest);
};

TEST_F(SuggestionsServiceTest, FetchSuggestionsData) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  // Set up net::FakeURLFetcherFactory.
  factory_.SetFakeResponse(SuggestionsServiceImpl::BuildSuggestionsURL(),
                           CreateSuggestionsProfile().SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  EXPECT_CALL(*mock_thumbnail_manager_, Initialize(_));
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_));
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. The data should be returned to the callback.
  suggestions_service_->FetchSuggestionsData();

  EXPECT_CALL(callback, Run(_));

  // Let the network request run.
  base::RunLoop().RunUntilIdle();

  SuggestionsProfile suggestions;
  test_suggestions_store_->LoadSuggestions(&suggestions);
  ASSERT_EQ(1, suggestions.suggestions_size());
  EXPECT_EQ(kTestTitle, suggestions.suggestions(0).title());
  EXPECT_EQ(kTestUrl, suggestions.suggestions(0).url());
  EXPECT_EQ(kTestFaviconUrl, suggestions.suggestions(0).favicon_url());
}

TEST_F(SuggestionsServiceTest, IgnoresNoopSyncChange) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  factory_.SetFakeResponse(SuggestionsServiceImpl::BuildSuggestionsURL(),
                           CreateSuggestionsProfile().SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // An no-op change should not result in a suggestions refresh.
  suggestions_service_->OnStateChanged(&mock_sync_service_);

  // Let any network request run (there shouldn't be one).
  base::RunLoop().RunUntilIdle();
}

TEST_F(SuggestionsServiceTest, IgnoresUninterestingSyncChange) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  factory_.SetFakeResponse(SuggestionsServiceImpl::BuildSuggestionsURL(),
                           CreateSuggestionsProfile().SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // An uninteresting change should not result in a network request (the
  // SyncState is INITIALIZED_ENABLED_HISTORY before and after).
  EXPECT_CALL(mock_sync_service_, GetActiveDataTypes())
      .Times(AnyNumber())
      .WillRepeatedly(Return(syncer::ModelTypeSet(
          syncer::HISTORY_DELETE_DIRECTIVES, syncer::BOOKMARKS)));
  suggestions_service_->OnStateChanged(&mock_sync_service_);

  // Let any network request run (there shouldn't be one).
  base::RunLoop().RunUntilIdle();
}

// During startup, the state changes from NOT_INITIALIZED_ENABLED to
// INITIALIZED_ENABLED_HISTORY (for a signed-in user with history sync enabled).
// This should *not* result in an automatic fetch.
TEST_F(SuggestionsServiceTest, DoesNotFetchOnStartup) {
  // The sync service starts out inactive.
  EXPECT_CALL(mock_sync_service_, IsSyncActive()).WillRepeatedly(Return(false));
  suggestions_service_->OnStateChanged(&mock_sync_service_);

  ASSERT_EQ(SuggestionsServiceImpl::NOT_INITIALIZED_ENABLED,
            suggestions_service_->ComputeSyncState());

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  factory_.SetFakeResponse(SuggestionsServiceImpl::BuildSuggestionsURL(),
                           CreateSuggestionsProfile().SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // Sync getting enabled should not result in a fetch.
  EXPECT_CALL(mock_sync_service_, IsSyncActive()).WillRepeatedly(Return(true));
  suggestions_service_->OnStateChanged(&mock_sync_service_);

  ASSERT_EQ(SuggestionsServiceImpl::INITIALIZED_ENABLED_HISTORY,
            suggestions_service_->ComputeSyncState());

  // Let any network request run (there shouldn't be one).
  base::RunLoop().RunUntilIdle();
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataSyncNotInitializedEnabled) {
  EXPECT_CALL(mock_sync_service_, IsSyncActive()).WillRepeatedly(Return(false));
  suggestions_service_->OnStateChanged(&mock_sync_service_);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  // Try to fetch suggestions. Since sync is not active, no network request
  // should be sent.
  suggestions_service_->FetchSuggestionsData();

  // Let any network request run (there shouldn't be one).
  base::RunLoop().RunUntilIdle();

  // |test_suggestions_store_| should still contain the default values.
  SuggestionsProfile suggestions;
  test_suggestions_store_->LoadSuggestions(&suggestions);
  EXPECT_THAT(suggestions, EqualsProto(CreateSuggestionsProfile()));
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataSyncDisabled) {
  EXPECT_CALL(mock_sync_service_, CanSyncStart()).WillRepeatedly(Return(false));

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  // Tell SuggestionsService that the sync state changed. The cache should be
  // cleared and empty data returned to the callback.
  EXPECT_CALL(callback, Run(EqualsProto(SuggestionsProfile())));
  suggestions_service_->OnStateChanged(&mock_sync_service_);

  // Try to fetch suggestions. Since sync is not active, no network request
  // should be sent.
  suggestions_service_->FetchSuggestionsData();

  // Let any network request run.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataNoAccessToken) {
  token_service_.set_auto_post_fetch_response_on_message_loop(false);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  suggestions_service_->FetchSuggestionsData();

  token_service_.IssueErrorForAllPendingRequests(GoogleServiceAuthError(
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  // No network request should be sent.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(HasPendingSuggestionsRequest());
}

TEST_F(SuggestionsServiceTest, IssueRequestIfNoneOngoingError) {
  // Fake a request error.
  factory_.SetFakeResponse(SuggestionsServiceImpl::BuildSuggestionsURL(),
                           "irrelevant", net::HTTP_OK,
                           net::URLRequestStatus::FAILED);

  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. Empty data will be returned to the callback.
  suggestions_service_->IssueRequestIfNoneOngoing(
      SuggestionsServiceImpl::BuildSuggestionsURL());

  // (Testing only) wait until suggestion fetch is complete.
  base::RunLoop().RunUntilIdle();
}

TEST_F(SuggestionsServiceTest, IssueRequestIfNoneOngoingResponseNotOK) {
  // Fake a non-200 response code.
  factory_.SetFakeResponse(SuggestionsServiceImpl::BuildSuggestionsURL(),
                           "irrelevant", net::HTTP_BAD_REQUEST,
                           net::URLRequestStatus::SUCCESS);

  // Expect that an upload to the blacklist is scheduled.
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. Empty data will be returned to the callback.
  suggestions_service_->IssueRequestIfNoneOngoing(
      SuggestionsServiceImpl::BuildSuggestionsURL());

  // (Testing only) wait until suggestion fetch is complete.
  base::RunLoop().RunUntilIdle();

  // Expect no suggestions in the cache.
  SuggestionsProfile empty_suggestions;
  EXPECT_FALSE(test_suggestions_store_->LoadSuggestions(&empty_suggestions));
}

TEST_F(SuggestionsServiceTest, BlacklistURL) {
  const base::TimeDelta no_delay = base::TimeDelta::FromSeconds(0);
  suggestions_service_->set_blacklist_delay(no_delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  const GURL blacklisted_url(kBlacklistedUrl);
  const GURL request_url(
      SuggestionsServiceImpl::BuildSuggestionsBlacklistURL(blacklisted_url));
  factory_.SetFakeResponse(request_url,
                           CreateSuggestionsProfile().SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(*mock_thumbnail_manager_, Initialize(_)).Times(2);

  // Expected calls to the blacklist store.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_)).Times(2);
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_blacklist_store_, GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(blacklisted_url), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, RemoveUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));

  EXPECT_CALL(callback, Run(_)).Times(2);

  EXPECT_TRUE(suggestions_service_->BlacklistURL(blacklisted_url));

  // Wait on the upload task, the blacklist request and the next blacklist
  // scheduling task. This only works when the scheduling task is not for future
  // execution (note how both the SuggestionsService's scheduling delay and the
  // BlacklistStore's candidacy delay are zero).
  base::RunLoop().RunUntilIdle();

  SuggestionsProfile suggestions;
  test_suggestions_store_->LoadSuggestions(&suggestions);
  ASSERT_EQ(1, suggestions.suggestions_size());
  EXPECT_EQ(kTestTitle, suggestions.suggestions(0).title());
  EXPECT_EQ(kTestUrl, suggestions.suggestions(0).url());
  EXPECT_EQ(kTestFaviconUrl, suggestions.suggestions(0).favicon_url());
}

TEST_F(SuggestionsServiceTest, BlacklistURLFails) {
  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(0);
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  const GURL blacklisted_url(kBlacklistedUrl);
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(false));
  EXPECT_FALSE(suggestions_service_->BlacklistURL(blacklisted_url));
}

// Initial blacklist request fails, triggering a second which succeeds.
TEST_F(SuggestionsServiceTest, BlacklistURLRequestFails) {
  const base::TimeDelta no_delay = base::TimeDelta::FromSeconds(0);
  suggestions_service_->set_blacklist_delay(no_delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  const GURL blacklisted_url(kBlacklistedUrl);
  const GURL request_url(
      SuggestionsServiceImpl::BuildSuggestionsBlacklistURL(blacklisted_url));
  const GURL blacklisted_url_alt(kBlacklistedUrlAlt);
  const GURL request_url_alt(
      SuggestionsServiceImpl::BuildSuggestionsBlacklistURL(
          blacklisted_url_alt));

  // Note: we want to set the response for the blacklist URL to first
  // succeed, then fail. This doesn't seem possible. For simplicity of testing,
  // we'll pretend the URL changed in the BlacklistStore between the first and
  // the second request, and adjust expectations accordingly.
  factory_.SetFakeResponse(request_url, "irrelevant", net::HTTP_OK,
                           net::URLRequestStatus::FAILED);
  factory_.SetFakeResponse(request_url_alt,
                           CreateSuggestionsProfile().SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // Expectations.
  EXPECT_CALL(*mock_thumbnail_manager_, Initialize(_)).Times(2);
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_)).Times(2);
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_blacklist_store_, GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(blacklisted_url), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(blacklisted_url_alt), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, RemoveUrl(Eq(blacklisted_url_alt)))
      .WillOnce(Return(true));

  EXPECT_CALL(callback, Run(_)).Times(2);

  // Blacklist call, first request attempt.
  EXPECT_TRUE(suggestions_service_->BlacklistURL(blacklisted_url));

  // Wait for the first scheduling, the first request, the second scheduling,
  // second request and the third scheduling. Again, note that calling
  // RunUntilIdle on the MessageLoop only works when the task is not posted for
  // the future.
  base::RunLoop().RunUntilIdle();

  SuggestionsProfile suggestions;
  test_suggestions_store_->LoadSuggestions(&suggestions);
  ASSERT_EQ(1, suggestions.suggestions_size());
  EXPECT_EQ(kTestTitle, suggestions.suggestions(0).title());
  EXPECT_EQ(kTestUrl, suggestions.suggestions(0).url());
  EXPECT_EQ(kTestFaviconUrl, suggestions.suggestions(0).favicon_url());
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURL) {
  // Ensure scheduling the request doesn't happen before undo.
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service_->set_blacklist_delay(delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  const GURL blacklisted_url(kBlacklistedUrl);

  // Blacklist expectations.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_thumbnail_manager_,
              Initialize(EqualsProto(CreateSuggestionsProfile())))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_)).Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  // Undo expectations.
  EXPECT_CALL(*mock_blacklist_store_,
              GetTimeUntilURLReadyForUpload(Eq(blacklisted_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(delay), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, RemoveUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));

  EXPECT_CALL(callback, Run(_)).Times(2);
  EXPECT_TRUE(suggestions_service_->BlacklistURL(blacklisted_url));
  EXPECT_TRUE(suggestions_service_->UndoBlacklistURL(blacklisted_url));
}

TEST_F(SuggestionsServiceTest, ClearBlacklist) {
  // Ensure scheduling the request doesn't happen before undo.
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service_->set_blacklist_delay(delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  const SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();
  const GURL blacklisted_url(kBlacklistedUrl);

  factory_.SetFakeResponse(
      SuggestionsServiceImpl::BuildSuggestionsBlacklistClearURL(),
      suggestions_profile.SerializeAsString(), net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  // Blacklist expectations.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_thumbnail_manager_,
              Initialize(EqualsProto(suggestions_profile)))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_)).Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, ClearBlacklist());

  EXPECT_CALL(callback, Run(_)).Times(2);
  EXPECT_TRUE(suggestions_service_->BlacklistURL(blacklisted_url));
  suggestions_service_->ClearBlacklist();
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURLFailsIfNotInBlacklist) {
  // Ensure scheduling the request doesn't happen before undo.
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service_->set_blacklist_delay(delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  const GURL blacklisted_url(kBlacklistedUrl);

  // Blacklist expectations.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_thumbnail_manager_,
              Initialize(EqualsProto(CreateSuggestionsProfile())));
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_));
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));

  // URL is not in local blacklist.
  EXPECT_CALL(*mock_blacklist_store_,
              GetTimeUntilURLReadyForUpload(Eq(blacklisted_url), _))
      .WillOnce(Return(false));

  EXPECT_CALL(callback, Run(_));
  EXPECT_TRUE(suggestions_service_->BlacklistURL(blacklisted_url));
  EXPECT_FALSE(suggestions_service_->UndoBlacklistURL(blacklisted_url));
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURLFailsIfAlreadyCandidate) {
  // Ensure scheduling the request doesn't happen before undo.
  const base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service_->set_blacklist_delay(delay);

  base::MockCallback<SuggestionsService::ResponseCallback> callback;
  auto subscription = suggestions_service_->AddCallback(callback.Get());

  const GURL blacklisted_url(kBlacklistedUrl);

  // Blacklist expectations.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklisted_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_thumbnail_manager_,
              Initialize(EqualsProto(CreateSuggestionsProfile())));
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_));
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));

  // URL is not yet candidate for upload.
  base::TimeDelta negative_delay = base::TimeDelta::FromHours(-1);
  EXPECT_CALL(*mock_blacklist_store_,
              GetTimeUntilURLReadyForUpload(Eq(blacklisted_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(negative_delay), Return(true)));

  EXPECT_CALL(callback, Run(_));
  EXPECT_TRUE(suggestions_service_->BlacklistURL(blacklisted_url));
  EXPECT_FALSE(suggestions_service_->UndoBlacklistURL(blacklisted_url));
}

TEST_F(SuggestionsServiceTest, GetBlacklistedUrlNotBlacklistRequest) {
  // Not a blacklist request.
  std::unique_ptr<net::FakeURLFetcher> fetcher(
      CreateURLFetcher(GURL("http://not-blacklisting.com/a?b=c"), nullptr, "",
                       net::HTTP_OK, net::URLRequestStatus::SUCCESS));
  GURL retrieved_url;
  EXPECT_FALSE(
      SuggestionsServiceImpl::GetBlacklistedUrl(*fetcher, &retrieved_url));
}

TEST_F(SuggestionsServiceTest, GetBlacklistedUrlBlacklistRequest) {
  // An actual blacklist request.
  const GURL blacklisted_url("http://blacklisted.com/a?b=c&d=e");
  const std::string encoded_blacklisted_url =
      "http%3A%2F%2Fblacklisted.com%2Fa%3Fb%3Dc%26d%3De";
  const std::string blacklist_request_prefix(
      SuggestionsServiceImpl::BuildSuggestionsBlacklistURLPrefix());
  std::unique_ptr<net::FakeURLFetcher> fetcher(CreateURLFetcher(
      GURL(blacklist_request_prefix + encoded_blacklisted_url), nullptr, "",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS));
  GURL retrieved_url;
  EXPECT_TRUE(
      SuggestionsServiceImpl::GetBlacklistedUrl(*fetcher, &retrieved_url));
  EXPECT_EQ(blacklisted_url, retrieved_url);
}

TEST_F(SuggestionsServiceTest, UpdateBlacklistDelay) {
  const base::TimeDelta initial_delay = suggestions_service_->blacklist_delay();

  // Delay unchanged on success.
  suggestions_service_->UpdateBlacklistDelay(true);
  EXPECT_EQ(initial_delay, suggestions_service_->blacklist_delay());

  // Delay increases on failure.
  suggestions_service_->UpdateBlacklistDelay(false);
  EXPECT_GT(suggestions_service_->blacklist_delay(), initial_delay);

  // Delay resets on success.
  suggestions_service_->UpdateBlacklistDelay(true);
  EXPECT_EQ(initial_delay, suggestions_service_->blacklist_delay());
}

TEST_F(SuggestionsServiceTest, CheckDefaultTimeStamps) {
  SuggestionsProfile suggestions =
      CreateSuggestionsProfileWithExpiryTimestamps();
  suggestions_service_->SetDefaultExpiryTimestamp(&suggestions,
                                                  kTestDefaultExpiry);
  EXPECT_EQ(kTestSetExpiry, suggestions.suggestions(0).expiry_ts());
  EXPECT_EQ(kTestDefaultExpiry, suggestions.suggestions(1).expiry_ts());
}

TEST_F(SuggestionsServiceTest, GetPageThumbnail) {
  const GURL test_url(kTestUrl);
  const GURL thumbnail_url("https://www.thumbnails.com/thumb.jpg");
  base::Callback<void(const GURL&, const gfx::Image&)> dummy_callback;

  EXPECT_CALL(*mock_thumbnail_manager_, GetImageForURL(test_url, _));
  suggestions_service_->GetPageThumbnail(test_url, dummy_callback);

  EXPECT_CALL(*mock_thumbnail_manager_, AddImageURL(test_url, thumbnail_url));
  EXPECT_CALL(*mock_thumbnail_manager_, GetImageForURL(test_url, _));
  suggestions_service_->GetPageThumbnailWithURL(test_url, thumbnail_url,
                                                dummy_callback);
}

}  // namespace suggestions
