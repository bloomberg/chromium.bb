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
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/top_sites.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/autocomplete_provider_listener.h"
#include "components/omnibox/omnibox_field_trial.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class FakeEmptyTopSites : public history::TopSites {
 public:
  FakeEmptyTopSites() {
  }

  // history::TopSites:
  bool SetPageThumbnail(const GURL& url, const gfx::Image& thumbnail,
                        const ThumbnailScore& score) override {
    return false;
  }
  bool SetPageThumbnailToJPEGBytes(const GURL& url,
                                   const base::RefCountedMemory* memory,
                                   const ThumbnailScore& score) override {
    return false;
  }
  void GetMostVisitedURLs(const GetMostVisitedURLsCallback& callback,
                          bool include_forced_urls) override;
  bool GetPageThumbnail(const GURL& url, bool prefix_match,
                        scoped_refptr<base::RefCountedMemory>* bytes) override {
    return false;
  }
  bool GetPageThumbnailScore(const GURL& url, ThumbnailScore* score) override {
    return false;
  }
  bool GetTemporaryPageThumbnailScore(const GURL& url, ThumbnailScore* score)
      override {
    return false;
  }
  void SyncWithHistory() override {}
  bool HasBlacklistedItems() const override {
    return false;
  }
  void AddBlacklistedURL(const GURL& url) override {}
  void RemoveBlacklistedURL(const GURL& url) override {}
  bool IsBlacklisted(const GURL& url) override {
    return false;
  }
  void ClearBlacklistedURLs() override {}
  base::CancelableTaskTracker::TaskId StartQueryForMostVisited() override {
    return 0;
  }
  bool IsKnownURL(const GURL& url) override {
    return false;
  }
  const std::string& GetCanonicalURLString(const GURL& url) const override {
    CHECK(false);
    return *(new std::string());
  }
  bool IsNonForcedFull() override {
    return false;
  }
  bool IsForcedFull() override {
    return false;
  }
  bool loaded() const override {
    return false;
  }
  history::PrepopulatedPageList GetPrepopulatedPages() override {
    return history::PrepopulatedPageList();
  }
  bool AddForcedURL(const GURL& url, const base::Time& time) override {
    return false;
  }

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override {}

  // A test-specific field for controlling when most visited callback is run
  // after top sites have been requested.
  GetMostVisitedURLsCallback mv_callback;

 protected:
  ~FakeEmptyTopSites() override {}
};

void FakeEmptyTopSites::GetMostVisitedURLs(
    const GetMostVisitedURLsCallback& callback,
    bool include_forced_urls)  {
  mv_callback = callback;
}

scoped_refptr<RefcountedKeyedService> BuildFakeEmptyTopSites(
    content::BrowserContext* profile) {
  scoped_refptr<history::TopSites> top_sites = new FakeEmptyTopSites();
  return top_sites;
}

}  // namespace


class ZeroSuggestProviderTest : public testing::Test,
                                public AutocompleteProviderListener {
 public:
  ZeroSuggestProviderTest();

  void SetUp() override;
  void TearDown() override;

 protected:
  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override;

  void ResetFieldTrialList();

  void CreatePersonalizedFieldTrial();
  void CreateMostVisitedFieldTrial();

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

  profile_.DestroyTopSites();
  TopSitesFactory::GetInstance()->SetTestingFactory(&profile_,
                                                    BuildFakeEmptyTopSites);
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

void ZeroSuggestProviderTest::CreateMostVisitedFieldTrial() {
  std::map<std::string, std::string> params;
  params[std::string(OmniboxFieldTrial::kZeroSuggestRule)] = "true";
  params[std::string(OmniboxFieldTrial::kZeroSuggestVariantRule)] =
      "MostVisitedWithoutSERP";
  variations::AssociateVariationParams(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params);
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
}

TEST_F(ZeroSuggestProviderTest, TestDoesNotReturnMatchesForPrefix) {
  CreatePersonalizedFieldTrial();

  std::string url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url), base::string16::npos,
                          std::string(), GURL(url),
                          metrics::OmniboxEventProto::INVALID_SPEC, true, false,
                          true, true,
                          ChromeAutocompleteSchemeClassifier(&profile_));

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = profile_.GetPrefs();
  prefs->SetString(prefs::kZeroSuggestCachedResults, json_response);

  provider_->Start(input, false, false);

  // Expect that matches don't get populated out of cache because we are not
  // in zero suggest mode.
  EXPECT_TRUE(provider_->matches().empty());

  // Expect that fetcher did not get created.
  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(1);
  EXPECT_FALSE(fetcher);
}

