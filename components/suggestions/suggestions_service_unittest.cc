// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/suggestions/suggestions_service.h"

#include <map>
#include <sstream>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "components/suggestions/blacklist_store.h"
#include "components/suggestions/image_manager.h"
#include "components/suggestions/proto/suggestions.pb.h"
#include "components/suggestions/suggestions_store.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::DoAll;
using ::testing::Eq;
using ::testing::Return;
using testing::SetArgPointee;
using ::testing::NiceMock;
using ::testing::StrictMock;
using ::testing::_;

namespace {

const char kFakeSuggestionsURL[] = "https://mysuggestions.com/proto";
const char kFakeSuggestionsCommonParams[] = "foo=bar";
const char kFakeBlacklistPath[] = "/blacklist";
const char kFakeBlacklistUrlParam[] = "baz";

const char kTestTitle[] = "a title";
const char kTestUrl[] = "http://go.com";
const char kBlacklistUrl[] = "http://blacklist.com";
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
  request_url << kFakeSuggestionsURL << kFakeBlacklistPath << "?"
              << kFakeSuggestionsCommonParams << "&" << kFakeBlacklistUrlParam
              << "=" << net::EscapeQueryParamValue(blacklist_url.spec(), true);
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

scoped_ptr<SuggestionsProfile> CreateSuggestionsProfile() {
  scoped_ptr<SuggestionsProfile> profile(new SuggestionsProfile());
  ChromeSuggestion* suggestion = profile->add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);
  suggestion->set_expiry_ts(kTestSetExpiry);
  return profile.Pass();
}

// Creates one suggestion with expiry timestamp and one without.
SuggestionsProfile CreateSuggestionsProfileWithExpiryTimestamps() {
  SuggestionsProfile profile;
  ChromeSuggestion* suggestion = profile.add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);
  suggestion->set_expiry_ts(kTestSetExpiry);

  suggestion = profile.add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);

  return profile;
}

class MockSuggestionsStore : public suggestions::SuggestionsStore {
 public:
  MOCK_METHOD1(LoadSuggestions, bool(SuggestionsProfile*));
  MOCK_METHOD1(StoreSuggestions, bool(const SuggestionsProfile&));
  MOCK_METHOD0(ClearSuggestions, void());
};

class MockImageManager : public suggestions::ImageManager {
 public:
  MockImageManager() {}
  virtual ~MockImageManager() {}
  MOCK_METHOD1(Initialize, void(const SuggestionsProfile&));
  MOCK_METHOD2(GetImageForURL,
               void(const GURL&,
                    base::Callback<void(const GURL&, const SkBitmap*)>));
};

class MockBlacklistStore : public suggestions::BlacklistStore {
 public:
  MOCK_METHOD1(BlacklistUrl, bool(const GURL&));
  MOCK_METHOD1(GetFirstUrlFromBlacklist, bool(GURL*));
  MOCK_METHOD1(RemoveUrl, bool(const GURL&));
  MOCK_METHOD1(FilterSuggestions, void(SuggestionsProfile*));
};

class SuggestionsServiceTest : public testing::Test {
 public:
  void CheckSuggestionsData(const SuggestionsProfile& suggestions_profile) {
    EXPECT_EQ(1, suggestions_profile.suggestions_size());
    EXPECT_EQ(kTestTitle, suggestions_profile.suggestions(0).title());
    EXPECT_EQ(kTestUrl, suggestions_profile.suggestions(0).url());
    ++suggestions_data_check_count_;
  }

  void ExpectEmptySuggestionsProfile(const SuggestionsProfile& profile) {
    EXPECT_EQ(0, profile.suggestions_size());
    ++suggestions_empty_data_count_;
  }

  int suggestions_data_check_count_;
  int suggestions_empty_data_count_;

 protected:
  SuggestionsServiceTest()
      : suggestions_data_check_count_(0),
        suggestions_empty_data_count_(0),
        factory_(NULL, base::Bind(&CreateURLFetcher)),
        mock_suggestions_store_(NULL),
        mock_thumbnail_manager_(NULL) {}

  virtual ~SuggestionsServiceTest() {}

  virtual void SetUp() OVERRIDE {
    request_context_ = new net::TestURLRequestContextGetter(
        io_message_loop_.message_loop_proxy());
  }

