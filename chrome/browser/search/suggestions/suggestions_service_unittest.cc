// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_service.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search/suggestions/suggestions_store.h"
#include "chrome/test/base/testing_profile.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace {

const char kFakeSuggestionsURL[] = "https://mysuggestions.com/proto";
const char kFakeSuggestionsSuffix[] = "?foo=bar";
const char kFakeBlacklistSuffix[] = "/blacklist?foo=bar&baz=";

const char kTestTitle[] = "a title";
const char kTestUrl[] = "http://go.com";
const char kBlacklistUrl[] = "http://blacklist.com";

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

namespace {

scoped_ptr<SuggestionsProfile> CreateSuggestionsProfile() {
  scoped_ptr<SuggestionsProfile> profile(new SuggestionsProfile());
  ChromeSuggestion* suggestion = profile->add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);
  return profile.Pass();
}

class MockSuggestionsStore : public suggestions::SuggestionsStore {
 public:
  MOCK_METHOD1(LoadSuggestions, bool(SuggestionsProfile*));
  MOCK_METHOD1(StoreSuggestions, bool(const SuggestionsProfile&));
  MOCK_METHOD0(ClearSuggestions, void());
};

}  // namespace

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
        mock_suggestions_store_(NULL) {
    profile_ = profile_builder_.Build();
  }
  virtual ~SuggestionsServiceTest() {}

  // Enables the "ChromeSuggestions.Group1" field trial.
  void EnableFieldTrial(const std::string& url,
                        const std::string& suggestions_suffix,
                        const std::string& blacklist_suffix) {
    // Clear the existing |field_trial_list_| to avoid firing a DCHECK.
    field_trial_list_.reset(NULL);
    field_trial_list_.reset(
        new base::FieldTrialList(new metrics::SHA1EntropyProvider("foo")));

    chrome_variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> params;
    params[kSuggestionsFieldTrialStateParam] =
        kSuggestionsFieldTrialStateEnabled;
    params[kSuggestionsFieldTrialURLParam] = url;
    params[kSuggestionsFieldTrialSuggestionsSuffixParam] = suggestions_suffix;
    params[kSuggestionsFieldTrialBlacklistSuffixParam] = blacklist_suffix;
    chrome_variations::AssociateVariationParams(kSuggestionsFieldTrialName,
                                                "Group1", params);
    field_trial_ = base::FieldTrialList::CreateFieldTrial(
        kSuggestionsFieldTrialName, "Group1");
    field_trial_->group();
  }

  SuggestionsService* CreateSuggestionsService() {
    SuggestionsServiceFactory* suggestions_service_factory =
        SuggestionsServiceFactory::GetInstance();
    return suggestions_service_factory->GetForProfile(profile_.get());
  }

  // Should not be called more than once per test since it stashes the
  // SuggestionsStore in |mock_suggestions_store_|.
  SuggestionsService* CreateSuggestionsServiceWithMockStore() {
    mock_suggestions_store_ = new StrictMock<MockSuggestionsStore>();
    return new SuggestionsService(
        profile_.get(), scoped_ptr<SuggestionsStore>(mock_suggestions_store_));
  }

  void FetchSuggestionsDataNoTimeoutHelper(bool interleaved_requests) {
    // Field trial enabled with a specific suggestions URL.
    EnableFieldTrial(kFakeSuggestionsURL, kFakeSuggestionsSuffix,
                     kFakeBlacklistSuffix);
    scoped_ptr<SuggestionsService> suggestions_service(
        CreateSuggestionsServiceWithMockStore());
    EXPECT_TRUE(suggestions_service != NULL);
    scoped_ptr<SuggestionsProfile> suggestions_profile(
        CreateSuggestionsProfile());

    // Set up net::FakeURLFetcherFactory.
    std::string expected_url =
        std::string(kFakeSuggestionsURL) + kFakeSuggestionsSuffix;
    factory_.SetFakeResponse(GURL(expected_url),
                             suggestions_profile->SerializeAsString(),
                             net::HTTP_OK, net::URLRequestStatus::SUCCESS);

    // Set up expectations on the SuggestionsStore. The number depends on
    // whether the second request is issued (it won't be issued if the second
    // fetch occurs before the first request has completed).
    int expected_count = 2;
    if (interleaved_requests)
      expected_count = 1;
    EXPECT_CALL(*mock_suggestions_store_,
                StoreSuggestions(EqualsProto(*suggestions_profile)))
        .Times(expected_count)
        .WillRepeatedly(Return(true));

    // Send the request. The data will be returned to the callback.
    suggestions_service->FetchSuggestionsDataNoTimeout(base::Bind(
        &SuggestionsServiceTest::CheckSuggestionsData, base::Unretained(this)));

    if (!interleaved_requests)
      base::MessageLoop::current()->RunUntilIdle();  // Let request complete.

    // Send the request a second time.
    suggestions_service->FetchSuggestionsDataNoTimeout(base::Bind(
        &SuggestionsServiceTest::CheckSuggestionsData, base::Unretained(this)));

    // (Testing only) wait until suggestion fetch is complete.
    base::MessageLoop::current()->RunUntilIdle();

    // Ensure that CheckSuggestionsData() ran twice.
    EXPECT_EQ(2, suggestions_data_check_count_);
  }

 protected:
  net::FakeURLFetcherFactory factory_;
  // Only used if the SuggestionsService is built with a MockSuggestionsStore.
  // Not owned.
  MockSuggestionsStore* mock_suggestions_store_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<base::FieldTrialList> field_trial_list_;
  scoped_refptr<base::FieldTrial> field_trial_;
  TestingProfile::Builder profile_builder_;
  scoped_ptr<TestingProfile> profile_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsServiceTest);
};

