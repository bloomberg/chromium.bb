// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/zero_suggest_provider.h"

#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autocomplete/autocomplete_provider_listener.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

class ZeroSuggestProviderTest : public testing::Test,
                                public AutocompleteProviderListener {
 public:
  ZeroSuggestProviderTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  // AutocompleteProviderListener:
  virtual void OnProviderUpdate(bool updated_matches) OVERRIDE;

  void ResetFieldTrialList();

  void CreatePersonalizedFieldTrial();

  // Set up threads for testing; this needs to be instantiated before
  // |profile_|.
  content::TestBrowserThreadBundle thread_bundle_;

  // Needed for OmniboxFieldTrial::ActivateStaticTrials().
  scoped_ptr<base::FieldTrialList> field_trial_list_;

  // URLFetcherFactory implementation registered.
  net::TestURLFetcherFactory test_factory_;

  // Profile we use.
  TestingProfile profile_;

  // ZeroSuggestProvider object under test.
  scoped_refptr<ZeroSuggestProvider> provider_;

  // Default template URL.
  TemplateURL* default_t_url_;
};

ZeroSuggestProviderTest::ZeroSuggestProviderTest() {
  ResetFieldTrialList();
}

void ZeroSuggestProviderTest::SetUp() {
  // Make sure that fetchers are automatically ungregistered upon destruction.
  test_factory_.set_remove_fetcher_on_delete(true);
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &TemplateURLServiceFactory::BuildInstanceFor);
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &AutocompleteClassifierFactory::BuildInstanceFor);

  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(&profile_);
  turl_model->Load();

  TemplateURLData data;
  data.short_name = base::ASCIIToUTF16("t");
  data.SetURL("https://www.google.com/?q={searchTerms}");
  data.suggestions_url = "https://www.google.com/complete/?q={searchTerms}";
  data.instant_url = "https://does/not/exist?strk=1";
  data.search_terms_replacement_key = "strk";
  default_t_url_ = new TemplateURL(data);
  turl_model->Add(default_t_url_);
  turl_model->SetUserSelectedDefaultSearchProvider(default_t_url_);

  provider_ = ZeroSuggestProvider::Create(this, turl_model, &profile_);
}

void ZeroSuggestProviderTest::TearDown() {
  // Shutdown the provider before the profile.
  provider_ = NULL;
}

void ZeroSuggestProviderTest::OnProviderUpdate(bool updated_matches) {
}

void ZeroSuggestProviderTest::ResetFieldTrialList() {
  // Destroy the existing FieldTrialList before creating a new one to avoid
  // a DCHECK.
  field_trial_list_.reset();
  field_trial_list_.reset(new base::FieldTrialList(
      new metrics::SHA1EntropyProvider("foo")));
  variations::testing::ClearAllVariationParams();
}

void ZeroSuggestProviderTest::CreatePersonalizedFieldTrial() {
  std::map<std::string, std::string> params;
  params[std::string(OmniboxFieldTrial::kZeroSuggestRule)] = "true";
  params[std::string(OmniboxFieldTrial::kZeroSuggestVariantRule)] =
      "Personalized";
  variations::AssociateVariationParams(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params);
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestCachingFirstRun) {
  CreatePersonalizedFieldTrial();

  // Ensure the cache is empty.
  PrefService* prefs = profile_.GetPrefs();
  prefs->SetString(prefs::kZeroSuggestCachedResults, std::string());

  std::string url("http://www.cnn.com");
  AutocompleteInput input(base::ASCIIToUTF16(url), base::string16::npos,
                          base::string16(), GURL(url),
                          metrics::OmniboxEventProto::INVALID_SPEC, true, false,
                          true, true,
                          ChromeAutocompleteSchemeClassifier(&profile_));

  provider_->Start(input, false);

  EXPECT_TRUE(prefs->GetString(prefs::kZeroSuggestCachedResults).empty());
  EXPECT_TRUE(provider_->matches().empty());

  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(200);
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  fetcher->SetResponseString(json_response);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(4U, provider_->matches().size());  // 3 results + verbatim
  EXPECT_EQ(json_response, prefs->GetString(prefs::kZeroSuggestCachedResults));
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestHasCachedResults) {
  CreatePersonalizedFieldTrial();

  std::string url("http://www.cnn.com");
  AutocompleteInput input(base::ASCIIToUTF16(url), base::string16::npos,
                          base::string16(), GURL(url),
                          metrics::OmniboxEventProto::INVALID_SPEC, true, false,
                          true, true,
                          ChromeAutocompleteSchemeClassifier(&profile_));

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = profile_.GetPrefs();
  prefs->SetString(prefs::kZeroSuggestCachedResults, json_response);

  provider_->Start(input, false);

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(4U, provider_->matches().size());
  EXPECT_EQ(base::ASCIIToUTF16("search1"), provider_->matches()[1].contents);
  EXPECT_EQ(base::ASCIIToUTF16("search2"), provider_->matches()[2].contents);
  EXPECT_EQ(base::ASCIIToUTF16("search3"), provider_->matches()[3].contents);

  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(200);
  std::string json_response2("[\"\",[\"search4\", \"search5\", \"search6\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  fetcher->SetResponseString(json_response2);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  base::RunLoop().RunUntilIdle();

  // Expect the same 4 results after the response has been handled.
  ASSERT_EQ(4U, provider_->matches().size());
  EXPECT_EQ(base::ASCIIToUTF16("search1"), provider_->matches()[1].contents);
  EXPECT_EQ(base::ASCIIToUTF16("search2"), provider_->matches()[2].contents);
  EXPECT_EQ(base::ASCIIToUTF16("search3"), provider_->matches()[3].contents);

  // Expect the new results have been stored.
  EXPECT_EQ(json_response2,
            prefs->GetString(prefs::kZeroSuggestCachedResults));
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestReceivedEmptyResults) {
  CreatePersonalizedFieldTrial();

  std::string url("http://www.cnn.com");
  AutocompleteInput input(base::ASCIIToUTF16(url), base::string16::npos,
                          base::string16(), GURL(url),
                          metrics::OmniboxEventProto::INVALID_SPEC, true, false,
                          true, true,
                          ChromeAutocompleteSchemeClassifier(&profile_));

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = profile_.GetPrefs();
  prefs->SetString(prefs::kZeroSuggestCachedResults, json_response);

  provider_->Start(input, false);

  // Expect that matches get populated synchronously out of the cache.
  ASSERT_EQ(4U, provider_->matches().size());
  EXPECT_EQ(base::ASCIIToUTF16("search1"), provider_->matches()[1].contents);
  EXPECT_EQ(base::ASCIIToUTF16("search2"), provider_->matches()[2].contents);
  EXPECT_EQ(base::ASCIIToUTF16("search3"), provider_->matches()[3].contents);

  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(1);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(200);
  std::string empty_response("[\"\",[],[],[],{}]");
  fetcher->SetResponseString(empty_response);
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  base::RunLoop().RunUntilIdle();

  // Expect that the matches have been cleared.
  ASSERT_TRUE(provider_->matches().empty());

  // Expect the new results have been stored.
  EXPECT_EQ(empty_response,
            prefs->GetString(prefs::kZeroSuggestCachedResults));
}
