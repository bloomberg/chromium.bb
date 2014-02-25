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
#include "chrome/test/base/testing_profile.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFakeSuggestionsURL[] = "https://mysuggestions.com/proto";

const char kTestTitle[] = "a title";
const char kTestUrl[] = "http://go.com";

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

}  // namespace

namespace suggestions {

class SuggestionsServiceTest : public testing::Test {
 public:
  void CheckSuggestionsData(const SuggestionsProfile& suggestions_profile) {
    EXPECT_EQ(1, suggestions_profile.suggestions_size());
    EXPECT_EQ(kTestTitle, suggestions_profile.suggestions(0).title());
    EXPECT_EQ(kTestUrl, suggestions_profile.suggestions(0).url());
    ++suggestions_data_check_count_;
  }

  int suggestions_data_check_count_;

 protected:
  SuggestionsServiceTest()
    : suggestions_data_check_count_(0),
      factory_(NULL, base::Bind(&CreateURLFetcher)) {
    profile_ = profile_builder_.Build();
  }
  virtual ~SuggestionsServiceTest() {}

  // Enables the "ChromeSuggestions.Group1" field trial.
  void EnableFieldTrial(const std::string& url) {
    // Clear the existing |field_trial_list_| to avoid firing a DCHECK.
    field_trial_list_.reset(NULL);
    field_trial_list_.reset(
        new base::FieldTrialList(new metrics::SHA1EntropyProvider("foo")));

    chrome_variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> params;
    params[kSuggestionsFieldTrialStateParam] =
        kSuggestionsFieldTrialStateEnabled;
    params[kSuggestionsFieldTrialURLParam] = url;
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

 protected:
  net::FakeURLFetcherFactory factory_;

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
  EnableFieldTrial("");
  EXPECT_TRUE(CreateSuggestionsService() != NULL);
}

TEST_F(SuggestionsServiceTest, FetchSuggestionsData) {
  // Field trial enabled with a specific suggestions URL.
  EnableFieldTrial(kFakeSuggestionsURL);
  SuggestionsService* suggestions_service = CreateSuggestionsService();
  EXPECT_TRUE(suggestions_service != NULL);

  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_title(kTestTitle);
  suggestion->set_url(kTestUrl);
  factory_.SetFakeResponse(GURL(kFakeSuggestionsURL),
                           suggestions_profile.SerializeAsString(),
                           net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  // Send the request. The data will be returned to the callback.
  suggestions_service->FetchSuggestionsData(
      base::Bind(&SuggestionsServiceTest::CheckSuggestionsData,
                 base::Unretained(this)));

  // Send the request a second time.
  suggestions_service->FetchSuggestionsData(
      base::Bind(&SuggestionsServiceTest::CheckSuggestionsData,
                 base::Unretained(this)));

  // (Testing only) wait until suggestion fetch is complete.
  base::MessageLoop::current()->RunUntilIdle();

  // Ensure that CheckSuggestionsData() ran twice.
  EXPECT_EQ(2, suggestions_data_check_count_);
}

}  // namespace suggestions