  // Enables the "ChromeSuggestions.Group1" field trial.
  void EnableFieldTrial(const std::string& url,
                        const std::string& common_params,
                        const std::string& blacklist_path,
                        const std::string& blacklist_url_param,
                        bool control_group) {
    // Clear the existing |field_trial_list_| to avoid firing a DCHECK.
    field_trial_list_.reset(NULL);
    field_trial_list_.reset(
        new base::FieldTrialList(new metrics::SHA1EntropyProvider("foo")));

    variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> params;
    params[kSuggestionsFieldTrialStateParam] =
        kSuggestionsFieldTrialStateEnabled;
    if (control_group) {
      params[kSuggestionsFieldTrialControlParam] =
          kSuggestionsFieldTrialStateEnabled;
    }
    params[kSuggestionsFieldTrialURLParam] = url;
    params[kSuggestionsFieldTrialCommonParamsParam] = common_params;
    params[kSuggestionsFieldTrialBlacklistPathParam] = blacklist_path;
    params[kSuggestionsFieldTrialBlacklistUrlParam] = blacklist_url_param;
    variations::AssociateVariationParams(kSuggestionsFieldTrialName, "Group1",
                                         params);
    field_trial_ = base::FieldTrialList::CreateFieldTrial(
        kSuggestionsFieldTrialName, "Group1");
    field_trial_->group();
  }

  // Should not be called more than once per test since it stashes the
  // SuggestionsStore in |mock_suggestions_store_|.
  SuggestionsService* CreateSuggestionsServiceWithMocks() {
    mock_suggestions_store_ = new StrictMock<MockSuggestionsStore>();
    mock_thumbnail_manager_ = new NiceMock<MockImageManager>();
    mock_blacklist_store_ = new MockBlacklistStore();
    return new SuggestionsService(
        request_context_, scoped_ptr<SuggestionsStore>(mock_suggestions_store_),
        scoped_ptr<ImageManager>(mock_thumbnail_manager_),
        scoped_ptr<BlacklistStore>(mock_blacklist_store_));
  }

  void FetchSuggestionsDataNoTimeoutHelper(bool interleaved_requests) {
    // Field trial enabled with a specific suggestions URL.
    EnableFieldTrial(kFakeSuggestionsURL, kFakeSuggestionsCommonParams,
                     kFakeBlacklistPath, kFakeBlacklistUrlParam, false);
    scoped_ptr<SuggestionsService> suggestions_service(
        CreateSuggestionsServiceWithMocks());
    EXPECT_TRUE(suggestions_service != NULL);
    scoped_ptr<SuggestionsProfile> suggestions_profile(
        CreateSuggestionsProfile());
    // Set up net::FakeURLFetcherFactory.
    std::string expected_url =
        (std::string(kFakeSuggestionsURL) + "?") + kFakeSuggestionsCommonParams;
    factory_.SetFakeResponse(GURL(expected_url),
                             suggestions_profile->SerializeAsString(),
                             net::HTTP_OK, net::URLRequestStatus::SUCCESS);
    // Set up expectations on the SuggestionsStore. The number depends on
    // whether the second request is issued (it won't be issued if the second
    // fetch occurs before the first request has completed).
    int expected_count = interleaved_requests ? 1 : 2;
    EXPECT_CALL(*mock_suggestions_store_,
                StoreSuggestions(EqualsProto(*suggestions_profile)))
        .Times(expected_count)
        .WillRepeatedly(Return(true));

    // Expect a call to the blacklist store. Return that there's nothing to
    // blacklist.
    EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_))
        .Times(expected_count);
    EXPECT_CALL(*mock_blacklist_store_, GetFirstUrlFromBlacklist(_))
        .Times(expected_count)
        .WillRepeatedly(Return(false));

    // Send the request. The data will be returned to the callback.
    suggestions_service->FetchSuggestionsDataNoTimeout(base::Bind(
        &SuggestionsServiceTest::CheckSuggestionsData, base::Unretained(this)));

    if (!interleaved_requests)
      io_message_loop_.RunUntilIdle();  // Let request complete.

    // Send the request a second time.
    suggestions_service->FetchSuggestionsDataNoTimeout(base::Bind(
        &SuggestionsServiceTest::CheckSuggestionsData, base::Unretained(this)));

    // (Testing only) wait until suggestion fetch is complete.
    io_message_loop_.RunUntilIdle();

    // Ensure that CheckSuggestionsData() ran twice.
    EXPECT_EQ(2, suggestions_data_check_count_);
  }

 protected:
  base::MessageLoopForIO io_message_loop_;
  net::FakeURLFetcherFactory factory_;
  // Only used if the SuggestionsService is built with mocks. Not owned.
  MockSuggestionsStore* mock_suggestions_store_;
  MockImageManager* mock_thumbnail_manager_;
  MockBlacklistStore* mock_blacklist_store_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;

 private:
  scoped_ptr<base::FieldTrialList> field_trial_list_;
  scoped_refptr<base::FieldTrial> field_trial_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsServiceTest);
};

