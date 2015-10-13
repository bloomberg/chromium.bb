// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_service.h"

#include <map>
#include <sstream>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_store.h"
#include "components/suggestions/suggestions_utils.h"
#include "net/base/escape.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;
using testing::DoAll;
using ::testing::AnyNumber;
using ::testing::Eq;
using ::testing::Return;
using testing::SetArgPointee;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::_;

namespace {

const char kTestTitle[] = "a title";
const char kTestUrl[] = "http://go.com";
const char kTestFaviconUrl[] =
    "https://s2.googleusercontent.com/s2/favicons?domain_url="
    "http://go.com&alt=s&sz=32";
const char kTestImpressionUrl[] =
    "https://www.google.com/chromesuggestions/click?q=123&cd=-1";
const char kTestClickUrl[] =
    "https://www.google.com/chromesuggestions/click?q=123&cd=0";
const char kBlacklistUrl[] = "http://blacklist.com";
const char kBlacklistUrlAlt[] = "http://blacklist-atl.com";
const int64 kTestDefaultExpiry = 1402200000000000;
const int64 kTestSetExpiry = 1404792000000000;

scoped_ptr<net::FakeURLFetcher> CreateURLFetcher(
    const GURL& url, net::URLFetcherDelegate* delegate,
    const std::string& response_data, net::HttpStatusCode response_code,
    net::URLRequestStatus::Status status) {
  scoped_ptr<net::FakeURLFetcher> fetcher(new net::FakeURLFetcher(
      url, delegate, response_data, response_code, status));

  if (response_code == net::HTTP_OK) {
    scoped_refptr<net::HttpResponseHeaders> download_headers(
        new net::HttpResponseHeaders(""));
    download_headers->AddHeader("Content-Type: text/html");
    fetcher->set_response_headers(download_headers);
  }
  return fetcher.Pass();
}

std::string GetExpectedBlacklistRequestUrl(const GURL& blacklist_url) {
  std::stringstream request_url;
  request_url << suggestions::kSuggestionsBlacklistURLPrefix
              << net::EscapeQueryParamValue(blacklist_url.spec(), true);
  return request_url.str();
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

class TestSuggestionsStore : public suggestions::SuggestionsStore {
 public:
  TestSuggestionsStore() {
    cached_suggestions = CreateSuggestionsProfile();
  }
  bool LoadSuggestions(SuggestionsProfile* suggestions) override {
    suggestions->CopyFrom(cached_suggestions);
    return cached_suggestions.suggestions_size();
  }
  bool StoreSuggestions(const SuggestionsProfile& suggestions)
      override {
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
                    base::Callback<void(const GURL&, const SkBitmap*)>));
  MOCK_METHOD2(AddImageURL, void(const GURL&, const GURL&));
};

class MockBlacklistStore : public suggestions::BlacklistStore {
 public:
  MOCK_METHOD1(BlacklistUrl, bool(const GURL&));
  MOCK_METHOD0(IsEmpty, bool());
  MOCK_METHOD1(GetTimeUntilReadyForUpload, bool(base::TimeDelta*));
  MOCK_METHOD2(GetTimeUntilURLReadyForUpload,
               bool(const GURL&, base::TimeDelta*));
  MOCK_METHOD1(GetCandidateForUpload, bool(GURL*));
  MOCK_METHOD1(RemoveUrl, bool(const GURL&));
  MOCK_METHOD1(FilterSuggestions, void(SuggestionsProfile*));
  MOCK_METHOD0(ClearBlacklist, void());
};

class SuggestionsServiceTest : public testing::Test {
 public:
  void CheckCallback(const SuggestionsProfile& suggestions_profile) {
    ++suggestions_data_callback_count_;
  }

  void CheckSuggestionsData() {
    SuggestionsProfile suggestions_profile;
    test_suggestions_store_->LoadSuggestions(&suggestions_profile);
    EXPECT_EQ(1, suggestions_profile.suggestions_size());
    EXPECT_EQ(kTestTitle, suggestions_profile.suggestions(0).title());
    EXPECT_EQ(kTestUrl, suggestions_profile.suggestions(0).url());
    EXPECT_EQ(kTestFaviconUrl,
              suggestions_profile.suggestions(0).favicon_url());
    EXPECT_EQ(kTestImpressionUrl,
              suggestions_profile.suggestions(0).impression_url());
    EXPECT_EQ(kTestClickUrl, suggestions_profile.suggestions(0).click_url());
  }

  void SetBlacklistFailure() {
    blacklisting_failed_ = true;
  }