TEST_F(SuggestionsServiceTest, ServiceBeingCreated) {
  // Field trial not enabled.
  EXPECT_TRUE(CreateSuggestionsService() == NULL);

  // Field trial enabled.
  EnableFieldTrial("", "", "");
  EXPECT_TRUE(CreateSuggestionsService() != NULL);
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataNoTimeout) {
  FetchSuggestionsDataNoTimeoutHelper(false);
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataNoTimeoutInterleaved) {
  FetchSuggestionsDataNoTimeoutHelper(true);
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataRequestError) {
  // Field trial enabled with a specific suggestions URL.
  EnableFieldTrial(kFakeSuggestionsURL, kFakeSuggestionsSuffix,
                   kFakeBlacklistSuffix);
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMockStore());
  EXPECT_TRUE(suggestions_service != NULL);

  // Fake a request error.
  std::string expected_url =
      std::string(kFakeSuggestionsURL) + kFakeSuggestionsSuffix;
  factory_.SetFakeResponse(GURL(expected_url), "irrelevant", net::HTTP_OK,
                           net::URLRequestStatus::FAILED);

  // Set up expectations on the SuggestionsStore.
  EXPECT_CALL(*mock_suggestions_store_, LoadSuggestions(_))
      .WillOnce(Return(true));

  // Send the request. Empty data will be returned to the callback.
  suggestions_service->FetchSuggestionsData(
      base::Bind(&SuggestionsServiceTest::ExpectEmptySuggestionsProfile,
                 base::Unretained(this)));

  // (Testing only) wait until suggestion fetch is complete.
  base::MessageLoop::current()->RunUntilIdle();

  // Ensure that ExpectEmptySuggestionsProfile ran once.
  EXPECT_EQ(1, suggestions_empty_data_count_);
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsDataResponseNotOK) {
  // Field trial enabled with a specific suggestions URL.
  EnableFieldTrial(kFakeSuggestionsURL, kFakeSuggestionsSuffix,
                   kFakeBlacklistSuffix);
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMockStore());
  EXPECT_TRUE(suggestions_service != NULL);

  // Response code != 200.
  std::string expected_url =
      std::string(kFakeSuggestionsURL) + kFakeSuggestionsSuffix;
  factory_.SetFakeResponse(GURL(expected_url), "irrelevant",
                           net::HTTP_BAD_REQUEST,
                           net::URLRequestStatus::SUCCESS);

  // Set up expectations on the SuggestionsStore.
  EXPECT_CALL(*mock_suggestions_store_, ClearSuggestions());

  // Send the request. Empty data will be returned to the callback.
  suggestions_service->FetchSuggestionsData(
      base::Bind(&SuggestionsServiceTest::ExpectEmptySuggestionsProfile,
                 base::Unretained(this)));

  // (Testing only) wait until suggestion fetch is complete.
  base::MessageLoop::current()->RunUntilIdle();

  // Ensure that ExpectEmptySuggestionsProfile ran once.
  EXPECT_EQ(1, suggestions_empty_data_count_);
}

TEST_F(SuggestionsServiceTest, BlacklistURL) {
  EnableFieldTrial(kFakeSuggestionsURL, kFakeSuggestionsSuffix,
                   kFakeBlacklistSuffix);
  scoped_ptr<SuggestionsService> suggestions_service(
      CreateSuggestionsServiceWithMockStore());
  EXPECT_TRUE(suggestions_service != NULL);

  std::string expected_url(kFakeSuggestionsURL);
  expected_url.append(kFakeBlacklistSuffix)
      .append(net::EscapeQueryParamValue(GURL(kBlacklistUrl).spec(), true));
  scoped_ptr<SuggestionsProfile> suggestions_profile(
      CreateSuggestionsProfile());
  factory_.SetFakeResponse(GURL(expected_url),
                           suggestions_profile->SerializeAsString(),
                           net::HTTP_OK, net::URLRequestStatus::SUCCESS);

  // Set up expectations on the SuggestionsStore.
  EXPECT_CALL(*mock_suggestions_store_,
              StoreSuggestions(EqualsProto(*suggestions_profile)))
      .WillOnce(Return(true));

  // Send the request. The data will be returned to the callback.
  suggestions_service->BlacklistURL(
      GURL(kBlacklistUrl),
      base::Bind(&SuggestionsServiceTest::CheckSuggestionsData,
                 base::Unretained(this)));

  // (Testing only) wait until blacklist request is complete.
  base::MessageLoop::current()->RunUntilIdle();

  // Ensure that CheckSuggestionsData() ran once.
  EXPECT_EQ(1, suggestions_data_check_count_);
}

}  // namespace suggestions