TEST_F(SuggestionsServiceTest, IsControlGroup) {
  // Field trial enabled.
  EnableFieldTrial("", "", "", "", false);
  EXPECT_FALSE(SuggestionsService::IsControlGroup());

  EnableFieldTrial("", "", "", "", true);
  EXPECT_TRUE(SuggestionsService::IsControlGroup());
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataNoTimeout) {
  FetchSuggestionsDataNoTimeoutHelper(false);
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataNoTimeoutInterleaved) {
  FetchSuggestionsDataNoTimeoutHelper(true);
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataRequestError) {
  // Field trial enabled with a specific suggestions URL.
  EnableFieldTrial(kFakeSuggestionsURL, kFakeSuggestionsCommonParams,
                   kFakeBlacklistPath, kFakeBlacklistUrlParam, false);
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);

  // Fake a request error.
  std::string expected_url =
      (std::string(kFakeSuggestionsURL) + "?") + kFakeSuggestionsCommonParams;
  factory_.SetFakeResponse(GURL(expected_url), "irrelevant", net::HTTP_OK,
                           net::URLRequestStatus::FAILED);

  // Set up expectations on the SuggestionsStore.
  EXPECT_CALL(*mock_suggestions_store_, LoadSuggestions(_))
      .WillOnce(Return(true));

  // Expect a call to the blacklist store. Return that there's nothing to
  // blacklist.
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_));
  EXPECT_CALL(*mock_blacklist_store_, GetFirstUrlFromBlacklist(_))
      .WillOnce(Return(false));

  // Send the request. Empty data will be returned to the callback.
  suggestions_service->FetchSuggestionsData(
      base::Bind(&SuggestionsServiceTest::ExpectEmptySuggestionsProfile,
                 base::Unretained(this)));

  // (Testing only) wait until suggestion fetch is complete.
  io_message_loop_.RunUntilIdle();

  // Ensure that ExpectEmptySuggestionsProfile ran once.
  EXPECT_EQ(1, suggestions_empty_data_count_);
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataResponseNotOK) {
  // Field trial enabled with a specific suggestions URL.
  EnableFieldTrial(kFakeSuggestionsURL, kFakeSuggestionsCommonParams,
                   kFakeBlacklistPath, kFakeBlacklistUrlParam, false);
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);

  // Response code != 200.
  std::string expected_url =
      (std::string(kFakeSuggestionsURL) + "?") + kFakeSuggestionsCommonParams;
  factory_.SetFakeResponse(GURL(expected_url), "irrelevant",
                           net::HTTP_BAD_REQUEST,
                           net::URLRequestStatus::SUCCESS);

  // Set up expectations on the SuggestionsStore.
  EXPECT_CALL(*mock_suggestions_store_, ClearSuggestions());

  // Expect a call to the blacklist store. Return that there's nothing to
  // blacklist.
  EXPECT_CALL(*mock_blacklist_store_, GetFirstUrlFromBlacklist(_))
      .WillOnce(Return(false));

  // Send the request. Empty data will be returned to the callback.
  suggestions_service->FetchSuggestionsData(
      base::Bind(&SuggestionsServiceTest::ExpectEmptySuggestionsProfile,
                 base::Unretained(this)));

  // (Testing only) wait until suggestion fetch is complete.
  io_message_loop_.RunUntilIdle();

  // Ensure that ExpectEmptySuggestionsProfile ran once.
  EXPECT_EQ(1, suggestions_empty_data_count_);
}