  void SetUndoBlacklistFailure() {
    undo_blacklisting_failed_ = true;
  }

  void ExpectEmptySuggestionsProfile(const SuggestionsProfile& profile) {
    EXPECT_EQ(0, profile.suggestions_size());
    ++suggestions_empty_data_count_;
  }

  int suggestions_data_callback_count_;
  int suggestions_empty_data_count_;
  bool blacklisting_failed_;
  bool undo_blacklisting_failed_;

 protected:
  SuggestionsServiceTest()
      : suggestions_data_callback_count_(0),
        suggestions_empty_data_count_(0),
        blacklisting_failed_(false),
        undo_blacklisting_failed_(false),
        factory_(NULL, base::Bind(&CreateURLFetcher)),
        mock_thumbnail_manager_(NULL),
        mock_blacklist_store_(NULL),
        test_suggestions_store_(NULL) {}

  ~SuggestionsServiceTest() override {}

  void SetUp() override {
    request_context_ =
        new net::TestURLRequestContextGetter(io_message_loop_.task_runner());
  }

  void FetchSuggestionsDataHelper(SyncState sync_state) {
    scoped_ptr<SuggestionsService> suggestions_service(
        CreateSuggestionsServiceWithMocks());
    EXPECT_TRUE(suggestions_service != NULL);

    SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();

    // Set up net::FakeURLFetcherFactory.
    factory_.SetFakeResponse(GURL(kSuggestionsURL),
                             suggestions_profile.SerializeAsString(),
                             net::HTTP_OK, net::URLRequestStatus::SUCCESS);

    // Expectations.
    EXPECT_CALL(*mock_thumbnail_manager_,
                Initialize(EqualsProto(suggestions_profile)));
    EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_));
    EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
        .WillOnce(Return(false));

    // Send the request. The data will be returned to the callback.
    suggestions_service->FetchSuggestionsData(
        sync_state, base::Bind(&SuggestionsServiceTest::CheckCallback,
                               base::Unretained(this)));

    // Ensure that CheckSuggestionsData() ran once.
    EXPECT_EQ(1, suggestions_data_callback_count_);

    // Let the network request run.
    io_message_loop_.RunUntilIdle();

    CheckSuggestionsData();
  }

  SuggestionsService* CreateSuggestionsServiceWithMocks() {
    // These objects are owned by the returned SuggestionsService, but we keep
    // the pointer around for testing.
    test_suggestions_store_ = new TestSuggestionsStore();
    mock_thumbnail_manager_ = new StrictMock<MockImageManager>();
    mock_blacklist_store_ = new StrictMock<MockBlacklistStore>();
    return new SuggestionsService(
        request_context_.get(),
        scoped_ptr<SuggestionsStore>(test_suggestions_store_),
        scoped_ptr<ImageManager>(mock_thumbnail_manager_),
        scoped_ptr<BlacklistStore>(mock_blacklist_store_));
  }

  void Blacklist(SuggestionsService* suggestions_service, GURL url) {
    suggestions_service->BlacklistURL(
        url, base::Bind(&SuggestionsServiceTest::CheckCallback,
                        base::Unretained(this)),
        base::Bind(&SuggestionsServiceTest::SetBlacklistFailure,
                   base::Unretained(this)));
  }

  void UndoBlacklist(SuggestionsService* suggestions_service, GURL url) {
    suggestions_service->UndoBlacklistURL(
        url, base::Bind(&SuggestionsServiceTest::CheckCallback,
                        base::Unretained(this)),
        base::Bind(&SuggestionsServiceTest::SetUndoBlacklistFailure,
                   base::Unretained(this)));
  }

  // Helper for Undo failure tests. Depending on |is_uploaded|, tests either
  // the case where the URL is no longer in the local blacklist or the case
  // in which it's not yet candidate for upload.
  void UndoBlacklistURLFailsHelper(bool is_uploaded) {
    scoped_ptr<SuggestionsService> suggestions_service(
        CreateSuggestionsServiceWithMocks());
    EXPECT_TRUE(suggestions_service != NULL);
    // Ensure scheduling the request doesn't happen before undo.
    base::TimeDelta delay = base::TimeDelta::FromHours(1);
    suggestions_service->set_blacklist_delay(delay);
    SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();
    GURL blacklist_url(kBlacklistUrl);

    // Blacklist expectations.
    EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklist_url)))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_thumbnail_manager_,
                Initialize(EqualsProto(suggestions_profile)));
    EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_));
    EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
        .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
    // Undo expectations.
    if (is_uploaded) {
      // URL is not in local blacklist.
      EXPECT_CALL(*mock_blacklist_store_,
                  GetTimeUntilURLReadyForUpload(Eq(blacklist_url), _))
          .WillOnce(Return(false));
    } else {
      // URL is not yet candidate for upload.
      base::TimeDelta negative_delay = base::TimeDelta::FromHours(-1);
      EXPECT_CALL(*mock_blacklist_store_,
                  GetTimeUntilURLReadyForUpload(Eq(blacklist_url), _))
          .WillOnce(DoAll(SetArgPointee<1>(negative_delay), Return(true)));
    }

    Blacklist(suggestions_service.get(), blacklist_url);
    UndoBlacklist(suggestions_service.get(), blacklist_url);

    EXPECT_EQ(1, suggestions_data_callback_count_);
    EXPECT_FALSE(blacklisting_failed_);
    EXPECT_TRUE(undo_blacklisting_failed_);
  }

 protected:
  base::MessageLoopForIO io_message_loop_;
  net::FakeURLFetcherFactory factory_;
  // Only used if the SuggestionsService is built with mocks. Not owned.
  MockImageManager* mock_thumbnail_manager_;
  MockBlacklistStore* mock_blacklist_store_;
  TestSuggestionsStore* test_suggestions_store_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SuggestionsServiceTest);
};