TEST_F(ZeroSuggestProviderTest, TestMostVisitedCallback) {
  CreateMostVisitedFieldTrial();

  std::string current_url("http://www.foxnews.com/");
  std::string input_url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(input_url), base::string16::npos,
                          std::string(), GURL(current_url),
                          metrics::OmniboxEventProto::OTHER, false, false,
                          true, true,
                          ChromeAutocompleteSchemeClassifier(&profile_));
  history::MostVisitedURLList urls;
  history::MostVisitedURL url(GURL("http://foo.com/"),
                              base::ASCIIToUTF16("Foo"));
  urls.push_back(url);

  provider_->Start(input, false, true);
  EXPECT_TRUE(provider_->matches().empty());
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(&profile_);
  static_cast<FakeEmptyTopSites*>(top_sites.get())->mv_callback.Run(urls);
  // Should have verbatim match + most visited url match.
  EXPECT_EQ(2U, provider_->matches().size());
  provider_->Stop(false, false);

  provider_->Start(input, false, true);
  provider_->Stop(false, false);
  EXPECT_TRUE(provider_->matches().empty());
  // Most visited results arriving after Stop() has been called, ensure they
  // are not displayed.
  static_cast<FakeEmptyTopSites*>(top_sites.get())->mv_callback.Run(urls);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ZeroSuggestProviderTest, TestMostVisitedNavigateToSearchPage) {
  CreateMostVisitedFieldTrial();

  std::string current_url("http://www.foxnews.com/");
  std::string input_url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(input_url), base::string16::npos,
                          std::string(), GURL(current_url),
                          metrics::OmniboxEventProto::OTHER, false, false,
                          true, true,
                          ChromeAutocompleteSchemeClassifier(&profile_));
  history::MostVisitedURLList urls;
  history::MostVisitedURL url(GURL("http://foo.com/"),
                              base::ASCIIToUTF16("Foo"));
  urls.push_back(url);

  provider_->Start(input, false, true);
  EXPECT_TRUE(provider_->matches().empty());
  // Stop() doesn't always get called.

  std::string search_url("https://www.google.com/?q=flowers");
  AutocompleteInput srp_input(
      base::ASCIIToUTF16(search_url),
      base::string16::npos,
      std::string(),
      GURL(search_url),
      metrics::OmniboxEventProto::
          SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT,
      false,
      false,
      true,
      true,
      ChromeAutocompleteSchemeClassifier(&profile_));

  provider_->Start(srp_input, false, true);
  EXPECT_TRUE(provider_->matches().empty());
  // Most visited results arriving after a new request has been started.
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(&profile_);
  static_cast<FakeEmptyTopSites*>(top_sites.get())->mv_callback.Run(urls);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ZeroSuggestProviderTest, TestPsuggestZeroSuggestCachingFirstRun) {
  CreatePersonalizedFieldTrial();

  // Ensure the cache is empty.
  PrefService* prefs = profile_.GetPrefs();
  prefs->SetString(prefs::kZeroSuggestCachedResults, std::string());

  std::string url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url), base::string16::npos,
                          std::string(), GURL(url),
                          metrics::OmniboxEventProto::INVALID_SPEC, true, false,
                          true, true,
                          ChromeAutocompleteSchemeClassifier(&profile_));

  provider_->Start(input, false, true);

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

  std::string url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url), base::string16::npos,
                          std::string(), GURL(url),
                          metrics::OmniboxEventProto::INVALID_SPEC, true, false,
                          true, true,
                          ChromeAutocompleteSchemeClassifier(&profile_));

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = profile_.GetPrefs();
  prefs->SetString(prefs::kZeroSuggestCachedResults, json_response);

  provider_->Start(input, false, true);

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

  std::string url("http://www.cnn.com/");
  AutocompleteInput input(base::ASCIIToUTF16(url), base::string16::npos,
                          std::string(), GURL(url),
                          metrics::OmniboxEventProto::INVALID_SPEC, true, false,
                          true, true,
                          ChromeAutocompleteSchemeClassifier(&profile_));

  // Set up the pref to cache the response from the previous run.
  std::string json_response("[\"\",[\"search1\", \"search2\", \"search3\"],"
      "[],[],{\"google:suggestrelevance\":[602, 601, 600],"
      "\"google:verbatimrelevance\":1300}]");
  PrefService* prefs = profile_.GetPrefs();
  prefs->SetString(prefs::kZeroSuggestCachedResults, json_response);

  provider_->Start(input, false, true);

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