TEST_F(SuggestionsServiceTest, BlacklistURL) {
  EnableFieldTrial(kFakeSuggestionsURL, kFakeSuggestionsCommonParams,
                   kFakeBlacklistPath, kFakeBlacklistUrlParam, false);
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);

  GURL blacklist_url(kBlacklistUrl);
  std::string request_url = GetExpectedBlacklistRequestUrl(blacklist_url);
  scoped_ptr<SuggestionsProfile> suggestions_profile(
      CreateSuggestionsProfile());
  factory_.SetFakeResponse(GURL(request_url),
                           suggestions_profile->SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // Set up expectations on the SuggestionsStore.
  EXPECT_CALL(*mock_suggestions_store_,
              StoreSuggestions(EqualsProto(*suggestions_profile)))
      .WillOnce(Return(true));

  // Expected calls to the blacklist store.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklist_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_blacklist_store_, RemoveUrl(Eq(blacklist_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_));
  EXPECT_CALL(*mock_blacklist_store_, GetFirstUrlFromBlacklist(_))
      .WillOnce(Return(false));

  // Send the request. The data will be returned to the callback.
  suggestions_service->BlacklistURL(
      blacklist_url, base::Bind(&SuggestionsServiceTest::CheckSuggestionsData,
                                base::Unretained(this)));

  // (Testing only) wait until blacklist request is complete.
  io_message_loop_.RunUntilIdle();

  // Ensure that CheckSuggestionsData() ran once.
  EXPECT_EQ(1, suggestions_data_check_count_);
}

// Initial blacklist request fails, triggering a scheduled upload which
// succeeds.
TEST_F(SuggestionsServiceTest, BlacklistURLFails) {
  EnableFieldTrial(kFakeSuggestionsURL, kFakeSuggestionsCommonParams,
                   kFakeBlacklistPath, kFakeBlacklistUrlParam, false);
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMocks());
  EXPECT_TRUE(suggestions_service != NULL);
  suggestions_service->set_blacklist_delay(0);  // Don't wait during a test!
  scoped_ptr<SuggestionsProfile> suggestions_profile(
      CreateSuggestionsProfile());
  GURL blacklist_url(kBlacklistUrl);

  // Set up behavior for the first call to blacklist.
  std::string request_url = GetExpectedBlacklistRequestUrl(blacklist_url);
  factory_.SetFakeResponse(GURL(request_url), "irrelevant", net::HTTP_OK,
                           net::URLRequestStatus::FAILED);

  // Expectations specific to the first request.
  EXPECT_CALL(*mock_blacklist_store_, BlacklistUrl(Eq(blacklist_url)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_suggestions_store_, LoadSuggestions(_))
      .WillOnce(DoAll(SetArgPointee<0>(*suggestions_profile), Return(true)));

  // Expectations specific to the second request.
  EXPECT_CALL(*mock_suggestions_store_,
              StoreSuggestions(EqualsProto(*suggestions_profile)))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_blacklist_store_, RemoveUrl(Eq(blacklist_url)))
      .WillOnce(Return(true));

  // Expectations pertaining to both requests.
  EXPECT_CALL(*mock_blacklist_store_, FilterSuggestions(_)).Times(2);
  EXPECT_CALL(*mock_blacklist_store_, GetFirstUrlFromBlacklist(_))
      .WillOnce(Return(true))
      .WillOnce(DoAll(SetArgPointee<0>(blacklist_url), Return(true)))
      .WillOnce(Return(false));

  // Send the request. The data will be returned to the callback.
  suggestions_service->BlacklistURL(
      blacklist_url, base::Bind(&SuggestionsServiceTest::CheckSuggestionsData,
                                base::Unretained(this)));

  // The first FakeURLFetcher was created; we can now set up behavior for the
  // second call to blacklist.
  factory_.SetFakeResponse(GURL(request_url),
                           suggestions_profile->SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // (Testing only) wait until both requests are complete.
  io_message_loop_.RunUntilIdle();
  // ... Other task gets posted to the message loop.
  base::MessageLoop::current()->RunUntilIdle();
  // ... And completes.
  io_message_loop_.RunUntilIdle();

  // Ensure that CheckSuggestionsData() ran once.
  EXPECT_EQ(1, suggestions_data_check_count_);
}

TEST_F(SuggestionsServiceTest, GetBlacklistedUrl) {
  EnableFieldTrial(kFakeSuggestionsURL, kFakeSuggestionsCommonParams,
                   kFakeBlacklistPath, kFakeBlacklistUrlParam, false);

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
  string blacklist_request_prefix =
      "https://mysuggestions.com/proto/blacklist?foo=bar&baz=";
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
  int initial_delay = suggestions_service->blacklist_delay();

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
}  // namespace suggestions