TEST_F(SuggestionsServiceTest, FetchSuggestionsData) {
  FetchSuggestionsDataHelper(INITIALIZED_ENABLED_HISTORY);
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataSyncNotInitializedEnabled) {
  FetchSuggestionsDataHelper(NOT_INITIALIZED_ENABLED);
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataSyncDisabled) {
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);

  // Send the request. Cache is cleared and empty data will be returned to the
  // callback.
  suggestions_service->FetchSuggestionsData(
      SYNC_OR_HISTORY_SYNC_DISABLED,
      base::Bind(&SuggestionsServiceTest::ExpectEmptySuggestionsProfile,
                 base::Unretained(this)));

  // Ensure that ExpectEmptySuggestionsProfile ran once.
  EXPECT_EQ(1, suggestions_empty_data_count_);
}

TEST_F(SuggestionsServiceTest, IssueRequestIfNoneOngoingError) {
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);

  // Fake a request error.
  factory_.SetFakeResponse(GURL(kSuggestionsURL), "irrelevant", net::HTTP_OK,
                           net::URLRequestStatus::FAILED);

  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. Empty data will be returned to the callback.
  suggestions_service->IssueRequestIfNoneOngoing(GURL(kSuggestionsURL));

  // (Testing only) wait until suggestion fetch is complete.
  io_message_loop_.RunUntilIdle();
}

TEST_F(SuggestionsServiceTest, IssueRequestIfNoneOngoingResponseNotOK) {
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);

  // Fake a non-200 response code.
  factory_.SetFakeResponse(GURL(kSuggestionsURL), "irrelevant",
                           net::HTTP_BAD_REQUEST,
                           net::URLRequestStatus::SUCCESS);

  // Expect that an upload to the blacklist is scheduled.
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(Return(false));

  // Send the request. Empty data will be returned to the callback.
  suggestions_service->IssueRequestIfNoneOngoing(GURL(kSuggestionsURL));

  // (Testing only) wait until suggestion fetch is complete.
  io_message_loop_.RunUntilIdle();

  // Expect no suggestions in the cache.
  SuggestionsProfile empty_suggestions;
  EXPECT_FALSE(test_suggestions_store_->LoadSuggestions(&empty_suggestions));
}

TEST_F(SuggestionsServiceTest, BlacklistURL) {
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);
  base::TimeDelta no_delay = base::TimeDelta::FromSeconds(0);
  suggestions_service->set_blacklist_delay(no_delay);

  GURL blacklist_url(kBlacklistUrl);
  std::string request_url = GetExpectedBlacklistRequestUrl(blacklist_url);
  SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();
  factory_.SetFakeResponse(GURL(request_url),
                           suggestions_profile.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(*mock_thumbnail_manager_,
              Initialize(EqualsProto(suggestions_profile)));

  // Expected calls to the blacklist store.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklist_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_));
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_blacklist_store_, GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(blacklist_url), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, RemoveUrl(Eq(blacklist_url)))
      .WillOnce(Return(true));

  Blacklist(suggestions_service.get(), blacklist_url);

  // Wait on the upload task. This only works when the scheduling task is not
  // for future execution (note how both the SuggestionsService's scheduling
  // delay and the BlacklistStore's candidacy delay are zero). Then wait on
  // the blacklist request, then again on the next blacklist scheduling task.
  base::MessageLoop::current()->RunUntilIdle();
  io_message_loop_.RunUntilIdle();
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(1, suggestions_data_callback_count_);
  EXPECT_FALSE(blacklisting_failed_);
  CheckSuggestionsData();
}

TEST_F(SuggestionsServiceTest, BlacklistURLFails) {
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);
  GURL blacklist_url(kBlacklistUrl);
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklist_url)))
      .WillOnce(Return(false));

  Blacklist(suggestions_service.get(), blacklist_url);

  EXPECT_TRUE(blacklisting_failed_);
  EXPECT_EQ(0, suggestions_data_callback_count_);
}

// Initial blacklist request fails, triggering a second which succeeds.
TEST_F(SuggestionsServiceTest, BlacklistURLRequestFails) {
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);
  base::TimeDelta no_delay = base::TimeDelta::FromSeconds(0);
  suggestions_service->set_blacklist_delay(no_delay);

  GURL blacklist_url(kBlacklistUrl);
  std::string request_url = GetExpectedBlacklistRequestUrl(blacklist_url);
  GURL blacklist_url_alt(kBlacklistUrlAlt);
  std::string request_url_alt = GetExpectedBlacklistRequestUrl(
    blacklist_url_alt);
  SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();

  // Note: we want to set the response for the blacklist URL to first
  // succeed, then fail. This doesn't seem possible. For simplicity of testing,
  // we'll pretend the URL changed in the BlacklistStore between the first and
  // the second request, and adjust expectations accordingly.
  factory_.SetFakeResponse(GURL(request_url), "irrelevant", net::HTTP_OK,
                           net::URLRequestStatus::FAILED);
  factory_.SetFakeResponse(GURL(request_url_alt),
                           suggestions_profile.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // Expectations.
  EXPECT_CALL(*mock_thumbnail_manager_,
              Initialize(EqualsProto(suggestions_profile)));
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklist_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_));
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(no_delay), Return(true)))
      .WillOnce(Return(false));
  EXPECT_CALL(*mock_blacklist_store_, GetCandidateForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(blacklist_url), Return(true)))
      .WillOnce(DoAll(SetArgPointee<0>(blacklist_url_alt), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, RemoveUrl(Eq(blacklist_url_alt)))
      .WillOnce(Return(true));

  // Blacklist call, first request attempt.
  Blacklist(suggestions_service.get(), blacklist_url);
  EXPECT_EQ(1, suggestions_data_callback_count_);
  EXPECT_FALSE(blacklisting_failed_);

  // Wait for the first scheduling, the first request, the second scheduling,
  // second request and the third scheduling. Again, note that calling
  // RunUntilIdle on the MessageLoop only works when the task is not posted for
  // the future.
  base::MessageLoop::current()->RunUntilIdle();
  io_message_loop_.RunUntilIdle();
  base::MessageLoop::current()->RunUntilIdle();
  io_message_loop_.RunUntilIdle();
  base::MessageLoop::current()->RunUntilIdle();
  CheckSuggestionsData();
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURL) {
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);
  // Ensure scheduling the request doesn't happen before undo.
  base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service->set_blacklist_delay(delay);
  SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();
  GURL blacklist_url(kBlacklistUrl);

  // Blacklist expectations.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklist_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_thumbnail_manager_,
              Initialize(EqualsProto(suggestions_profile)))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  // Undo expectations.
  EXPECT_CALL(*mock_blacklist_store_,
              GetTimeUntilURLReadyForUpload(Eq(blacklist_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(delay), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, RemoveUrl(Eq(blacklist_url)))
    .WillOnce(Return(true));

  Blacklist(suggestions_service.get(), blacklist_url);
  UndoBlacklist(suggestions_service.get(), blacklist_url);

  EXPECT_EQ(2, suggestions_data_callback_count_);
  EXPECT_FALSE(blacklisting_failed_);
  EXPECT_FALSE(undo_blacklisting_failed_);
}

TEST_F(SuggestionsServiceTest, ClearBlacklist) {
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);
  // Ensure scheduling the request doesn't happen before undo.
  base::TimeDelta delay = base::TimeDelta::FromHours(1);
  suggestions_service->set_blacklist_delay(delay);
  SuggestionsProfile suggestions_profile = CreateSuggestionsProfile();
  GURL blacklist_url(kBlacklistUrl);

  factory_.SetFakeResponse(GURL(suggestions::kSuggestionsBlacklistClearURL),
                           suggestions_profile.SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // Blacklist expectations.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklist_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_thumbnail_manager_,
              Initialize(EqualsProto(suggestions_profile)))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_)).Times(AnyNumber());
  EXPECT_CALL(*mock_blacklist_store_, GetTimeUntilReadyForUpload(_))
      .WillOnce(DoAll(SetArgPointee<0>(delay), Return(true)));
  EXPECT_CALL(*mock_blacklist_store_, ClearBlacklist());

  Blacklist(suggestions_service.get(), blacklist_url);
  suggestions_service->ClearBlacklist(base::Bind(
      &SuggestionsServiceTest::CheckCallback, base::Unretained(this)));

  EXPECT_EQ(2, suggestions_data_callback_count_);
  EXPECT_FALSE(blacklisting_failed_);
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURLFailsIfNotInBlacklist) {
  UndoBlacklistURLFailsHelper(true);
}

TEST_F(SuggestionsServiceTest, UndoBlacklistURLFailsIfAlreadyCandidate) {
  UndoBlacklistURLFailsHelper(false);
}

TEST_F(SuggestionsServiceTest, GetBlacklistedUrl) {
  scoped_ptr<GURL> request_url;
  scoped_ptr<net::FakeURLFetcher> fetcher;
  GURL retrieved_url;

  // Not a blacklist request.
  request_url.reset(new GURL("http://not-blacklisting.com/a?b=c"));
  fetcher = CreateURLFetcher(*request_url, NULL, "", net::HTTP_OK,
                             net::URLRequestStatus::SUCCESS);
  EXPECT_FALSE(SuggestionsService::GetBlacklistedUrl(*fetcher, &retrieved_url));

  // An actual blacklist request.
  string blacklisted_url = "http://blacklisted.com/a?b=c&d=e";
  string encoded_blacklisted_url =
      "http%3A%2F%2Fblacklisted.com%2Fa%3Fb%3Dc%26d%3De";
  string blacklist_request_prefix(kSuggestionsBlacklistURLPrefix);
  request_url.reset(
      new GURL(blacklist_request_prefix + encoded_blacklisted_url));
  fetcher.reset();
  fetcher = CreateURLFetcher(*request_url, NULL, "", net::HTTP_OK,
                             net::URLRequestStatus::SUCCESS);
  EXPECT_TRUE(SuggestionsService::GetBlacklistedUrl(*fetcher, &retrieved_url));
  EXPECT_EQ(blacklisted_url, retrieved_url.spec());
}

TEST_F(SuggestionsServiceTest, UpdateBlacklistDelay) {
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  base::TimeDelta initial_delay = suggestions_service->blacklist_delay();

  // Delay unchanged on success.
  suggestions_service->UpdateBlacklistDelay(true);
  EXPECT_EQ(initial_delay, suggestions_service->blacklist_delay());

  // Delay increases on failure.
  suggestions_service->UpdateBlacklistDelay(false);
  EXPECT_GT(suggestions_service->blacklist_delay(), initial_delay);

  // Delay resets on success.
  suggestions_service->UpdateBlacklistDelay(true);
  EXPECT_EQ(initial_delay, suggestions_service->blacklist_delay());
}

TEST_F(SuggestionsServiceTest, CheckDefaultTimeStamps) {
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  SuggestionsProfile suggestions =
      CreateSuggestionsProfileWithExpiryTimestamps();
  suggestions_service->SetDefaultExpiryTimestamp(&suggestions,
                                                 kTestDefaultExpiry);
  EXPECT_EQ(kTestSetExpiry, suggestions.suggestions(0).expiry_ts());
  EXPECT_EQ(kTestDefaultExpiry, suggestions.suggestions(1).expiry_ts());
}

TEST_F(SuggestionsServiceTest, GetPageThumbnail) {
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  ASSERT_TRUE(suggestions_service != NULL);

  GURL test_url(kTestUrl);
  GURL thumbnail_url("https://www.thumbnails.com/thumb.jpg");
  base::Callback<void(const GURL&, const SkBitmap*)> dummy_callback;

  EXPECT_CALL(*mock_thumbnail_manager_, GetImageForURL(test_url, _));
  suggestions_service->GetPageThumbnail(test_url, dummy_callback);

  EXPECT_CALL(*mock_thumbnail_manager_, AddImageURL(test_url, thumbnail_url));
  EXPECT_CALL(*mock_thumbnail_manager_, GetImageForURL(test_url, _));
  suggestions_service->GetPageThumbnailWithURL(test_url, thumbnail_url,
                                               dummy_callback);

}

}  // namespace suggestions
