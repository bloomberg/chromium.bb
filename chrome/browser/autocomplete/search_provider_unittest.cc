// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/search_provider.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/google/core/browser/google_switches.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/autocomplete_input.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/omnibox/autocomplete_provider.h"
#include "components/omnibox/autocomplete_provider_listener.h"
#include "components/omnibox/omnibox_field_trial.h"
#include "components/omnibox/omnibox_switches.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync_driver/pref_names.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace {

// Returns the first match in |matches| with |allowed_to_be_default_match|
// set to true.
ACMatches::const_iterator FindDefaultMatch(const ACMatches& matches) {
  ACMatches::const_iterator it = matches.begin();
  while ((it != matches.end()) && !it->allowed_to_be_default_match)
    ++it;
  return it;
}

class SuggestionDeletionHandler;
class SearchProviderForTest : public SearchProvider {
 public:
  SearchProviderForTest(AutocompleteProviderListener* listener,
                        TemplateURLService* template_url_service,
                        Profile* profile);
  bool is_success() { return is_success_; };

 protected:
  virtual ~SearchProviderForTest();

 private:
  virtual void RecordDeletionResult(bool success) OVERRIDE;
  bool is_success_;
  DISALLOW_COPY_AND_ASSIGN(SearchProviderForTest);
};

SearchProviderForTest::SearchProviderForTest(
    AutocompleteProviderListener* listener,
    TemplateURLService* template_url_service,
    Profile* profile)
    : SearchProvider(listener, template_url_service, profile),
      is_success_(false) {
}

SearchProviderForTest::~SearchProviderForTest() {
}

void SearchProviderForTest::RecordDeletionResult(bool success) {
  is_success_ = success;
}

} // namespace

// SearchProviderTest ---------------------------------------------------------

// The following environment is configured for these tests:
// . The TemplateURL default_t_url_ is set as the default provider.
// . The TemplateURL keyword_t_url_ is added to the TemplateURLService. This
//   TemplateURL has a valid suggest and search URL.
// . The URL created by using the search term term1_ with default_t_url_ is
//   added to history.
// . The URL created by using the search term keyword_term_ with keyword_t_url_
//   is added to history.
// . test_factory_ is set as the URLFetcherFactory.
class SearchProviderTest : public testing::Test,
                           public AutocompleteProviderListener {
 public:
  struct ResultInfo {
    ResultInfo() : result_type(AutocompleteMatchType::NUM_TYPES),
                   allowed_to_be_default_match(false) {
    }
    ResultInfo(GURL gurl,
               AutocompleteMatch::Type result_type,
               bool allowed_to_be_default_match,
               base::string16 fill_into_edit)
      : gurl(gurl),
        result_type(result_type),
        allowed_to_be_default_match(allowed_to_be_default_match),
        fill_into_edit(fill_into_edit) {
    }

    const GURL gurl;
    const AutocompleteMatch::Type result_type;
    const bool allowed_to_be_default_match;
    const base::string16 fill_into_edit;
  };

  struct TestData {
    const base::string16 input;
    const size_t num_results;
    const ResultInfo output[3];
  };

  SearchProviderTest()
      : default_t_url_(NULL),
        term1_(ASCIIToUTF16("term1")),
        keyword_t_url_(NULL),
        keyword_term_(ASCIIToUTF16("keyword")),
        run_loop_(NULL) {
    ResetFieldTrialList();
  }

  // See description above class for what this registers.
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void RunTest(TestData* cases, int num_cases, bool prefer_keyword);

 protected:
  // Needed for AutocompleteFieldTrial::ActivateStaticTrials();
  scoped_ptr<base::FieldTrialList> field_trial_list_;

  // Default value used for testing.
  static const std::string kNotApplicable;

  // Adds a search for |term|, using the engine |t_url| to the history, and
  // returns the URL for that search.
  GURL AddSearchToHistory(TemplateURL* t_url, base::string16 term, int visit_count);

  // Looks for a match in |provider_| with |contents| equal to |contents|.
  // Sets |match| to it if found.  Returns whether |match| was set.
  bool FindMatchWithContents(const base::string16& contents,
                             AutocompleteMatch* match);

  // Looks for a match in |provider_| with destination |url|.  Sets |match| to
  // it if found.  Returns whether |match| was set.
  bool FindMatchWithDestination(const GURL& url, AutocompleteMatch* match);

  // AutocompleteProviderListener:
  // If we're waiting for the provider to finish, this exits the message loop.
  virtual void OnProviderUpdate(bool updated_matches) OVERRIDE;

  // Runs a nested message loop until provider_ is done. The message loop is
  // exited by way of OnProviderUpdate.
  void RunTillProviderDone();

  // Invokes Start on provider_, then runs all pending tasks.
  void QueryForInput(const base::string16& text,
                     bool prevent_inline_autocomplete,
                     bool prefer_keyword);

  // Calls QueryForInput(), finishes any suggest query, then if |wyt_match| is
  // non-NULL, sets it to the "what you typed" entry for |text|.
  void QueryForInputAndSetWYTMatch(const base::string16& text,
                                   AutocompleteMatch* wyt_match);

  // Notifies the URLFetcher for the suggest query corresponding to the default
  // search provider that it's done.
  // Be sure and wrap calls to this in ASSERT_NO_FATAL_FAILURE.
  void FinishDefaultSuggestQuery();

  // Runs SearchProvider on |input|, for which the suggest server replies
  // with |json|, and expects that the resulting matches' contents equals
  // that in |matches|.  An empty entry in |matches| means no match should
  // be returned in that position.  Reports any errors with a message that
  // includes |error_description|.
  void ForcedQueryTestHelper(const std::string& input,
                             const std::string& json,
                             const std::string matches[3],
                             const std::string& error_description);

  void ResetFieldTrialList();

  // Create a field trial, with ZeroSuggest activation based on |enabled|.
  base::FieldTrial* CreateZeroSuggestFieldTrial(bool enabled);

  void ClearAllResults();

  // See description above class for details of these fields.
  TemplateURL* default_t_url_;
  const base::string16 term1_;
  GURL term1_url_;
  TemplateURL* keyword_t_url_;
  const base::string16 keyword_term_;
  GURL keyword_url_;

  content::TestBrowserThreadBundle thread_bundle_;

  // URLFetcherFactory implementation registered.
  net::TestURLFetcherFactory test_factory_;

  // Profile we use.
  TestingProfile profile_;

  // The provider.
  scoped_refptr<SearchProviderForTest> provider_;

  // If non-NULL, OnProviderUpdate quits the current |run_loop_|.
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderTest);
};

// static
const std::string SearchProviderTest::kNotApplicable = "Not Applicable";

void SearchProviderTest::SetUp() {
  // Make sure that fetchers are automatically ungregistered upon destruction.
  test_factory_.set_remove_fetcher_on_delete(true);

  // We need both the history service and template url model loaded.
  ASSERT_TRUE(profile_.CreateHistoryService(true, false));
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &TemplateURLServiceFactory::BuildInstanceFor);

  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(&profile_);

  turl_model->Load();

  // Reset the default TemplateURL.
  TemplateURLData data;
  data.short_name = ASCIIToUTF16("t");
  data.SetURL("http://defaultturl/{searchTerms}");
  data.suggestions_url = "http://defaultturl2/{searchTerms}";
  data.instant_url = "http://does/not/exist?strk=1";
  data.search_terms_replacement_key = "strk";
  default_t_url_ = new TemplateURL(data);
  turl_model->Add(default_t_url_);
  turl_model->SetUserSelectedDefaultSearchProvider(default_t_url_);
  TemplateURLID default_provider_id = default_t_url_->id();
  ASSERT_NE(0, default_provider_id);

  // Add url1, with search term term1_.
  term1_url_ = AddSearchToHistory(default_t_url_, term1_, 1);

  // Create another TemplateURL.
  data.short_name = ASCIIToUTF16("k");
  data.SetKeyword(ASCIIToUTF16("k"));
  data.SetURL("http://keyword/{searchTerms}");
  data.suggestions_url = "http://suggest_keyword/{searchTerms}";
  keyword_t_url_ = new TemplateURL(data);
  turl_model->Add(keyword_t_url_);
  ASSERT_NE(0, keyword_t_url_->id());

  // Add a page and search term for keyword_t_url_.
  keyword_url_ = AddSearchToHistory(keyword_t_url_, keyword_term_, 1);

  // Keywords are updated by the InMemoryHistoryBackend only after the message
  // has been processed on the history thread. Block until history processes all
  // requests to ensure the InMemoryDatabase is the state we expect it.
  profile_.BlockUntilHistoryProcessesPendingRequests();

  provider_ = new SearchProviderForTest(this, turl_model, &profile_);
  provider_->kMinimumTimeBetweenSuggestQueriesMs = 0;
}

void SearchProviderTest::TearDown() {
  base::RunLoop().RunUntilIdle();

  // Shutdown the provider before the profile.
  provider_ = NULL;
}

void SearchProviderTest::RunTest(TestData* cases,
                                 int num_cases,
                                 bool prefer_keyword) {
  ACMatches matches;
  for (int i = 0; i < num_cases; ++i) {
    AutocompleteInput input(cases[i].input, base::string16::npos,
                            base::string16(), GURL(),
                            metrics::OmniboxEventProto::INVALID_SPEC, false,
                            prefer_keyword, true, true,
                            ChromeAutocompleteSchemeClassifier(&profile_));
    provider_->Start(input, false);
    matches = provider_->matches();
    base::string16 diagnostic_details =
        ASCIIToUTF16("Input was: ") +
        cases[i].input +
        ASCIIToUTF16("; prefer_keyword was: ") +
        (prefer_keyword ? ASCIIToUTF16("true") : ASCIIToUTF16("false"));
    EXPECT_EQ(cases[i].num_results, matches.size()) << diagnostic_details;
    if (matches.size() == cases[i].num_results) {
      for (size_t j = 0; j < cases[i].num_results; ++j) {
        EXPECT_EQ(cases[i].output[j].gurl, matches[j].destination_url) <<
            diagnostic_details;
        EXPECT_EQ(cases[i].output[j].result_type, matches[j].type) <<
            diagnostic_details;
        EXPECT_EQ(cases[i].output[j].fill_into_edit,
                  matches[j].fill_into_edit) << diagnostic_details;
        EXPECT_EQ(cases[i].output[j].allowed_to_be_default_match,
                  matches[j].allowed_to_be_default_match) << diagnostic_details;
      }
    }
  }
}

void SearchProviderTest::OnProviderUpdate(bool updated_matches) {
  if (run_loop_ && provider_->done()) {
    run_loop_->Quit();
    run_loop_ = NULL;
  }
}

void SearchProviderTest::RunTillProviderDone() {
  if (provider_->done())
    return;

  base::RunLoop run_loop;
  run_loop_ = &run_loop;
  run_loop.Run();
}

void SearchProviderTest::QueryForInput(const base::string16& text,
                                       bool prevent_inline_autocomplete,
                                       bool prefer_keyword) {
  // Start a query.
  AutocompleteInput input(text, base::string16::npos, base::string16(), GURL(),
                          metrics::OmniboxEventProto::INVALID_SPEC,
                          prevent_inline_autocomplete, prefer_keyword, true,
                          true, ChromeAutocompleteSchemeClassifier(&profile_));
  provider_->Start(input, false);

  // RunUntilIdle so that the task scheduled by SearchProvider to create the
  // URLFetchers runs.
  base::RunLoop().RunUntilIdle();
}

void SearchProviderTest::QueryForInputAndSetWYTMatch(
    const base::string16& text,
    AutocompleteMatch* wyt_match) {
  QueryForInput(text, false, false);
  profile_.BlockUntilHistoryProcessesPendingRequests();
  ASSERT_NO_FATAL_FAILURE(FinishDefaultSuggestQuery());
  if (!wyt_match)
    return;
  ASSERT_GE(provider_->matches().size(), 1u);
  EXPECT_TRUE(FindMatchWithDestination(
      GURL(default_t_url_->url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(base::CollapseWhitespace(
              text, false)),
          TemplateURLServiceFactory::GetForProfile(
              &profile_)->search_terms_data())),
      wyt_match));
}

GURL SearchProviderTest::AddSearchToHistory(TemplateURL* t_url,
                                            base::string16 term,
                                            int visit_count) {
  HistoryService* history =
      HistoryServiceFactory::GetForProfile(&profile_,
                                           Profile::EXPLICIT_ACCESS);
  GURL search(t_url->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(term),
      TemplateURLServiceFactory::GetForProfile(
          &profile_)->search_terms_data()));
  static base::Time last_added_time;
  last_added_time = std::max(base::Time::Now(),
      last_added_time + base::TimeDelta::FromMicroseconds(1));
  history->AddPageWithDetails(search, base::string16(), visit_count, visit_count,
      last_added_time, false, history::SOURCE_BROWSED);
  history->SetKeywordSearchTermsForURL(search, t_url->id(), term);
  return search;
}

bool SearchProviderTest::FindMatchWithContents(const base::string16& contents,
                                               AutocompleteMatch* match) {
  for (ACMatches::const_iterator i = provider_->matches().begin();
       i != provider_->matches().end(); ++i) {
    if (i->contents == contents) {
      *match = *i;
      return true;
    }
  }
  return false;
}

bool SearchProviderTest::FindMatchWithDestination(const GURL& url,
                                                  AutocompleteMatch* match) {
  for (ACMatches::const_iterator i = provider_->matches().begin();
       i != provider_->matches().end(); ++i) {
    if (i->destination_url == url) {
      *match = *i;
      return true;
    }
  }
  return false;
}

void SearchProviderTest::FinishDefaultSuggestQuery() {
  net::TestURLFetcher* default_fetcher =
      test_factory_.GetFetcherByID(
          SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(default_fetcher);

  // Tell the SearchProvider the default suggest query is done.
  default_fetcher->set_response_code(200);
  default_fetcher->delegate()->OnURLFetchComplete(default_fetcher);
}

void SearchProviderTest::ForcedQueryTestHelper(
    const std::string& input,
    const std::string& json,
    const std::string expected_matches[3],
    const std::string& error_description) {
  QueryForInput(ASCIIToUTF16(input), false, false);
  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(200);
  fetcher->SetResponseString(json);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  RunTillProviderDone();

  const ACMatches& matches = provider_->matches();
  ASSERT_LE(matches.size(), 3u);
  size_t i = 0;
  // Ensure that the returned matches equal the expectations.
  for (; i < matches.size(); ++i) {
    EXPECT_EQ(ASCIIToUTF16(expected_matches[i]), matches[i].contents) <<
        error_description;
  }
  // Ensure that no expected matches are missing.
  for (; i < 3u; ++i) {
    EXPECT_EQ(std::string(), expected_matches[i]) <<
        "Case #" << i << ": " << error_description;
  }
}

void SearchProviderTest::ResetFieldTrialList() {
  // Destroy the existing FieldTrialList before creating a new one to avoid
  // a DCHECK.
  field_trial_list_.reset();
  field_trial_list_.reset(new base::FieldTrialList(
      new metrics::SHA1EntropyProvider("foo")));
  variations::testing::ClearAllVariationParams();
  base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
      "AutocompleteDynamicTrial_0", "DefaultGroup");
  trial->group();
}

base::FieldTrial* SearchProviderTest::CreateZeroSuggestFieldTrial(
    bool enabled) {
  std::map<std::string, std::string> params;
  params[std::string(OmniboxFieldTrial::kZeroSuggestRule)] = enabled ?
      "true" : "false";
  variations::AssociateVariationParams(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params);
  return base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
}

void SearchProviderTest::ClearAllResults() {
  provider_->ClearAllResults();
}

// Actual Tests ---------------------------------------------------------------

// Make sure we query history for the default provider and a URLFetcher is
// created for the default provider suggest results.
TEST_F(SearchProviderTest, QueryDefaultProvider) {
  base::string16 term = term1_.substr(0, term1_.length() - 1);
  QueryForInput(term, false, false);

  // Make sure the default providers suggest service was queried.
  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(fetcher);

  // And the URL matches what we expected.
  GURL expected_url(default_t_url_->suggestions_url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(term),
      TemplateURLServiceFactory::GetForProfile(
          &profile_)->search_terms_data()));
  ASSERT_TRUE(fetcher->GetOriginalURL() == expected_url);

  // Tell the SearchProvider the suggest query is done.
  fetcher->set_response_code(200);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  fetcher = NULL;

  // Run till the history results complete.
  RunTillProviderDone();

  // The SearchProvider is done. Make sure it has a result for the history
  // term term1.
  AutocompleteMatch term1_match;
  EXPECT_TRUE(FindMatchWithDestination(term1_url_, &term1_match));
  // Term1 should not have a description, it's set later.
  EXPECT_TRUE(term1_match.description.empty());

  AutocompleteMatch wyt_match;
  EXPECT_TRUE(FindMatchWithDestination(
      GURL(default_t_url_->url_ref().ReplaceSearchTerms(
          TemplateURLRef::SearchTermsArgs(term),
          TemplateURLServiceFactory::GetForProfile(
              &profile_)->search_terms_data())),
      &wyt_match));
  EXPECT_TRUE(wyt_match.description.empty());

  // The match for term1 should be more relevant than the what you typed match.
  EXPECT_GT(term1_match.relevance, wyt_match.relevance);
  // This longer match should be inlineable.
  EXPECT_TRUE(term1_match.allowed_to_be_default_match);
  // The what you typed match should be too, of course.
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

TEST_F(SearchProviderTest, HonorPreventInlineAutocomplete) {
  base::string16 term = term1_.substr(0, term1_.length() - 1);
  QueryForInput(term, true, false);

  ASSERT_FALSE(provider_->matches().empty());
  ASSERT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            provider_->matches()[0].type);
  EXPECT_TRUE(provider_->matches()[0].allowed_to_be_default_match);
}

// Issues a query that matches the registered keyword and makes sure history
// is queried as well as URLFetchers getting created.
TEST_F(SearchProviderTest, QueryKeywordProvider) {
  base::string16 term = keyword_term_.substr(0, keyword_term_.length() - 1);
  QueryForInput(keyword_t_url_->keyword() + ASCIIToUTF16(" ") + term,
                false,
                false);

  // Make sure the default providers suggest service was queried.
  net::TestURLFetcher* default_fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(default_fetcher);

  // Tell the SearchProvider the default suggest query is done.
  default_fetcher->set_response_code(200);
  default_fetcher->delegate()->OnURLFetchComplete(default_fetcher);
  default_fetcher = NULL;

  // Make sure the keyword providers suggest service was queried.
  net::TestURLFetcher* keyword_fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kKeywordProviderURLFetcherID);
  ASSERT_TRUE(keyword_fetcher);

  // And the URL matches what we expected.
  GURL expected_url(keyword_t_url_->suggestions_url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(term),
      TemplateURLServiceFactory::GetForProfile(
          &profile_)->search_terms_data()));
  ASSERT_TRUE(keyword_fetcher->GetOriginalURL() == expected_url);

  // Tell the SearchProvider the keyword suggest query is done.
  keyword_fetcher->set_response_code(200);
  keyword_fetcher->delegate()->OnURLFetchComplete(keyword_fetcher);
  keyword_fetcher = NULL;

  // Run till the history results complete.
  RunTillProviderDone();

  // The SearchProvider is done. Make sure it has a result for the history
  // term keyword.
  AutocompleteMatch match;
  EXPECT_TRUE(FindMatchWithDestination(keyword_url_, &match));

  // The match should have an associated keyword.
  EXPECT_FALSE(match.keyword.empty());

  // The fill into edit should contain the keyword.
  EXPECT_EQ(keyword_t_url_->keyword() + base::char16(' ') + keyword_term_,
            match.fill_into_edit);
}

TEST_F(SearchProviderTest, DontSendPrivateDataToSuggest) {
  // None of the following input strings should be sent to the suggest server,
  // because they may contain private data.
  const char* inputs[] = {
    "username:password",
    "http://username:password",
    "https://username:password",
    "username:password@hostname",
    "http://username:password@hostname/",
    "file://filename",
    "data://data",
    "unknownscheme:anything",
    "http://hostname/?query=q",
    "http://hostname/path#ref",
    "http://hostname/path #ref",
    "https://hostname/path",
  };

  for (size_t i = 0; i < arraysize(inputs); ++i) {
    QueryForInput(ASCIIToUTF16(inputs[i]), false, false);
    // Make sure the default provider's suggest service was not queried.
    ASSERT_TRUE(test_factory_.GetFetcherByID(
        SearchProvider::kDefaultProviderURLFetcherID) == NULL);
    // Run till the history results complete.
    RunTillProviderDone();
  }
}

TEST_F(SearchProviderTest, SendNonPrivateDataToSuggest) {
  // All of the following input strings should be sent to the suggest server,
  // because they should not get caught by the private data checks.
  const char* inputs[] = {
    "query",
    "query with spaces",
    "http://hostname",
    "http://hostname/path",
    "http://hostname #ref",
    "www.hostname.com #ref",
    "https://hostname",
    "#hashtag",
    "foo https://hostname/path"
  };

  profile_.BlockUntilHistoryProcessesPendingRequests();
  for (size_t i = 0; i < arraysize(inputs); ++i) {
    QueryForInput(ASCIIToUTF16(inputs[i]), false, false);
    // Make sure the default provider's suggest service was queried.
    ASSERT_TRUE(test_factory_.GetFetcherByID(
        SearchProvider::kDefaultProviderURLFetcherID) != NULL);
  }
}

TEST_F(SearchProviderTest, DontAutocompleteURLLikeTerms) {
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &AutocompleteClassifierFactory::BuildInstanceFor);
  GURL url = AddSearchToHistory(default_t_url_,
                                ASCIIToUTF16("docs.google.com"), 1);

  // Add the term as a url.
  HistoryServiceFactory::GetForProfile(&profile_, Profile::EXPLICIT_ACCESS)->
      AddPageWithDetails(GURL("http://docs.google.com"), base::string16(), 1, 1,
                         base::Time::Now(), false, history::SOURCE_BROWSED);
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("docs"),
                                                      &wyt_match));

  // There should be two matches, one for what you typed, the other for
  // 'docs.google.com'. The search term should have a lower priority than the
  // what you typed match.
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(url, &term_match));
  EXPECT_GT(wyt_match.relevance, term_match.relevance);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
}

TEST_F(SearchProviderTest, DontGiveNavsuggestionsInForcedQueryMode) {
  const std::string kEmptyMatch;
  struct {
    const std::string json;
    const std::string matches_in_default_mode[3];
    const std::string matches_in_forced_query_mode[3];
  } cases[] = {
    // Without suggested relevance scores.
    { "[\"a\",[\"http://a1.com\", \"a2\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"QUERY\"]}]",
      { "a", "a1.com", "a2" },
      { "a", "a2", kEmptyMatch } },

    // With suggested relevance scores in a situation where navsuggest would
    // go second.
    { "[\"a\",[\"http://a1.com\", \"a2\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"QUERY\"],"
        "\"google:suggestrelevance\":[1250, 1200]}]",
      { "a", "a1.com", "a2" },
      { "a", "a2", kEmptyMatch } },

    // With suggested relevance scores in a situation where navsuggest
    // would go first.
    { "[\"a\",[\"http://a1.com\", \"a2\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"QUERY\"],"
        "\"google:suggestrelevance\":[1350, 1250]}]",
      { "a1.com", "a", "a2" },
      { "a", "a2", kEmptyMatch } },

    // With suggested relevance scores in a situation where navsuggest
    // would go first only because verbatim has been demoted.
    { "[\"a\",[\"http://a1.com\", \"a2\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"QUERY\"],"
        "\"google:suggestrelevance\":[1450, 1400],"
        "\"google:verbatimrelevance\":1350}]",
      { "a1.com", "a2", "a" },
      { "a2", "a", kEmptyMatch } },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    ForcedQueryTestHelper("a", cases[i].json, cases[i].matches_in_default_mode,
                           "regular input with json=" + cases[i].json);
    ForcedQueryTestHelper("?a", cases[i].json,
                          cases[i].matches_in_forced_query_mode,
                          "forced query input with json=" + cases[i].json);
  }
}

// A multiword search with one visit should not autocomplete until multiple
// words are typed.
TEST_F(SearchProviderTest, DontAutocompleteUntilMultipleWordsTyped) {
  GURL term_url(AddSearchToHistory(default_t_url_, ASCIIToUTF16("one search"),
                                   1));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("on"),
                                                      &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(wyt_match.relevance, term_match.relevance);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("one se"),
                                                      &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// A multiword search with more than one visit should autocomplete immediately.
TEST_F(SearchProviderTest, AutocompleteMultipleVisitsImmediately) {
  GURL term_url(AddSearchToHistory(default_t_url_, ASCIIToUTF16("two searches"),
                                   2));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("tw"),
                                                      &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Autocompletion should work at a word boundary after a space, and should
// offer a suggestion for the trimmed search query.
TEST_F(SearchProviderTest, AutocompleteAfterSpace) {
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("two  searches "), 2);
  GURL suggested_url(default_t_url_->url_ref().ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("two searches")),
      TemplateURLServiceFactory::GetForProfile(
          &profile_)->search_terms_data()));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("two "),
                                                      &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(suggested_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  EXPECT_EQ(ASCIIToUTF16("searches"), term_match.inline_autocompletion);
  EXPECT_EQ(ASCIIToUTF16("two searches"), term_match.fill_into_edit);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Newer multiword searches should score more highly than older ones.
TEST_F(SearchProviderTest, ScoreNewerSearchesHigher) {
  GURL term_url_a(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("three searches aaa"), 1));
  GURL term_url_b(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("three searches bbb"), 1));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("three se"),
                                                      &wyt_match));
  ASSERT_EQ(3u, provider_->matches().size());
  AutocompleteMatch term_match_a;
  EXPECT_TRUE(FindMatchWithDestination(term_url_a, &term_match_a));
  AutocompleteMatch term_match_b;
  EXPECT_TRUE(FindMatchWithDestination(term_url_b, &term_match_b));
  EXPECT_GT(term_match_b.relevance, term_match_a.relevance);
  EXPECT_GT(term_match_a.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match_b.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_a.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// An autocompleted multiword search should not be replaced by a different
// autocompletion while the user is still typing a valid prefix unless the
// user has typed the prefix as a query before.
TEST_F(SearchProviderTest, DontReplacePreviousAutocompletion) {
  GURL term_url_a(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("four searches aaa"), 3));
  GURL term_url_b(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("four searches bbb"), 1));
  GURL term_url_c(AddSearchToHistory(default_t_url_,
                                     ASCIIToUTF16("four searches"), 1));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("fo"),
                                                      &wyt_match));
  ASSERT_EQ(4u, provider_->matches().size());
  AutocompleteMatch term_match_a;
  EXPECT_TRUE(FindMatchWithDestination(term_url_a, &term_match_a));
  AutocompleteMatch term_match_b;
  EXPECT_TRUE(FindMatchWithDestination(term_url_b, &term_match_b));
  AutocompleteMatch term_match_c;
  EXPECT_TRUE(FindMatchWithDestination(term_url_c, &term_match_c));
  EXPECT_GT(term_match_a.relevance, wyt_match.relevance);
  // We don't care about the relative order of b and c.
  EXPECT_GT(wyt_match.relevance, term_match_b.relevance);
  EXPECT_GT(wyt_match.relevance, term_match_c.relevance);
  EXPECT_TRUE(term_match_a.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_b.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_c.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);

  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("four se"),
                                                      &wyt_match));
  ASSERT_EQ(4u, provider_->matches().size());
  EXPECT_TRUE(FindMatchWithDestination(term_url_a, &term_match_a));
  EXPECT_TRUE(FindMatchWithDestination(term_url_b, &term_match_b));
  EXPECT_TRUE(FindMatchWithDestination(term_url_c, &term_match_c));
  EXPECT_GT(term_match_a.relevance, wyt_match.relevance);
  EXPECT_GT(wyt_match.relevance, term_match_b.relevance);
  EXPECT_GT(wyt_match.relevance, term_match_c.relevance);
  EXPECT_TRUE(term_match_a.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_b.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_c.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);

  // For the exact previously-issued query, the what-you-typed match should win.
  ASSERT_NO_FATAL_FAILURE(
      QueryForInputAndSetWYTMatch(ASCIIToUTF16("four searches"), &wyt_match));
  ASSERT_EQ(3u, provider_->matches().size());
  EXPECT_TRUE(FindMatchWithDestination(term_url_a, &term_match_a));
  EXPECT_TRUE(FindMatchWithDestination(term_url_b, &term_match_b));
  EXPECT_GT(wyt_match.relevance, term_match_a.relevance);
  EXPECT_GT(wyt_match.relevance, term_match_b.relevance);
  EXPECT_TRUE(term_match_a.allowed_to_be_default_match);
  EXPECT_TRUE(term_match_b.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Non-completable multiword searches should not crowd out single-word searches.
TEST_F(SearchProviderTest, DontCrowdOutSingleWords) {
  GURL term_url(AddSearchToHistory(default_t_url_, ASCIIToUTF16("five"), 1));
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("five searches bbb"), 1);
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("five searches ccc"), 1);
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("five searches ddd"), 1);
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("five searches eee"), 1);
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("fi"),
                                                      &wyt_match));
  ASSERT_EQ(AutocompleteProvider::kMaxMatches + 1, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
  EXPECT_TRUE(wyt_match.allowed_to_be_default_match);
}

// Inline autocomplete matches regardless of case differences from the input.
TEST_F(SearchProviderTest, InlineMixedCaseMatches) {
  GURL term_url(AddSearchToHistory(default_t_url_, ASCIIToUTF16("FOO"), 1));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch wyt_match;
  ASSERT_NO_FATAL_FAILURE(QueryForInputAndSetWYTMatch(ASCIIToUTF16("f"),
                                                      &wyt_match));
  ASSERT_EQ(2u, provider_->matches().size());
  AutocompleteMatch term_match;
  EXPECT_TRUE(FindMatchWithDestination(term_url, &term_match));
  EXPECT_GT(term_match.relevance, wyt_match.relevance);
  EXPECT_EQ(ASCIIToUTF16("FOO"), term_match.fill_into_edit);
  EXPECT_EQ(ASCIIToUTF16("OO"), term_match.inline_autocompletion);
  EXPECT_TRUE(term_match.allowed_to_be_default_match);
}

// Verifies AutocompleteControllers return results (including keyword
// results) in the right order and set descriptions for them correctly.
TEST_F(SearchProviderTest, KeywordOrderingAndDescriptions) {
  // Add an entry that corresponds to a keyword search with 'term2'.
  AddSearchToHistory(keyword_t_url_, ASCIIToUTF16("term2"), 1);
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteController controller(&profile_,
      TemplateURLServiceFactory::GetForProfile(&profile_),
      NULL, AutocompleteProvider::TYPE_SEARCH);
  controller.Start(AutocompleteInput(
      ASCIIToUTF16("k t"), base::string16::npos, base::string16(), GURL(),
      metrics::OmniboxEventProto::INVALID_SPEC, false, false, true, true,
      ChromeAutocompleteSchemeClassifier(&profile_)));
  const AutocompleteResult& result = controller.result();

  // There should be three matches, one for the keyword history, one for
  // keyword provider's what-you-typed, and one for the default provider's
  // what you typed, in that order.
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_HISTORY, result.match_at(0).type);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_OTHER_ENGINE,
            result.match_at(1).type);
  EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            result.match_at(2).type);
  EXPECT_GT(result.match_at(0).relevance, result.match_at(1).relevance);
  EXPECT_GT(result.match_at(1).relevance, result.match_at(2).relevance);
  EXPECT_TRUE(result.match_at(0).allowed_to_be_default_match);
  EXPECT_TRUE(result.match_at(1).allowed_to_be_default_match);
  EXPECT_FALSE(result.match_at(2).allowed_to_be_default_match);

  // The two keyword results should come with the keyword we expect.
  EXPECT_EQ(ASCIIToUTF16("k"), result.match_at(0).keyword);
  EXPECT_EQ(ASCIIToUTF16("k"), result.match_at(1).keyword);
  // The default provider has a different keyword.  (We don't explicitly
  // set it during this test, so all we do is assert that it's different.)
  EXPECT_NE(result.match_at(0).keyword, result.match_at(2).keyword);

  // The top result will always have a description.  The third result,
  // coming from a different provider than the first two, should also.
  // Whether the second result has one doesn't matter much.  (If it was
  // missing, people would infer that it's the same search provider as
  // the one above it.)
  EXPECT_FALSE(result.match_at(0).description.empty());
  EXPECT_FALSE(result.match_at(2).description.empty());
  EXPECT_NE(result.match_at(0).description, result.match_at(2).description);
}

TEST_F(SearchProviderTest, KeywordVerbatim) {
  TestData cases[] = {
    // Test a simple keyword input.
    { ASCIIToUTF16("k foo"), 2,
      { ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo")),
        ResultInfo(GURL("http://defaultturl/k%20foo"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("k foo") ) } },

    // Make sure extra whitespace after the keyword doesn't change the
    // keyword verbatim query.  Also verify that interior consecutive
    // whitespace gets trimmed.
    { ASCIIToUTF16("k   foo"), 2,
      { ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo")),
        ResultInfo(GURL("http://defaultturl/k%20foo"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("k foo")) } },
    // Leading whitespace should be stripped before SearchProvider gets the
    // input; hence there are no tests here about how it handles those inputs.

    // Verify that interior consecutive whitespace gets trimmed in either case.
    { ASCIIToUTF16("k  foo  bar"), 2,
      { ResultInfo(GURL("http://keyword/foo%20bar"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo bar")),
        ResultInfo(GURL("http://defaultturl/k%20foo%20bar"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("k foo bar")) } },

    // Verify that trailing whitespace gets trimmed.
    { ASCIIToUTF16("k foo bar  "), 2,
      { ResultInfo(GURL("http://keyword/foo%20bar"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo bar")),
        ResultInfo(GURL("http://defaultturl/k%20foo%20bar"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("k foo bar")) } },

    // Keywords can be prefixed by certain things that should get ignored
    // when constructing the keyword match.
    { ASCIIToUTF16("www.k foo"), 2,
      { ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo")),
        ResultInfo(GURL("http://defaultturl/www.k%20foo"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("www.k foo")) } },
    { ASCIIToUTF16("http://k foo"), 2,
      { ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo")),
        ResultInfo(GURL("http://defaultturl/http%3A//k%20foo"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("http://k foo")) } },
    { ASCIIToUTF16("http://www.k foo"), 2,
      { ResultInfo(GURL("http://keyword/foo"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k foo")),
        ResultInfo(GURL("http://defaultturl/http%3A//www.k%20foo"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("http://www.k foo")) } },

    // A keyword with no remaining input shouldn't get a keyword
    // verbatim match.
    { ASCIIToUTF16("k"), 1,
      { ResultInfo(GURL("http://defaultturl/k"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   true,
                   ASCIIToUTF16("k")) } },
    // Ditto.  Trailing whitespace shouldn't make a difference.
    { ASCIIToUTF16("k "), 1,
      { ResultInfo(GURL("http://defaultturl/k"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   true,
                   ASCIIToUTF16("k")) } }

    // The fact that verbatim queries to keyword are handled by KeywordProvider
    // not SearchProvider is tested in
    // chrome/browser/extensions/api/omnibox/omnibox_apitest.cc.
  };

  // Test not in keyword mode.
  RunTest(cases, arraysize(cases), false);

  // Test in keyword mode.  (Both modes should give the same result.)
  RunTest(cases, arraysize(cases), true);
}

// Ensures command-line flags are reflected in the URLs the search provider
// generates.
TEST_F(SearchProviderTest, CommandLineOverrides) {
  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(&profile_);

  TemplateURLData data;
  data.short_name = ASCIIToUTF16("default");
  data.SetKeyword(data.short_name);
  data.SetURL("{google:baseURL}{searchTerms}");
  default_t_url_ = new TemplateURL(data);
  turl_model->Add(default_t_url_);
  turl_model->SetUserSelectedDefaultSearchProvider(default_t_url_);

  CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kGoogleBaseURL,
                                                      "http://www.bar.com/");
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kExtraSearchQueryParams, "a=b");

  TestData cases[] = {
    { ASCIIToUTF16("k a"), 2,
      { ResultInfo(GURL("http://keyword/a"),
                   AutocompleteMatchType::SEARCH_OTHER_ENGINE,
                   true,
                   ASCIIToUTF16("k a")),
        ResultInfo(GURL("http://www.bar.com/k%20a?a=b"),
                   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                   false,
                   ASCIIToUTF16("k a")) } },
  };

  RunTest(cases, arraysize(cases), false);
}

// Verifies Navsuggest results don't set a TemplateURL, which Instant relies on.
// Also verifies that just the *first* navigational result is listed as a match
// if suggested relevance scores were not sent.
TEST_F(SearchProviderTest, NavSuggestNoSuggestedRelevanceScores) {
  QueryForInput(ASCIIToUTF16("a.c"), false, false);

  // Make sure the default providers suggest service was queried.
  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(fetcher);

  // Tell the SearchProvider the suggest query is done.
  fetcher->set_response_code(200);
  fetcher->SetResponseString(
      "[\"a.c\",[\"a.com\", \"a.com/b\"],[\"a\", \"b\"],[],"
      "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"]}]");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  fetcher = NULL;

  // Run till the history results complete.
  RunTillProviderDone();

  // Make sure the only match is 'a.com' and it doesn't have a template_url.
  AutocompleteMatch nav_match;
  EXPECT_TRUE(FindMatchWithDestination(GURL("http://a.com"), &nav_match));
  EXPECT_TRUE(nav_match.keyword.empty());
  EXPECT_TRUE(nav_match.allowed_to_be_default_match);
  EXPECT_FALSE(FindMatchWithDestination(GURL("http://a.com/b"), &nav_match));
}

// Verifies that the most relevant suggest results are added properly.
TEST_F(SearchProviderTest, SuggestRelevance) {
  QueryForInput(ASCIIToUTF16("a"), false, false);

  // Make sure the default provider's suggest service was queried.
  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(fetcher);

  // Tell the SearchProvider the suggest query is done.
  fetcher->set_response_code(200);
  fetcher->SetResponseString("[\"a\",[\"a1\", \"a2\", \"a3\", \"a4\"]]");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  fetcher = NULL;

  // Run till the history results complete.
  RunTillProviderDone();

  // Check the expected verbatim and (first 3) suggestions' relative relevances.
  AutocompleteMatch verbatim, match_a1, match_a2, match_a3, match_a4;
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("a"), &verbatim));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("a1"), &match_a1));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("a2"), &match_a2));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("a3"), &match_a3));
  EXPECT_FALSE(FindMatchWithContents(ASCIIToUTF16("a4"), &match_a4));
  EXPECT_GT(verbatim.relevance, match_a1.relevance);
  EXPECT_GT(match_a1.relevance, match_a2.relevance);
  EXPECT_GT(match_a2.relevance, match_a3.relevance);
  EXPECT_TRUE(verbatim.allowed_to_be_default_match);
  EXPECT_TRUE(match_a1.allowed_to_be_default_match);
  EXPECT_TRUE(match_a2.allowed_to_be_default_match);
  EXPECT_TRUE(match_a3.allowed_to_be_default_match);
}

// Verifies that the default provider abandons suggested relevance scores
// when in keyword mode.  This should happen regardless of whether the
// keyword provider returns suggested relevance scores.
TEST_F(SearchProviderTest, DefaultProviderNoSuggestRelevanceInKeywordMode) {
  struct {
    const std::string default_provider_json;
    const std::string keyword_provider_json;
    const std::string matches[5];
  } cases[] = {
    // First, try an input where the keyword provider does not deliver
    // suggested relevance scores.
    { "[\"k a\",[\"k adefault-query\", \"adefault.com\"],[],[],"
      "{\"google:verbatimrelevance\":9700,"
      "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
      "\"google:suggestrelevance\":[9900, 9800]}]",
      "[\"a\",[\"akeyword-query\"],[],[],{\"google:suggesttype\":[\"QUERY\"]}]",
      { "a", "akeyword-query", "k a", "adefault.com", "k adefault-query" } },

    // Now try with keyword provider suggested relevance scores.
    { "[\"k a\",[\"k adefault-query\", \"adefault.com\"],[],[],"
      "{\"google:verbatimrelevance\":9700,"
      "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
      "\"google:suggestrelevance\":[9900, 9800]}]",
      "[\"a\",[\"akeyword-query\"],[],[],{\"google:suggesttype\":[\"QUERY\"],"
      "\"google:verbatimrelevance\":9500,"
      "\"google:suggestrelevance\":[9600]}]",
      { "akeyword-query", "a", "k a", "adefault.com", "k adefault-query" } }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    QueryForInput(ASCIIToUTF16("k a"), false, true);
    net::TestURLFetcher* default_fetcher =
        test_factory_.GetFetcherByID(
            SearchProvider::kDefaultProviderURLFetcherID);
    ASSERT_TRUE(default_fetcher);
    default_fetcher->set_response_code(200);
    default_fetcher->SetResponseString(cases[i].default_provider_json);
    default_fetcher->delegate()->OnURLFetchComplete(default_fetcher);
    net::TestURLFetcher* keyword_fetcher =
        test_factory_.GetFetcherByID(
            SearchProvider::kKeywordProviderURLFetcherID);
    ASSERT_TRUE(keyword_fetcher);
    keyword_fetcher->set_response_code(200);
    keyword_fetcher->SetResponseString(cases[i].keyword_provider_json);
    keyword_fetcher->delegate()->OnURLFetchComplete(keyword_fetcher);
    RunTillProviderDone();

    const std::string description = "for input with default_provider_json=" +
        cases[i].default_provider_json + " and keyword_provider_json=" +
        cases[i].keyword_provider_json;
    const ACMatches& matches = provider_->matches();
    ASSERT_LE(matches.size(), ARRAYSIZE_UNSAFE(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      EXPECT_EQ(ASCIIToUTF16(cases[i].matches[j]), matches[j].contents) <<
          description;
    }
    // Ensure that no expected matches are missing.
    for (; j < ARRAYSIZE_UNSAFE(cases[i].matches); ++j)
      EXPECT_EQ(std::string(), cases[i].matches[j]) << description;
  }
}

// Verifies that suggest results with relevance scores are added
// properly when using the default fetcher.  When adding a new test
// case to this test, please consider adding it to the tests in
// KeywordFetcherSuggestRelevance below.
TEST_F(SearchProviderTest, DefaultFetcherSuggestRelevance) {
  struct DefaultFetcherMatch {
    std::string contents;
    bool allowed_to_be_default_match;
  };
  const DefaultFetcherMatch kEmptyMatch = { kNotApplicable, false };
  struct {
    const std::string json;
    const DefaultFetcherMatch matches[6];
    const std::string inline_autocompletion;
  } cases[] = {
    // Ensure that suggestrelevance scores reorder matches.
    { "[\"a\",[\"b\", \"c\"],[],[],{\"google:suggestrelevance\":[1, 2]}]",
      { { "a", true }, { "c", false }, { "b", false }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\", \"http://c.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[1, 2]}]",
      { { "a", true }, { "c.com", false }, { "b.com", false }, kEmptyMatch,
        kEmptyMatch, kEmptyMatch },
      std::string() },

    // Without suggested relevance scores, we should only allow one
    // navsuggest result to be be displayed.
    { "[\"a\",[\"http://b.com\", \"http://c.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"]}]",
      { { "a", true }, { "b.com", false }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch, kEmptyMatch },
      std::string() },

    // Ensure that verbatimrelevance scores reorder or suppress verbatim.
    // Negative values will have no effect; the calculated value will be used.
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":9999,"
                             "\"google:suggestrelevance\":[9998]}]",
      { { "a", true}, { "a1", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      std::string() },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":9998,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1", true }, { "a", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":0,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":-1,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1", true }, { "a", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      "1" },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9999,"
        "\"google:suggestrelevance\":[9998]}]",
      { { "a", true }, { "a.com", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9998,"
        "\"google:suggestrelevance\":[9999]}]",
      { { "a.com", true }, { "a", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      ".com" },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9999]}]",
      { { "a.com", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      ".com" },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:verbatimrelevance\":-1,"
        "\"google:suggestrelevance\":[9999]}]",
      { { "a.com", true }, { "a", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      ".com" },

    // Ensure that both types of relevance scores reorder matches together.
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[9999, 9997],"
                                     "\"google:verbatimrelevance\":9998}]",
      { { "a1", true }, { "a", true }, { "a2", true }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      "1" },

    // Allow non-inlineable matches to be the highest-scoring match but,
    // if the result set lacks a single inlineable result, abandon suggested
    // relevance scores entirely.
    { "[\"a\",[\"b\"],[],[],{\"google:suggestrelevance\":[9999]}]",
      { { "b", false }, { "a", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      std::string() },
    { "[\"a\",[\"b\"],[],[],{\"google:suggestrelevance\":[9999],"
                            "\"google:verbatimrelevance\":0}]",
      { { "a", true }, { "b", false }, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999]}]",
      { { "b.com", false }, { "a", true }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999],"
        "\"google:verbatimrelevance\":0}]",
      { { "a", true }, { "b.com", false }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch, kEmptyMatch },
      std::string() },

    // Allow low-scoring matches.
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":0}]",
      { { "a1", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":1}]",
      { { "a1", true }, { "a", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:suggestrelevance\":[1],"
                             "\"google:verbatimrelevance\":0}]",
      { { "a1", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[1, 2],"
                                     "\"google:verbatimrelevance\":0}]",
      { { "a2", true }, { "a1", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      "2" },
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[1, 3],"
      "\"google:verbatimrelevance\":2}]",
      { { "a2", true }, { "a", true }, { "a1", true }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      "2" },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[1],"
        "\"google:verbatimrelevance\":0}]",
      { { "a.com", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      ".com" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[1, 2],"
        "\"google:verbatimrelevance\":0}]",
      { { "a2.com", true }, { "a1.com", true }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch, kEmptyMatch },
      "2.com" },

    // Ensure that all suggestions are considered, regardless of order.
    { "[\"a\",[\"b\", \"c\", \"d\", \"e\", \"f\", \"g\", \"h\"],[],[],"
       "{\"google:suggestrelevance\":[1, 2, 3, 4, 5, 6, 7]}]",
      { { "a", true }, { "h", false }, { "g", false }, { "f", false },
        { "e", false }, { "d", false } },
      std::string() },
    { "[\"a\",[\"http://b.com\", \"http://c.com\", \"http://d.com\","
              "\"http://e.com\", \"http://f.com\", \"http://g.com\","
              "\"http://h.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[1, 2, 3, 4, 5, 6, 7]}]",
      { { "a", true }, { "h.com", false }, { "g.com", false },
        { "f.com", false }, { "e.com", false }, { "d.com", false } },
      std::string() },

    // Ensure that incorrectly sized suggestion relevance lists are ignored.
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[1]}]",
      { { "a", true }, { "a1", true }, { "a2", true }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      std::string() },
    { "[\"a\",[\"a1\"],[],[],{\"google:suggestrelevance\":[9999, 1]}]",
      { { "a", true }, { "a1", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[1]}]",
      { { "a", true }, { "a1.com", true }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
       "\"google:suggestrelevance\":[9999, 1]}]",
      { { "a", true }, { "a1.com", true }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch, kEmptyMatch },
      std::string() },

    // Ensure that all 'verbatim' results are merged with their maximum score.
    { "[\"a\",[\"a\", \"a1\", \"a2\"],[],[],"
       "{\"google:suggestrelevance\":[9998, 9997, 9999]}]",
      { { "a2", true }, { "a", true }, { "a1", true }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      "2" },
    { "[\"a\",[\"a\", \"a1\", \"a2\"],[],[],"
       "{\"google:suggestrelevance\":[9998, 9997, 9999],"
        "\"google:verbatimrelevance\":0}]",
      { { "a2", true }, { "a", true }, { "a1", true }, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      "2" },

    // Ensure that verbatim is always generated without other suggestions.
    // TODO(msw): Ensure verbatimrelevance is respected (except suppression).
    { "[\"a\",[],[],[],{\"google:verbatimrelevance\":1}]",
      { { "a", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      std::string() },
    { "[\"a\",[],[],[],{\"google:verbatimrelevance\":0}]",
      { { "a", true }, kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch,
        kEmptyMatch },
      std::string() },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    QueryForInput(ASCIIToUTF16("a"), false, false);
    net::TestURLFetcher* fetcher =
        test_factory_.GetFetcherByID(
            SearchProvider::kDefaultProviderURLFetcherID);
    ASSERT_TRUE(fetcher);
    fetcher->set_response_code(200);
    fetcher->SetResponseString(cases[i].json);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    RunTillProviderDone();

    const std::string description = "for input with json=" + cases[i].json;
    const ACMatches& matches = provider_->matches();
    ASSERT_FALSE(matches.empty());
    // Find the first match that's allowed to be the default match and check
    // its inline_autocompletion.
    ACMatches::const_iterator it = FindDefaultMatch(matches);
    ASSERT_NE(matches.end(), it);
    EXPECT_EQ(ASCIIToUTF16(cases[i].inline_autocompletion),
              it->inline_autocompletion) << description;

    ASSERT_LE(matches.size(), ARRAYSIZE_UNSAFE(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      EXPECT_EQ(ASCIIToUTF16(cases[i].matches[j].contents),
                matches[j].contents) << description;
      EXPECT_EQ(cases[i].matches[j].allowed_to_be_default_match,
                matches[j].allowed_to_be_default_match) << description;
    }
    // Ensure that no expected matches are missing.
    for (; j < ARRAYSIZE_UNSAFE(cases[i].matches); ++j)
      EXPECT_EQ(kNotApplicable, cases[i].matches[j].contents) <<
          "Case # " << i << " " << description;
  }
}

// Verifies that suggest results with relevance scores are added
// properly when using the keyword fetcher.  This is similar to the
// test DefaultFetcherSuggestRelevance above but this uses inputs that
// trigger keyword suggestions (i.e., "k a" rather than "a") and has
// different expectations (because now the results are a mix of
// keyword suggestions and default provider suggestions).  When a new
// test is added to this TEST_F, please consider if it would be
// appropriate to add to DefaultFetcherSuggestRelevance as well.
TEST_F(SearchProviderTest, KeywordFetcherSuggestRelevance) {
  struct KeywordFetcherMatch {
    std::string contents;
    bool from_keyword;
    bool allowed_to_be_default_match;
  };
  const KeywordFetcherMatch kEmptyMatch = { kNotApplicable, false, false };
  struct {
    const std::string json;
    const KeywordFetcherMatch matches[6];
    const std::string inline_autocompletion;
  } cases[] = {
    // Ensure that suggest relevance scores reorder matches and that
    // the keyword verbatim (lacking a suggested verbatim score) beats
    // the default provider verbatim.
    { "[\"a\",[\"b\", \"c\"],[],[],{\"google:suggestrelevance\":[1, 2]}]",
      { { "a",   true,  true },
        { "k a", false, false },
        { "c",   true,  false },
        { "b",   true,  false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    // Again, check that relevance scores reorder matches, just this
    // time with navigation matches.  This also checks that with
    // suggested relevance scores we allow multiple navsuggest results.
    // Note that navsuggest results that come from a keyword provider
    // are marked as not a keyword result.  (They don't go to a
    // keyword search engine.)
    { "[\"a\",[\"http://b.com\", \"http://c.com\", \"d\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
       "\"google:suggestrelevance\":[1301, 1302, 1303]}]",
      { { "a",     true,  true },
        { "d",     true,  false },
        { "c.com", false, false },
        { "b.com", false, false },
        { "k a",   false, false },
        kEmptyMatch },
      std::string() },

    // Without suggested relevance scores, we should only allow one
    // navsuggest result to be be displayed.
    { "[\"a\",[\"http://b.com\", \"http://c.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"]}]",
      { { "a",     true,  true },
        { "b.com", false, false },
        { "k a",   false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // Ensure that verbatimrelevance scores reorder or suppress verbatim.
    // Negative values will have no effect; the calculated value will be used.
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":9999,"
                             "\"google:suggestrelevance\":[9998]}]",
      { { "a",   true,  true },
        { "a1",  true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":9998,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1",  true,  true },
        { "a",   true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":0,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1",  true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":-1,"
                             "\"google:suggestrelevance\":[9999]}]",
      { { "a1",  true,  true },
        { "a",   true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"http://a.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9999,"
        "\"google:suggestrelevance\":[9998]}]",
      { { "a",     true,  true },
        { "a.com", false, false },
        { "k a",   false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // Ensure that both types of relevance scores reorder matches together.
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[9999, 9997],"
                                     "\"google:verbatimrelevance\":9998}]",
      { { "a1",  true,  true },
        { "a",   true,  true },
        { "a2",  true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch },
      "1" },

    // Check that non-inlinable matches may be ranked as the highest result
    // if there is at least one inlineable match.
    { "[\"a\",[\"b\"],[],[],{\"google:suggestrelevance\":[9999]}]",
      { { "b",   true,  false },
        { "a",   true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999]}]",
      { { "b.com", false, false },
        { "a",     true,  true },
        { "k a",   false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    // On the other hand, if there is no inlineable match, restore
    // the keyword verbatim score.
    { "[\"a\",[\"b\"],[],[],{\"google:suggestrelevance\":[9999],"
                            "\"google:verbatimrelevance\":0}]",
      { { "b",   true,  false },
        { "a",   true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://b.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999],"
        "\"google:verbatimrelevance\":0}]",
      { { "b.com", false, false },
        { "a",     true,  true },
        { "k a",   false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // The top result does not have to score as highly as calculated
    // verbatim.  i.e., there are no minimum score restrictions in
    // this provider.
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":0}]",
      { { "a1",  true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:verbatimrelevance\":1}]",
      { { "a1",  true,  true },
        { "k a", false, false },
        { "a",   true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\"],[],[],{\"google:suggestrelevance\":[1],"
                             "\"google:verbatimrelevance\":0}]",
      { { "k a", false, false },
        { "a1",   true, true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "1" },
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[1, 2],"
                                     "\"google:verbatimrelevance\":0}]",
      {
        { "k a", false, false },
        { "a2",  true,  true },
        { "a1",  true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      "2" },
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[1, 3],"
      "\"google:verbatimrelevance\":2}]",
      { { "k a", false, false },
        { "a2",  true,  true },
        { "a",   true,  true },
        { "a1",  true,  true },
        kEmptyMatch, kEmptyMatch },
      "2" },

    // Ensure that all suggestions are considered, regardless of order.
    { "[\"a\",[\"b\", \"c\", \"d\", \"e\", \"f\", \"g\", \"h\"],[],[],"
       "{\"google:suggestrelevance\":[1, 2, 3, 4, 5, 6, 7]}]",
      { { "a",   true,  true },
        { "k a", false, false },
        { "h",   true,  false },
        { "g",   true,  false },
        { "f",   true,  false },
        { "e",   true,  false } },
      std::string() },
    { "[\"a\",[\"http://b.com\", \"http://c.com\", \"http://d.com\","
              "\"http://e.com\", \"http://f.com\", \"http://g.com\","
              "\"http://h.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\", \"NAVIGATION\","
                                "\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[1, 2, 3, 4, 5, 6, 7]}]",
      { { "a",     true,  true },
        { "k a",   false, false },
        { "h.com", false, false },
        { "g.com", false, false },
        { "f.com", false, false },
        { "e.com", false, false } },
      std::string() },

    // Ensure that incorrectly sized suggestion relevance lists are ignored.
    // Note that keyword suggestions by default (not in suggested relevance
    // mode) score more highly than the default verbatim.
    { "[\"a\",[\"a1\", \"a2\"],[],[],{\"google:suggestrelevance\":[1]}]",
      { { "a",   true,  true },
        { "a1",  true,  true },
        { "a2",  true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"a1\"],[],[],{\"google:suggestrelevance\":[9999, 1]}]",
      { { "a",   true,  true },
        { "a1",  true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    // In this case, ignoring the suggested relevance scores means we keep
    // only one navsuggest result.
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:suggestrelevance\":[1]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
       "\"google:suggestrelevance\":[9999, 1]}]",
      { { "a",      true,  true },
        { "a1.com", false, false },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // Ensure that all 'verbatim' results are merged with their maximum score.
    { "[\"a\",[\"a\", \"a1\", \"a2\"],[],[],"
       "{\"google:suggestrelevance\":[9998, 9997, 9999]}]",
      { { "a2",  true,  true },
        { "a",   true,  true },
        { "a1",  true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch },
      "2" },
    { "[\"a\",[\"a\", \"a1\", \"a2\"],[],[],"
       "{\"google:suggestrelevance\":[9998, 9997, 9999],"
        "\"google:verbatimrelevance\":0}]",
      { { "a2",  true,  true },
        { "a",   true,  true },
        { "a1",  true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch },
      "2" },

    // Ensure that verbatim is always generated without other suggestions.
    // TODO(mpearson): Ensure the value of verbatimrelevance is respected
    // (except when suggested relevances are ignored).
    { "[\"a\",[],[],[],{\"google:verbatimrelevance\":1}]",
      { { "k a", false, false },
        { "a",   true,  true },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[],[],[],{\"google:verbatimrelevance\":0}]",
      { { "a",   true,  true },
        { "k a", false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },

    // In reorder mode, navsuggestions will not need to be demoted (because
    // they are marked as not allowed to be default match and will be
    // reordered as necessary).
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9998, 9999]}]",
      { { "a2.com", false, false },
        { "a1.com", false, false },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9999, 9998]}]",
      { { "a1.com", false, false },
        { "a2.com", false, false },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"https://a/\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\"],"
        "\"google:suggestrelevance\":[9999]}]",
      { { "https://a", false, false },
        { "a",         true,  true },
        { "k a",       false, false },
        kEmptyMatch, kEmptyMatch, kEmptyMatch },
      std::string() },
    // Check when navsuggest scores more than verbatim and there is query
    // suggestion but it scores lower.
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9998, 9999, 1300]}]",
      { { "a2.com", false, false },
        { "a1.com", false, false },
        { "a",      true,  true },
        { "a3",     true,  true },
        { "k a",    false, false },
        kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9999, 9998, 1300]}]",
      { { "a1.com", false, false },
        { "a2.com", false, false },
        { "a",      true,  true },
        { "a3",     true,  true },
        { "k a",    false, false },
        kEmptyMatch },
      std::string() },
    // Check when navsuggest scores more than a query suggestion.  There is
    // a verbatim but it scores lower.
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9998, 9999, 9997]}]",
      { { "a2.com", false, false },
        { "a1.com", false, false },
        { "a3",     true,  true },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9999, 9998, 9997]}]",
      { { "a1.com", false, false },
        { "a2.com", false, false },
        { "a3",     true,  true },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9998, 9999, 9997]}]",
      { { "a2.com", false, false },
        { "a1.com", false, false },
        { "a3",     true,  true },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9999, 9998, 9997]}]",
      { { "a1.com", false, false },
        { "a2.com", false, false },
        { "a3",     true,  true },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      "3" },
    // Check when there is neither verbatim nor a query suggestion that,
    // because we can't demote navsuggestions below a query suggestion,
    // we restore the keyword verbatim score.
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9998, 9999]}]",
      { { "a2.com", false, false },
        { "a1.com", false, false },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\"],"
        "\"google:verbatimrelevance\":0,"
        "\"google:suggestrelevance\":[9999, 9998]}]",
      { { "a1.com", false, false },
        { "a2.com", false, false },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch, kEmptyMatch },
      std::string() },
    // More checks that everything works when it's not necessary to demote.
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9997, 9998, 9999]}]",
      { { "a3",     true,  true },
        { "a2.com", false, false },
        { "a1.com", false, false },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch },
      "3" },
    { "[\"a\",[\"http://a1.com\", \"http://a2.com\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"NAVIGATION\", \"NAVIGATION\", \"QUERY\"],"
        "\"google:verbatimrelevance\":9990,"
        "\"google:suggestrelevance\":[9998, 9997, 9999]}]",
      { { "a3",     true,  true },
        { "a1.com", false, false },
        { "a2.com", false, false },
        { "a",      true,  true },
        { "k a",    false, false },
        kEmptyMatch },
      "3" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    QueryForInput(ASCIIToUTF16("k a"), false, true);

    // Set up a default fetcher with no results.
    net::TestURLFetcher* default_fetcher =
        test_factory_.GetFetcherByID(
            SearchProvider::kDefaultProviderURLFetcherID);
    ASSERT_TRUE(default_fetcher);
    default_fetcher->set_response_code(200);
    default_fetcher->delegate()->OnURLFetchComplete(default_fetcher);
    default_fetcher = NULL;

    // Set up a keyword fetcher with provided results.
    net::TestURLFetcher* keyword_fetcher =
        test_factory_.GetFetcherByID(
            SearchProvider::kKeywordProviderURLFetcherID);
    ASSERT_TRUE(keyword_fetcher);
    keyword_fetcher->set_response_code(200);
    keyword_fetcher->SetResponseString(cases[i].json);
    keyword_fetcher->delegate()->OnURLFetchComplete(keyword_fetcher);
    keyword_fetcher = NULL;
    RunTillProviderDone();

    const std::string description = "for input with json=" + cases[i].json;
    const ACMatches& matches = provider_->matches();
    ASSERT_FALSE(matches.empty());
    // Find the first match that's allowed to be the default match and check
    // its inline_autocompletion.
    ACMatches::const_iterator it = FindDefaultMatch(matches);
    ASSERT_NE(matches.end(), it);
    EXPECT_EQ(ASCIIToUTF16(cases[i].inline_autocompletion),
              it->inline_autocompletion) << description;

    ASSERT_LE(matches.size(), ARRAYSIZE_UNSAFE(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      EXPECT_EQ(ASCIIToUTF16(cases[i].matches[j].contents),
                matches[j].contents) << description;
      EXPECT_EQ(cases[i].matches[j].from_keyword,
                matches[j].keyword == ASCIIToUTF16("k")) << description;
      EXPECT_EQ(cases[i].matches[j].allowed_to_be_default_match,
                matches[j].allowed_to_be_default_match) << description;
    }
    // Ensure that no expected matches are missing.
    for (; j < ARRAYSIZE_UNSAFE(cases[i].matches); ++j)
      EXPECT_EQ(kNotApplicable, cases[i].matches[j].contents) <<
          "Case # " << i << " " << description;
  }
}

TEST_F(SearchProviderTest, LocalAndRemoteRelevances) {
  // We hardcode the string "term1" below, so ensure that the search term that
  // got added to history already is that string.
  ASSERT_EQ(ASCIIToUTF16("term1"), term1_);
  base::string16 term = term1_.substr(0, term1_.length() - 1);

  AddSearchToHistory(default_t_url_, term + ASCIIToUTF16("2"), 2);
  profile_.BlockUntilHistoryProcessesPendingRequests();

  struct {
    const base::string16 input;
    const std::string json;
    const std::string matches[6];
  } cases[] = {
    // The history results outscore the default verbatim score.  term2 has more
    // visits so it outscores term1.  The suggestions are still returned since
    // they're server-scored.
    { term,
      "[\"term\",[\"a1\", \"a2\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\", \"QUERY\"],"
        "\"google:suggestrelevance\":[1, 2, 3]}]",
      { "term2", "term1", "term", "a3", "a2", "a1" } },
    // Because we already have three suggestions by the time we see the history
    // results, they don't get returned.
    { term,
      "[\"term\",[\"a1\", \"a2\", \"a3\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\", \"QUERY\"],"
        "\"google:verbatimrelevance\":1450,"
        "\"google:suggestrelevance\":[1440, 1430, 1420]}]",
      { "term", "a1", "a2", "a3", kNotApplicable, kNotApplicable } },
    // If we only have two suggestions, we have room for a history result.
    { term,
      "[\"term\",[\"a1\", \"a2\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\"],"
        "\"google:verbatimrelevance\":1450,"
        "\"google:suggestrelevance\":[1430, 1410]}]",
      { "term", "a1", "a2", "term2", kNotApplicable, kNotApplicable } },
    // If we have more than three suggestions, they should all be returned as
    // long as we have enough total space for them.
    { term,
      "[\"term\",[\"a1\", \"a2\", \"a3\", \"a4\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\", \"QUERY\", \"QUERY\"],"
        "\"google:verbatimrelevance\":1450,"
        "\"google:suggestrelevance\":[1440, 1430, 1420, 1410]}]",
      { "term", "a1", "a2", "a3", "a4", kNotApplicable } },
    { term,
      "[\"term\",[\"a1\", \"a2\", \"a3\", \"a4\", \"a5\", \"a6\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\", \"QUERY\", \"QUERY\","
                                "\"QUERY\", \"QUERY\"],"
        "\"google:verbatimrelevance\":1450,"
        "\"google:suggestrelevance\":[1440, 1430, 1420, 1410, 1400, 1390]}]",
      { "term", "a1", "a2", "a3", "a4", "a5" } },
    { term,
      "[\"term\",[\"a1\", \"a2\", \"a3\", \"a4\"],[],[],"
       "{\"google:suggesttype\":[\"QUERY\", \"QUERY\", \"QUERY\", \"QUERY\"],"
        "\"google:verbatimrelevance\":1450,"
        "\"google:suggestrelevance\":[1430, 1410, 1390, 1370]}]",
      { "term", "a1", "a2", "term2", "a3", "a4" } }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    QueryForInput(cases[i].input, false, false);
    net::TestURLFetcher* fetcher =
        test_factory_.GetFetcherByID(
            SearchProvider::kDefaultProviderURLFetcherID);
    ASSERT_TRUE(fetcher);
    fetcher->set_response_code(200);
    fetcher->SetResponseString(cases[i].json);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    RunTillProviderDone();

    const std::string description = "for input with json=" + cases[i].json;
    const ACMatches& matches = provider_->matches();

    // Ensure no extra matches are present.
    ASSERT_LE(matches.size(), ARRAYSIZE_UNSAFE(cases[i].matches));

    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j)
      EXPECT_EQ(ASCIIToUTF16(cases[i].matches[j]),
                matches[j].contents) << description;
    // Ensure that no expected matches are missing.
    for (; j < ARRAYSIZE_UNSAFE(cases[i].matches); ++j)
      EXPECT_EQ(kNotApplicable, cases[i].matches[j]) <<
          "Case # " << i << " " << description;
  }
}

// Verifies suggest relevance behavior for URL input.
TEST_F(SearchProviderTest, DefaultProviderSuggestRelevanceScoringUrlInput) {
  struct DefaultFetcherUrlInputMatch {
    const std::string match_contents;
    AutocompleteMatch::Type match_type;
    bool allowed_to_be_default_match;
  };
  const DefaultFetcherUrlInputMatch kEmptyMatch =
      { kNotApplicable, AutocompleteMatchType::NUM_TYPES, false };
  struct {
    const std::string input;
    const std::string json;
    const DefaultFetcherUrlInputMatch output[4];
  } cases[] = {
    // Ensure NAVIGATION matches are allowed to be listed first for URL
    // input regardless of whether the match is inlineable.  Note that
    // non-inlineable matches should not be allowed to be the default match.
    { "a.com", "[\"a.com\",[\"http://b.com/\"],[],[],"
                "{\"google:suggesttype\":[\"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9999]}]",
      { { "b.com",   AutocompleteMatchType::NAVSUGGEST,            false },
        { "a.com",   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        kEmptyMatch, kEmptyMatch } },
    { "a.com", "[\"a.com\",[\"https://b.com\"],[],[],"
                "{\"google:suggesttype\":[\"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9999]}]",
      { { "https://b.com", AutocompleteMatchType::NAVSUGGEST,           false },
        { "a.com",         AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        kEmptyMatch, kEmptyMatch } },
    { "a.com", "[\"a.com\",[\"http://a.com/a\"],[],[],"
                "{\"google:suggesttype\":[\"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9999]}]",
      { { "a.com/a", AutocompleteMatchType::NAVSUGGEST,            true },
        { "a.com",   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        kEmptyMatch, kEmptyMatch } },
    { "a.com", "[\"a.com\",[\"https://a.com\"],[],[],"
                "{\"google:suggesttype\":[\"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9999]}]",
      { { "https://a.com", AutocompleteMatchType::NAVSUGGEST,            true },
        { "a.com",         AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        kEmptyMatch, kEmptyMatch } },

    // Ensure topmost inlineable SUGGEST matches are NOT allowed for URL
    // input.  SearchProvider disregards search and verbatim suggested
    // relevances.
    { "a.com", "[\"a.com\",[\"a.com info\"],[],[],"
                "{\"google:suggestrelevance\":[9999]}]",
      { { "a.com",      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        { "a.com info", AutocompleteMatchType::SEARCH_SUGGEST,        true },
        kEmptyMatch, kEmptyMatch } },
    { "a.com", "[\"a.com\",[\"a.com info\"],[],[],"
                "{\"google:suggestrelevance\":[9999]}]",
      { { "a.com",   AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,   true },
        { "a.com info", AutocompleteMatchType::SEARCH_SUGGEST,        true },
        kEmptyMatch, kEmptyMatch } },

    // Ensure the fallback mechanism allows inlinable NAVIGATION matches.
    { "a.com", "[\"a.com\",[\"a.com info\", \"http://a.com/b\"],[],[],"
                "{\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9999, 9998]}]",
      { { "a.com/b",    AutocompleteMatchType::NAVSUGGEST,            true },
        { "a.com",      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        { "a.com info", AutocompleteMatchType::SEARCH_SUGGEST,        true },
        kEmptyMatch } },
    { "a.com", "[\"a.com\",[\"a.com info\", \"http://a.com/b\"],[],[],"
                "{\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
                 "\"google:suggestrelevance\":[9998, 9997],"
                 "\"google:verbatimrelevance\":9999}]",
      { { "a.com/b",    AutocompleteMatchType::NAVSUGGEST,            true },
        { "a.com",      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        { "a.com info", AutocompleteMatchType::SEARCH_SUGGEST,        true },
        kEmptyMatch } },

    // Ensure topmost non-inlineable SUGGEST matches are allowed for URL
    // input assuming the top inlineable match is not a query (i.e., is a
    // NAVSUGGEST).
    { "a.com", "[\"a.com\",[\"info\"],[],[],"
                "{\"google:suggestrelevance\":[9999]}]",
      { { "info",  AutocompleteMatchType::SEARCH_SUGGEST,        false },
        { "a.com", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        kEmptyMatch, kEmptyMatch } },
    { "a.com", "[\"a.com\",[\"info\"],[],[],"
                "{\"google:suggestrelevance\":[9999]}]",
      { { "info",  AutocompleteMatchType::SEARCH_SUGGEST,        false },
        { "a.com", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, true },
        kEmptyMatch, kEmptyMatch } },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    QueryForInput(ASCIIToUTF16(cases[i].input), false, false);
    net::TestURLFetcher* fetcher =
        test_factory_.GetFetcherByID(
            SearchProvider::kDefaultProviderURLFetcherID);
    ASSERT_TRUE(fetcher);
    fetcher->set_response_code(200);
    fetcher->SetResponseString(cases[i].json);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
    RunTillProviderDone();

    size_t j = 0;
    const ACMatches& matches = provider_->matches();
    ASSERT_LE(matches.size(), ARRAYSIZE_UNSAFE(cases[i].output));
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      EXPECT_EQ(ASCIIToUTF16(cases[i].output[j].match_contents),
                matches[j].contents);
      EXPECT_EQ(cases[i].output[j].match_type, matches[j].type);
      EXPECT_EQ(cases[i].output[j].allowed_to_be_default_match,
                matches[j].allowed_to_be_default_match);
    }
    // Ensure that no expected matches are missing.
    for (; j < ARRAYSIZE_UNSAFE(cases[i].output); ++j) {
      EXPECT_EQ(kNotApplicable, cases[i].output[j].match_contents);
      EXPECT_EQ(AutocompleteMatchType::NUM_TYPES,
                cases[i].output[j].match_type);
      EXPECT_FALSE(cases[i].output[j].allowed_to_be_default_match);
    }
  }
}

// A basic test that verifies the field trial triggered parsing logic.
TEST_F(SearchProviderTest, FieldTrialTriggeredParsing) {
  QueryForInput(ASCIIToUTF16("foo"), false, false);

  // Make sure the default providers suggest service was queried.
  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(fetcher);

  // Tell the SearchProvider the suggest query is done.
  fetcher->set_response_code(200);
  fetcher->SetResponseString(
      "[\"foo\",[\"foo bar\"],[\"\"],[],"
      "{\"google:suggesttype\":[\"QUERY\"],"
      "\"google:fieldtrialtriggered\":true}]");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  fetcher = NULL;

  // Run till the history results complete.
  RunTillProviderDone();

  {
    // Check for the match and field trial triggered bits.
    AutocompleteMatch match;
    EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("foo bar"), &match));
    ProvidersInfo providers_info;
    provider_->AddProviderInfo(&providers_info);
    ASSERT_EQ(1U, providers_info.size());
    EXPECT_EQ(1, providers_info[0].field_trial_triggered_size());
    EXPECT_EQ(1, providers_info[0].field_trial_triggered_in_session_size());
  }
  {
    // Reset the session and check that bits are reset.
    provider_->ResetSession();
    ProvidersInfo providers_info;
    provider_->AddProviderInfo(&providers_info);
    ASSERT_EQ(1U, providers_info.size());
    EXPECT_EQ(1, providers_info[0].field_trial_triggered_size());
    EXPECT_EQ(0, providers_info[0].field_trial_triggered_in_session_size());
  }
}

// Verifies inline autocompletion of navigational results.
TEST_F(SearchProviderTest, NavigationInline) {
  struct {
    const std::string input;
    const std::string url;
    // Test the expected fill_into_edit, which may drop "http://".
    // Some cases do not trim "http://" to match from the start of the scheme.
    const std::string fill_into_edit;
    const std::string inline_autocompletion;
    const bool allowed_to_be_default_match_in_regular_mode;
    const bool allowed_to_be_default_match_in_prevent_inline_mode;
  } cases[] = {
    // Do not inline matches that do not contain the input; trim http as needed.
    { "x",                 "http://www.abc.com",
                                  "www.abc.com",  std::string(), false, false },
    { "https:",            "http://www.abc.com",
                                  "www.abc.com",  std::string(), false, false },
    { "http://www.abc.com/a", "http://www.abc.com",
                              "http://www.abc.com",  std::string(), false,
                                                                    false },

    // Do not inline matches with invalid input prefixes; trim http as needed.
    { "ttp",              "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },
    { "://w",             "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },
    { "ww.",              "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },
    { ".ab",              "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },
    { "bc",               "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },
    { ".com",             "http://www.abc.com",
                                 "www.abc.com", std::string(), false, false },

    // Do not inline matches that omit input domain labels; trim http as needed.
    { "www.a",            "http://a.com",
                                 "a.com",       std::string(), false, false },
    { "http://www.a",     "http://a.com",
                          "http://a.com",       std::string(), false, false },
    { "www.a",            "ftp://a.com",
                          "ftp://a.com",        std::string(), false, false },
    { "ftp://www.a",      "ftp://a.com",
                          "ftp://a.com",        std::string(), false, false },

    // Input matching but with nothing to inline will not yield an offset, but
    // will be allowed to be default.
    { "abc.com",             "http://www.abc.com",
                                    "www.abc.com", std::string(), true, true },
    { "abc.com/",            "http://www.abc.com",
                                    "www.abc.com", std::string(), true, true },
    { "http://www.abc.com",  "http://www.abc.com",
                             "http://www.abc.com", std::string(), true, true },
    { "http://www.abc.com/", "http://www.abc.com",
                             "http://www.abc.com", std::string(), true, true },

    // Inputs with trailing whitespace should inline when possible.
    { "abc.com ",      "http://www.abc.com",
                              "www.abc.com",      std::string(), true,  true },
    { "abc.com/ ",     "http://www.abc.com",
                              "www.abc.com",      std::string(), true,  true },
    { "abc.com ",      "http://www.abc.com/bar",
                              "www.abc.com/bar",  "/bar",        false, false },

    // A suggestion that's equivalent to what the input gets fixed up to
    // should be inlined.
    { "abc.com:",      "http://abc.com/",
                              "abc.com",      "", true, true },
    { "abc.com:",      "http://www.abc.com",
                              "www.abc.com",  "", true, true },

    // Inline matches when the input is a leading substring of the scheme.
    { "h",             "http://www.abc.com",
                       "http://www.abc.com", "ttp://www.abc.com", true, false },
    { "http",          "http://www.abc.com",
                       "http://www.abc.com", "://www.abc.com",    true, false },

    // Inline matches when the input is a leading substring of the full URL.
    { "http:",             "http://www.abc.com",
                           "http://www.abc.com", "//www.abc.com", true, false },
    { "http://w",          "http://www.abc.com",
                           "http://www.abc.com", "ww.abc.com",    true, false },
    { "http://www.",       "http://www.abc.com",
                           "http://www.abc.com", "abc.com",       true, false },
    { "http://www.ab",     "http://www.abc.com",
                           "http://www.abc.com", "c.com",         true, false },
    { "http://www.abc.com/p", "http://www.abc.com/path/file.htm?q=x#foo",
                              "http://www.abc.com/path/file.htm?q=x#foo",
                                                  "ath/file.htm?q=x#foo",
                                                                  true, false },
    { "http://abc.com/p",     "http://abc.com/path/file.htm?q=x#foo",
                              "http://abc.com/path/file.htm?q=x#foo",
                                              "ath/file.htm?q=x#foo",
                                                                  true, false},

    // Inline matches with valid URLPrefixes; only trim "http://".
    { "w",               "http://www.abc.com",
                                "www.abc.com", "ww.abc.com", true, false },
    { "www.a",           "http://www.abc.com",
                                "www.abc.com", "bc.com",     true, false },
    { "abc",             "http://www.abc.com",
                                "www.abc.com", ".com",       true, false },
    { "abc.c",           "http://www.abc.com",
                                "www.abc.com", "om",         true, false },
    { "abc.com/p",       "http://www.abc.com/path/file.htm?q=x#foo",
                                "www.abc.com/path/file.htm?q=x#foo",
                                             "ath/file.htm?q=x#foo",
                                                             true, false },
    { "abc.com/p",       "http://abc.com/path/file.htm?q=x#foo",
                                "abc.com/path/file.htm?q=x#foo",
                                         "ath/file.htm?q=x#foo",
                                                             true, false },

    // Inline matches using the maximal URLPrefix components.
    { "h",               "http://help.com",
                                "help.com", "elp.com",     true, false },
    { "http",            "http://http.com",
                                "http.com", ".com",        true, false },
    { "h",               "http://www.help.com",
                                "www.help.com", "elp.com", true, false },
    { "http",            "http://www.http.com",
                                "www.http.com", ".com",    true, false },
    { "w",               "http://www.www.com",
                                "www.www.com",  "ww.com",  true, false },

    // Test similar behavior for the ftp and https schemes.
    { "ftp://www.ab",  "ftp://www.abc.com/path/file.htm?q=x#foo",
                       "ftp://www.abc.com/path/file.htm?q=x#foo",
                                  "c.com/path/file.htm?q=x#foo",  true, false },
    { "www.ab",        "ftp://www.abc.com/path/file.htm?q=x#foo",
                       "ftp://www.abc.com/path/file.htm?q=x#foo",
                                   "c.com/path/file.htm?q=x#foo", true, false },
    { "ab",            "ftp://www.abc.com/path/file.htm?q=x#foo",
                       "ftp://www.abc.com/path/file.htm?q=x#foo",
                                   "c.com/path/file.htm?q=x#foo", true, false },
    { "ab",            "ftp://abc.com/path/file.htm?q=x#foo",
                       "ftp://abc.com/path/file.htm?q=x#foo",
                               "c.com/path/file.htm?q=x#foo",     true, false },
    { "https://www.ab",  "https://www.abc.com/path/file.htm?q=x#foo",
                         "https://www.abc.com/path/file.htm?q=x#foo",
                                       "c.com/path/file.htm?q=x#foo",
                                                                  true, false },
    { "www.ab",      "https://www.abc.com/path/file.htm?q=x#foo",
                     "https://www.abc.com/path/file.htm?q=x#foo",
                                   "c.com/path/file.htm?q=x#foo", true, false },
    { "ab",          "https://www.abc.com/path/file.htm?q=x#foo",
                     "https://www.abc.com/path/file.htm?q=x#foo",
                                   "c.com/path/file.htm?q=x#foo", true, false },
    { "ab",          "https://abc.com/path/file.htm?q=x#foo",
                     "https://abc.com/path/file.htm?q=x#foo",
                               "c.com/path/file.htm?q=x#foo",     true, false },

    // Forced query input should inline and retain the "?" prefix.
    { "?http://www.ab",  "http://www.abc.com",
                        "?http://www.abc.com", "c.com", true, false },
    { "?www.ab",         "http://www.abc.com",
                               "?www.abc.com", "c.com", true, false },
    { "?ab",             "http://www.abc.com",
                               "?www.abc.com", "c.com", true, false },
    { "?abc.com",        "http://www.abc.com",
                               "?www.abc.com", "",      true, true },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    // First test regular mode.
    QueryForInput(ASCIIToUTF16(cases[i].input), false, false);
    AutocompleteMatch match(
        provider_->NavigationToMatch(SearchSuggestionParser::NavigationResult(
            ChromeAutocompleteSchemeClassifier(&profile_), GURL(cases[i].url),
            AutocompleteMatchType::NAVSUGGEST, base::string16(), std::string(),
            false, 0, false, ASCIIToUTF16(cases[i].input), std::string())));
    EXPECT_EQ(ASCIIToUTF16(cases[i].inline_autocompletion),
              match.inline_autocompletion);
    EXPECT_EQ(ASCIIToUTF16(cases[i].fill_into_edit), match.fill_into_edit);
    EXPECT_EQ(cases[i].allowed_to_be_default_match_in_regular_mode,
              match.allowed_to_be_default_match);

    // Then test prevent-inline-autocomplete mode.
    QueryForInput(ASCIIToUTF16(cases[i].input), true, false);
    AutocompleteMatch match_prevent_inline(
        provider_->NavigationToMatch(SearchSuggestionParser::NavigationResult(
            ChromeAutocompleteSchemeClassifier(&profile_), GURL(cases[i].url),
            AutocompleteMatchType::NAVSUGGEST, base::string16(), std::string(),
            false, 0, false, ASCIIToUTF16(cases[i].input), std::string())));
    EXPECT_EQ(ASCIIToUTF16(cases[i].inline_autocompletion),
              match_prevent_inline.inline_autocompletion);
    EXPECT_EQ(ASCIIToUTF16(cases[i].fill_into_edit),
              match_prevent_inline.fill_into_edit);
    EXPECT_EQ(cases[i].allowed_to_be_default_match_in_prevent_inline_mode,
              match_prevent_inline.allowed_to_be_default_match);
  }
}

// Verifies that "http://" is not trimmed for input that is a leading substring.
TEST_F(SearchProviderTest, NavigationInlineSchemeSubstring) {
  const base::string16 input(ASCIIToUTF16("ht"));
  const base::string16 url(ASCIIToUTF16("http://a.com"));
  const SearchSuggestionParser::NavigationResult result(
      ChromeAutocompleteSchemeClassifier(&profile_), GURL(url),
      AutocompleteMatchType::NAVSUGGEST,
      base::string16(), std::string(), false, 0, false, input, std::string());

  // Check the offset and strings when inline autocompletion is allowed.
  QueryForInput(input, false, false);
  AutocompleteMatch match_inline(provider_->NavigationToMatch(result));
  EXPECT_EQ(url, match_inline.fill_into_edit);
  EXPECT_EQ(url.substr(2), match_inline.inline_autocompletion);
  EXPECT_TRUE(match_inline.allowed_to_be_default_match);
  EXPECT_EQ(url, match_inline.contents);

  // Check the same strings when inline autocompletion is prevented.
  QueryForInput(input, true, false);
  AutocompleteMatch match_prevent(provider_->NavigationToMatch(result));
  EXPECT_EQ(url, match_prevent.fill_into_edit);
  EXPECT_FALSE(match_prevent.allowed_to_be_default_match);
  EXPECT_EQ(url, match_prevent.contents);
}

// Verifies that input "w" marks a more significant domain label than "www.".
TEST_F(SearchProviderTest, NavigationInlineDomainClassify) {
  QueryForInput(ASCIIToUTF16("w"), false, false);
  AutocompleteMatch match(
      provider_->NavigationToMatch(SearchSuggestionParser::NavigationResult(
          ChromeAutocompleteSchemeClassifier(&profile_),
          GURL("http://www.wow.com"),
          AutocompleteMatchType::NAVSUGGEST, base::string16(), std::string(),
          false, 0, false, ASCIIToUTF16("w"), std::string())));
  EXPECT_EQ(ASCIIToUTF16("ow.com"), match.inline_autocompletion);
  EXPECT_TRUE(match.allowed_to_be_default_match);
  EXPECT_EQ(ASCIIToUTF16("www.wow.com"), match.fill_into_edit);
  EXPECT_EQ(ASCIIToUTF16("www.wow.com"), match.contents);

  // Ensure that the match for input "w" is marked on "wow" and not "www".
  ASSERT_EQ(3U, match.contents_class.size());
  EXPECT_EQ(0U, match.contents_class[0].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::URL,
            match.contents_class[0].style);
  EXPECT_EQ(4U, match.contents_class[1].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::URL |
            AutocompleteMatch::ACMatchClassification::MATCH,
            match.contents_class[1].style);
  EXPECT_EQ(5U, match.contents_class[2].offset);
  EXPECT_EQ(AutocompleteMatch::ACMatchClassification::URL,
            match.contents_class[2].style);
}

#if !defined(OS_WIN)
// Verify entity suggestion parsing.
TEST_F(SearchProviderTest, ParseEntitySuggestion) {
  struct Match {
    std::string contents;
    std::string description;
    std::string query_params;
    std::string fill_into_edit;
    AutocompleteMatchType::Type type;
  };
  const Match kEmptyMatch = {
    kNotApplicable, kNotApplicable, kNotApplicable, kNotApplicable,
    AutocompleteMatchType::NUM_TYPES};

  struct {
    const std::string input_text;
    const std::string response_json;
    const Match matches[5];
  } cases[] = {
    // A query and an entity suggestion with different search terms.
    { "x",
      "[\"x\",[\"xy\", \"yy\"],[\"\",\"\"],[],"
      " {\"google:suggestdetail\":[{},"
      "   {\"a\":\"A\",\"t\":\"xy\",\"q\":\"p=v\"}],"
      "\"google:suggesttype\":[\"QUERY\",\"ENTITY\"]}]",
      { { "x", "", "", "x", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
        { "xy", "", "", "xy", AutocompleteMatchType::SEARCH_SUGGEST },
        { "xy", "A", "p=v", "yy",
          AutocompleteMatchType::SEARCH_SUGGEST_ENTITY },
        kEmptyMatch,
        kEmptyMatch
      },
    },
    // A query and an entity suggestion with same search terms.
    { "x",
      "[\"x\",[\"xy\", \"xy\"],[\"\",\"\"],[],"
      " {\"google:suggestdetail\":[{},"
      "   {\"a\":\"A\",\"t\":\"xy\",\"q\":\"p=v\"}],"
      "\"google:suggesttype\":[\"QUERY\",\"ENTITY\"]}]",
      { { "x", "", "", "x", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
        { "xy", "", "", "xy", AutocompleteMatchType::SEARCH_SUGGEST },
        { "xy", "A", "p=v", "xy",
          AutocompleteMatchType::SEARCH_SUGGEST_ENTITY },
        kEmptyMatch,
        kEmptyMatch
      },
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    QueryForInput(ASCIIToUTF16(cases[i].input_text), false, false);

    // Set up a default fetcher with provided results.
    net::TestURLFetcher* fetcher =
        test_factory_.GetFetcherByID(
            SearchProvider::kDefaultProviderURLFetcherID);
    ASSERT_TRUE(fetcher);
    fetcher->set_response_code(200);
    fetcher->SetResponseString(cases[i].response_json);
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    RunTillProviderDone();

    const ACMatches& matches = provider_->matches();
    ASSERT_FALSE(matches.empty());

    SCOPED_TRACE("for input with json = " + cases[i].response_json);

    ASSERT_LE(matches.size(), ARRAYSIZE_UNSAFE(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      const Match& match = cases[i].matches[j];
      SCOPED_TRACE(" and match index: " + base::IntToString(j));
      EXPECT_EQ(match.contents,
                base::UTF16ToUTF8(matches[j].contents));
      EXPECT_EQ(match.description,
                base::UTF16ToUTF8(matches[j].description));
      EXPECT_EQ(match.query_params,
                matches[j].search_terms_args->suggest_query_params);
      EXPECT_EQ(match.fill_into_edit,
                base::UTF16ToUTF8(matches[j].fill_into_edit));
      EXPECT_EQ(match.type, matches[j].type);
    }
    // Ensure that no expected matches are missing.
    for (; j < ARRAYSIZE_UNSAFE(cases[i].matches); ++j) {
      SCOPED_TRACE(" and match index: " + base::IntToString(j));
      EXPECT_EQ(cases[i].matches[j].contents, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].description, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].query_params, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].fill_into_edit, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].type, AutocompleteMatchType::NUM_TYPES);
    }
  }
}
#endif  // !defined(OS_WIN)


// A basic test that verifies the prefetch metadata parsing logic.
TEST_F(SearchProviderTest, PrefetchMetadataParsing) {
  struct Match {
    std::string contents;
    bool allowed_to_be_prefetched;
    AutocompleteMatchType::Type type;
    bool from_keyword;
  };
  const Match kEmptyMatch = { kNotApplicable,
                              false,
                              AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
                              false };

  struct {
    const std::string input_text;
    bool prefer_keyword_provider_results;
    const std::string default_provider_response_json;
    const std::string keyword_provider_response_json;
    const Match matches[5];
  } cases[] = {
    // Default provider response does not have prefetch details. Ensure that the
    // suggestions are not marked as prefetch query.
    { "a",
      false,
      "[\"a\",[\"b\", \"c\"],[],[],{\"google:suggestrelevance\":[1, 2]}]",
      std::string(),
      { { "a", false, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, false },
        { "c", false, AutocompleteMatchType::SEARCH_SUGGEST, false },
        { "b", false, AutocompleteMatchType::SEARCH_SUGGEST, false },
        kEmptyMatch,
        kEmptyMatch
      },
    },
    // Ensure that default provider suggest response prefetch details are
    // parsed and recorded in AutocompleteMatch.
    { "ab",
      false,
      "[\"ab\",[\"abc\", \"http://b.com\", \"http://c.com\"],[],[],"
          "{\"google:clientdata\":{\"phi\": 0},"
          "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\", \"NAVIGATION\"],"
          "\"google:suggestrelevance\":[999, 12, 1]}]",
      std::string(),
      { { "ab",    false, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, false },
        { "abc",   true,  AutocompleteMatchType::SEARCH_SUGGEST, false },
        { "b.com", false, AutocompleteMatchType::NAVSUGGEST, false },
        { "c.com", false, AutocompleteMatchType::NAVSUGGEST, false },
        kEmptyMatch
      },
    },
    // Default provider suggest response has prefetch details.
    // SEARCH_WHAT_YOU_TYPE suggestion outranks SEARCH_SUGGEST suggestion for
    // the same query string. Ensure that the prefetch details from
    // SEARCH_SUGGEST match are set onto SEARCH_WHAT_YOU_TYPE match.
    { "ab",
      false,
      "[\"ab\",[\"ab\", \"http://ab.com\"],[],[],"
          "{\"google:clientdata\":{\"phi\": 0},"
          "\"google:suggesttype\":[\"QUERY\", \"NAVIGATION\"],"
          "\"google:suggestrelevance\":[99, 98]}]",
      std::string(),
      { {"ab", true, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, false },
        {"ab.com", false, AutocompleteMatchType::NAVSUGGEST, false },
        kEmptyMatch,
        kEmptyMatch,
        kEmptyMatch
      },
    },
    // Default provider response has prefetch details. We prefer keyword
    // provider results. Ensure that prefetch bit for a suggestion from the
    // default search provider does not get copied onto a higher-scoring match
    // for the same query string from the keyword provider.
    { "k a",
      true,
      "[\"k a\",[\"a\", \"ab\"],[],[], {\"google:clientdata\":{\"phi\": 0},"
          "\"google:suggesttype\":[\"QUERY\", \"QUERY\"],"
          "\"google:suggestrelevance\":[9, 12]}]",
      "[\"a\",[\"b\", \"c\"],[],[],{\"google:suggestrelevance\":[1, 2]}]",
      { { "a", false, AutocompleteMatchType::SEARCH_OTHER_ENGINE, true},
        { "k a", false, AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED, false },
        { "ab", false, AutocompleteMatchType::SEARCH_SUGGEST, false },
        { "c", false, AutocompleteMatchType::SEARCH_SUGGEST, true },
        { "b", false, AutocompleteMatchType::SEARCH_SUGGEST, true }
      },
    }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    QueryForInput(ASCIIToUTF16(cases[i].input_text), false,
                  cases[i].prefer_keyword_provider_results);

    // Set up a default fetcher with provided results.
    net::TestURLFetcher* fetcher =
        test_factory_.GetFetcherByID(
            SearchProvider::kDefaultProviderURLFetcherID);
    ASSERT_TRUE(fetcher);
    fetcher->set_response_code(200);
    fetcher->SetResponseString(cases[i].default_provider_response_json);
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    if (cases[i].prefer_keyword_provider_results) {
      // Set up a keyword fetcher with provided results.
      net::TestURLFetcher* keyword_fetcher =
          test_factory_.GetFetcherByID(
              SearchProvider::kKeywordProviderURLFetcherID);
      ASSERT_TRUE(keyword_fetcher);
      keyword_fetcher->set_response_code(200);
      keyword_fetcher->SetResponseString(
          cases[i].keyword_provider_response_json);
      keyword_fetcher->delegate()->OnURLFetchComplete(keyword_fetcher);
      keyword_fetcher = NULL;
    }

    RunTillProviderDone();

    const std::string description =
        "for input with json =" + cases[i].default_provider_response_json;
    const ACMatches& matches = provider_->matches();
    // The top match must inline and score as highly as calculated verbatim.
    ASSERT_FALSE(matches.empty());
    EXPECT_GE(matches[0].relevance, 1300);

    ASSERT_LE(matches.size(), ARRAYSIZE_UNSAFE(cases[i].matches));
    // Ensure that the returned matches equal the expectations.
    for (size_t j = 0; j < matches.size(); ++j) {
      SCOPED_TRACE(description);
      EXPECT_EQ(cases[i].matches[j].contents,
                base::UTF16ToUTF8(matches[j].contents));
      EXPECT_EQ(cases[i].matches[j].allowed_to_be_prefetched,
                SearchProvider::ShouldPrefetch(matches[j]));
      EXPECT_EQ(cases[i].matches[j].type, matches[j].type);
      EXPECT_EQ(cases[i].matches[j].from_keyword,
                matches[j].keyword == ASCIIToUTF16("k"));
    }
  }
}

TEST_F(SearchProviderTest, XSSIGuardedJSONParsing_InvalidResponse) {
  ClearAllResults();

  std::string input_str("abc");
  QueryForInput(ASCIIToUTF16(input_str), false, false);

  // Set up a default fetcher with provided results.
  net::TestURLFetcher* fetcher =
      test_factory_.GetFetcherByID(
          SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(200);
  fetcher->SetResponseString("this is a bad non-json response");
  fetcher->delegate()->OnURLFetchComplete(fetcher);

  RunTillProviderDone();

  const ACMatches& matches = provider_->matches();

  // Should have exactly one "search what you typed" match
  ASSERT_TRUE(matches.size() == 1);
  EXPECT_EQ(input_str, base::UTF16ToUTF8(matches[0].contents));
  EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            matches[0].type);
}

// A basic test that verifies that the XSSI guarded JSON response is parsed
// correctly.
TEST_F(SearchProviderTest, XSSIGuardedJSONParsing_ValidResponses) {
  struct Match {
    std::string contents;
    AutocompleteMatchType::Type type;
  };
  const Match kEmptyMatch = {
      kNotApplicable, AutocompleteMatchType::NUM_TYPES
  };

  struct {
    const std::string input_text;
    const std::string default_provider_response_json;
    const Match matches[4];
  } cases[] = {
    // No XSSI guard.
    { "a",
      "[\"a\",[\"b\", \"c\"],[],[],"
      "{\"google:suggesttype\":[\"QUERY\",\"QUERY\"],"
      "\"google:suggestrelevance\":[1, 2]}]",
      { { "a", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
        { "c", AutocompleteMatchType::SEARCH_SUGGEST },
        { "b", AutocompleteMatchType::SEARCH_SUGGEST },
        kEmptyMatch,
      },
    },
    // Standard XSSI guard - )]}'\n.
    { "a",
      ")]}'\n[\"a\",[\"b\", \"c\"],[],[],"
      "{\"google:suggesttype\":[\"QUERY\",\"QUERY\"],"
      "\"google:suggestrelevance\":[1, 2]}]",
      { { "a", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
        { "c", AutocompleteMatchType::SEARCH_SUGGEST },
        { "b", AutocompleteMatchType::SEARCH_SUGGEST },
        kEmptyMatch,
      },
    },
    // Modified XSSI guard - contains "[".
    { "a",
      ")]}'\n[)\"[\"a\",[\"b\", \"c\"],[],[],"
      "{\"google:suggesttype\":[\"QUERY\",\"QUERY\"],"
      "\"google:suggestrelevance\":[1, 2]}]",
      { { "a", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
        { "c", AutocompleteMatchType::SEARCH_SUGGEST },
        { "b", AutocompleteMatchType::SEARCH_SUGGEST },
        kEmptyMatch,
      },
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    ClearAllResults();
    QueryForInput(ASCIIToUTF16(cases[i].input_text), false, false);

    // Set up a default fetcher with provided results.
    net::TestURLFetcher* fetcher =
        test_factory_.GetFetcherByID(
            SearchProvider::kDefaultProviderURLFetcherID);
    ASSERT_TRUE(fetcher);
    fetcher->set_response_code(200);
    fetcher->SetResponseString(cases[i].default_provider_response_json);
    fetcher->delegate()->OnURLFetchComplete(fetcher);

    RunTillProviderDone();

    const ACMatches& matches = provider_->matches();
    // The top match must inline and score as highly as calculated verbatim.
    ASSERT_FALSE(matches.empty());
    EXPECT_GE(matches[0].relevance, 1300);

    SCOPED_TRACE("for case: " + base::IntToString(i));
    ASSERT_LE(matches.size(), ARRAYSIZE_UNSAFE(cases[i].matches));
    size_t j = 0;
    // Ensure that the returned matches equal the expectations.
    for (; j < matches.size(); ++j) {
      SCOPED_TRACE("and match: " + base::IntToString(j));
      EXPECT_EQ(cases[i].matches[j].contents,
                base::UTF16ToUTF8(matches[j].contents));
      EXPECT_EQ(cases[i].matches[j].type, matches[j].type);
    }
    for (; j < ARRAYSIZE_UNSAFE(cases[i].matches); ++j) {
      SCOPED_TRACE("and match: " + base::IntToString(j));
      EXPECT_EQ(cases[i].matches[j].contents, kNotApplicable);
      EXPECT_EQ(cases[i].matches[j].type, AutocompleteMatchType::NUM_TYPES);
    }
  }
}

// Test that deletion url gets set on an AutocompleteMatch when available for a
// personalized query or a personalized URL.
TEST_F(SearchProviderTest, ParseDeletionUrl) {
   struct Match {
     std::string contents;
     std::string deletion_url;
     AutocompleteMatchType::Type type;
   };

   const Match kEmptyMatch = {
       kNotApplicable, "", AutocompleteMatchType::NUM_TYPES
   };

   const char* url[] = {
    "http://defaultturl/complete/deleteitems"
       "?delq=ab&client=chrome&deltok=xsrf124",
    "http://defaultturl/complete/deleteitems"
       "?delq=www.amazon.com&client=chrome&deltok=xsrf123",
     };

   struct {
       const std::string input_text;
       const std::string response_json;
       const Match matches[5];
     } cases[] = {
       // A deletion URL on a personalized query should be reflected in the
       // resulting AutocompleteMatch.
       { "a",
         "[\"a\",[\"ab\", \"ac\",\"www.amazon.com\"],[],[],"
         "{\"google:suggesttype\":[\"PERSONALIZED_QUERY\",\"QUERY\","
          "\"PERSONALIZED_NAVIGATION\"],"
         "\"google:suggestrelevance\":[3, 2, 1],"
         "\"google:suggestdetail\":[{\"du\":"
         "\"/complete/deleteitems?delq=ab&client=chrome"
          "&deltok=xsrf124\"}, {}, {\"du\":"
         "\"/complete/deleteitems?delq=www.amazon.com&"
         "client=chrome&deltok=xsrf123\"}]}]",
         { { "a", "", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
           { "ab", url[0], AutocompleteMatchType::SEARCH_SUGGEST },
           { "ac", "", AutocompleteMatchType::SEARCH_SUGGEST },
           { "www.amazon.com", url[1],
              AutocompleteMatchType::NAVSUGGEST_PERSONALIZED },
           kEmptyMatch,
         },
       },
       // Personalized queries or a personalized URL without deletion URLs
       // shouldn't cause errors.
       { "a",
         "[\"a\",[\"ab\", \"ac\"],[],[],"
         "{\"google:suggesttype\":[\"PERSONALIZED_QUERY\",\"QUERY\","
         "\"PERSONALIZED_NAVIGATION\"],"
         "\"google:suggestrelevance\":[1, 2],"
         "\"google:suggestdetail\":[{}, {}]}]",
         { { "a", "", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
           { "ac", "", AutocompleteMatchType::SEARCH_SUGGEST },
           { "ab", "", AutocompleteMatchType::SEARCH_SUGGEST },
           { "www.amazon.com", "",
              AutocompleteMatchType::NAVSUGGEST_PERSONALIZED },
           kEmptyMatch,
         },
       },
       // Personalized queries or a personalized URL without
       // google:suggestdetail shouldn't cause errors.
       { "a",
         "[\"a\",[\"ab\", \"ac\"],[],[],"
         "{\"google:suggesttype\":[\"PERSONALIZED_QUERY\",\"QUERY\","
         "\"PERSONALIZED_NAVIGATION\"],"
         "\"google:suggestrelevance\":[1, 2]}]",
         { { "a", "", AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
           { "ac", "", AutocompleteMatchType::SEARCH_SUGGEST },
           { "ab", "", AutocompleteMatchType::SEARCH_SUGGEST },
           { "www.amazon.com", "",
              AutocompleteMatchType::NAVSUGGEST_PERSONALIZED },
           kEmptyMatch,
         },
       },
     };

     for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
       QueryForInput(ASCIIToUTF16(cases[i].input_text), false, false);

       net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(
           SearchProvider::kDefaultProviderURLFetcherID);
       ASSERT_TRUE(fetcher);
       fetcher->set_response_code(200);
       fetcher->SetResponseString(cases[i].response_json);
       fetcher->delegate()->OnURLFetchComplete(fetcher);

       RunTillProviderDone();

       const ACMatches& matches = provider_->matches();
       ASSERT_FALSE(matches.empty());

       SCOPED_TRACE("for input with json = " + cases[i].response_json);

       for (size_t j = 0; j < matches.size(); ++j) {
         const Match& match = cases[i].matches[j];
         SCOPED_TRACE(" and match index: " + base::IntToString(j));
         EXPECT_EQ(match.contents, base::UTF16ToUTF8(matches[j].contents));
         EXPECT_EQ(match.deletion_url, matches[j].GetAdditionalInfo(
             "deletion_url"));
       }
     }
}

TEST_F(SearchProviderTest, ReflectsBookmarkBarState) {
  profile_.GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, false);
  base::string16 term = term1_.substr(0, term1_.length() - 1);
  QueryForInput(term, true, false);
  ASSERT_FALSE(provider_->matches().empty());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            provider_->matches()[0].type);
  ASSERT_TRUE(provider_->matches()[0].search_terms_args != NULL);
  EXPECT_FALSE(provider_->matches()[0].search_terms_args->bookmark_bar_pinned);

  profile_.GetPrefs()->SetBoolean(prefs::kShowBookmarkBar, true);
  term = term1_.substr(0, term1_.length() - 1);
  QueryForInput(term, true, false);
  ASSERT_FALSE(provider_->matches().empty());
  EXPECT_EQ(AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED,
            provider_->matches()[0].type);
  ASSERT_TRUE(provider_->matches()[0].search_terms_args != NULL);
  EXPECT_TRUE(provider_->matches()[0].search_terms_args->bookmark_bar_pinned);
}

TEST_F(SearchProviderTest, CanSendURL) {
  TemplateURLData template_url_data;
  template_url_data.short_name = ASCIIToUTF16("t");
  template_url_data.SetURL("http://www.google.com/{searchTerms}");
  template_url_data.suggestions_url = "http://www.google.com/{searchTerms}";
  template_url_data.instant_url = "http://does/not/exist?strk=1";
  template_url_data.search_terms_replacement_key = "strk";
  template_url_data.id = SEARCH_ENGINE_GOOGLE;
  TemplateURL google_template_url(template_url_data);

  // Create field trial.
  CreateZeroSuggestFieldTrial(true);

  // Not signed in.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), &profile_));
  SigninManagerBase* signin = SigninManagerFactory::GetForProfile(&profile_);
  signin->SetAuthenticatedUsername("test");

  // All conditions should be met.
  EXPECT_TRUE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), &profile_));

  // Not in field trial.
  ResetFieldTrialList();
  CreateZeroSuggestFieldTrial(false);
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), &profile_));
  ResetFieldTrialList();
  CreateZeroSuggestFieldTrial(true);

  // Invalid page URL.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("badpageurl"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), &profile_));

  // Invalid page classification.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS,
      SearchTermsData(), &profile_));

  // Invalid page classification.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      SearchTermsData(), &profile_));

  // HTTPS page URL on same domain as provider.
  EXPECT_TRUE(SearchProvider::CanSendURL(
      GURL("https://www.google.com/search"),
      GURL("https://www.google.com/complete/search"),
      &google_template_url, metrics::OmniboxEventProto::OTHER,
      SearchTermsData(), &profile_));

  // Non-HTTP[S] page URL on same domain as provider.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("ftp://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), &profile_));

  // Non-HTTP page URL on different domain.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("https://www.notgoogle.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), &profile_));

  // Non-HTTPS provider.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("http://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), &profile_));

  // Suggest disabled.
  profile_.GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, false);
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), &profile_));
  profile_.GetPrefs()->SetBoolean(prefs::kSearchSuggestEnabled, true);

  // Incognito.
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(),
      profile_.GetOffTheRecordProfile()));

  // Tab sync not enabled.
  profile_.GetPrefs()->SetBoolean(sync_driver::prefs::kSyncKeepEverythingSynced,
                                  false);
  profile_.GetPrefs()->SetBoolean(sync_driver::prefs::kSyncTabs, false);
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), &profile_));
  profile_.GetPrefs()->SetBoolean(sync_driver::prefs::kSyncTabs, true);

  // Tab sync is encrypted.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(&profile_);
  syncer::ModelTypeSet encrypted_types = service->GetEncryptedDataTypes();
  encrypted_types.Put(syncer::SESSIONS);
  service->OnEncryptedTypesChanged(encrypted_types, false);
  EXPECT_FALSE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), &profile_));
  encrypted_types.Remove(syncer::SESSIONS);
  service->OnEncryptedTypesChanged(encrypted_types, false);

  // Check that there were no side effects from previous tests.
  EXPECT_TRUE(SearchProvider::CanSendURL(
      GURL("http://www.google.com/search"),
      GURL("https://www.google.com/complete/search"), &google_template_url,
      metrics::OmniboxEventProto::OTHER, SearchTermsData(), &profile_));
}

TEST_F(SearchProviderTest, TestDeleteMatch) {
  AutocompleteMatch match(provider_, 0, true,
                          AutocompleteMatchType::SEARCH_SUGGEST);
  match.RecordAdditionalInfo(
      SearchProvider::kDeletionUrlKey,
      "https://www.google.com/complete/deleteitem?q=foo");

  // Test a successful deletion request.
  provider_->matches_.push_back(match);
  provider_->DeleteMatch(match);
  EXPECT_FALSE(provider_->deletion_handlers_.empty());
  EXPECT_TRUE(provider_->matches_.empty());
  // Set up a default fetcher with provided results.
  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDeletionURLFetcherID);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(200);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_TRUE(provider_->deletion_handlers_.empty());
  EXPECT_TRUE(provider_->is_success());

  // Test a failing deletion request.
  provider_->matches_.push_back(match);
  provider_->DeleteMatch(match);
  EXPECT_FALSE(provider_->deletion_handlers_.empty());
  // Set up a default fetcher with provided results.
  fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDeletionURLFetcherID);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(500);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  EXPECT_TRUE(provider_->deletion_handlers_.empty());
  EXPECT_FALSE(provider_->is_success());
}

TEST_F(SearchProviderTest, TestDeleteHistoryQueryMatch) {
  GURL term_url(
      AddSearchToHistory(default_t_url_, ASCIIToUTF16("flash games"), 1));
  profile_.BlockUntilHistoryProcessesPendingRequests();

  AutocompleteMatch games;
  QueryForInput(ASCIIToUTF16("fla"), false, false);
  profile_.BlockUntilHistoryProcessesPendingRequests();
  ASSERT_NO_FATAL_FAILURE(FinishDefaultSuggestQuery());
  ASSERT_TRUE(FindMatchWithContents(ASCIIToUTF16("flash games"), &games));

  size_t matches_before = provider_->matches().size();
  provider_->DeleteMatch(games);
  EXPECT_EQ(matches_before - 1, provider_->matches().size());

  // Process history deletions.
  profile_.BlockUntilHistoryProcessesPendingRequests();

  // Check that the match is gone.
  QueryForInput(ASCIIToUTF16("fla"), false, false);
  profile_.BlockUntilHistoryProcessesPendingRequests();
  ASSERT_NO_FATAL_FAILURE(FinishDefaultSuggestQuery());
  EXPECT_FALSE(FindMatchWithContents(ASCIIToUTF16("flash games"), &games));
}

// Verifies that duplicates are preserved in AddMatchToMap().
TEST_F(SearchProviderTest, CheckDuplicateMatchesSaved) {
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("a"), 1);
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("alpha"), 1);
  AddSearchToHistory(default_t_url_, ASCIIToUTF16("avid"), 1);

  profile_.BlockUntilHistoryProcessesPendingRequests();
  QueryForInput(ASCIIToUTF16("a"), false, false);

  // Make sure the default provider's suggest service was queried.
  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(fetcher);

  // Tell the SearchProvider the suggest query is done.
  fetcher->set_response_code(200);
  fetcher->SetResponseString(
      "[\"a\",[\"a\", \"alpha\", \"avid\", \"apricot\"],[],[],"
      "{\"google:suggestrelevance\":[1450, 1200, 1150, 1100],"
      "\"google:verbatimrelevance\":1350}]");
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  fetcher = NULL;

  // Run till the history results complete.
  RunTillProviderDone();

  AutocompleteMatch verbatim, match_alpha, match_apricot, match_avid;
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("a"), &verbatim));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("alpha"), &match_alpha));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("apricot"), &match_apricot));
  EXPECT_TRUE(FindMatchWithContents(ASCIIToUTF16("avid"), &match_avid));

  // Verbatim match duplicates are added such that each one has a higher
  // relevance than the previous one.
  EXPECT_EQ(2U, verbatim.duplicate_matches.size());

  // Other match duplicates are added in descending relevance order.
  EXPECT_EQ(1U, match_alpha.duplicate_matches.size());
  EXPECT_EQ(1U, match_avid.duplicate_matches.size());

  EXPECT_EQ(0U, match_apricot.duplicate_matches.size());
}

TEST_F(SearchProviderTest, SuggestQueryUsesToken) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableAnswersInSuggest);

  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(&profile_);

  TemplateURLData data;
  data.short_name = ASCIIToUTF16("default");
  data.SetKeyword(data.short_name);
  data.SetURL("http://example/{searchTerms}{google:sessionToken}");
  data.suggestions_url =
      "http://suggest/?q={searchTerms}&{google:sessionToken}";
  default_t_url_ = new TemplateURL(data);
  turl_model->Add(default_t_url_);
  turl_model->SetUserSelectedDefaultSearchProvider(default_t_url_);

  base::string16 term = term1_.substr(0, term1_.length() - 1);
  QueryForInput(term, false, false);

  // Make sure the default provider's suggest service was queried.
  net::TestURLFetcher* fetcher = test_factory_.GetFetcherByID(
      SearchProvider::kDefaultProviderURLFetcherID);
  ASSERT_TRUE(fetcher);

  // And the URL matches what we expected.
  TemplateURLRef::SearchTermsArgs search_terms_args(term);
  search_terms_args.session_token = provider_->current_token_;
  GURL expected_url(default_t_url_->suggestions_url_ref().ReplaceSearchTerms(
      search_terms_args, turl_model->search_terms_data()));
  EXPECT_EQ(fetcher->GetOriginalURL().spec(), expected_url.spec());

  // Complete running the fetcher to clean up.
  fetcher->set_response_code(200);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
  RunTillProviderDone();
}

TEST_F(SearchProviderTest, SessionToken) {
  // Subsequent calls always get the same token.
  std::string token = provider_->GetSessionToken();
  std::string token2 = provider_->GetSessionToken();
  EXPECT_EQ(token, token2);
  EXPECT_FALSE(token.empty());

  // Calls do not regenerate a token.
  provider_->current_token_ = "PRE-EXISTING TOKEN";
  token = provider_->GetSessionToken();
  EXPECT_EQ(token, "PRE-EXISTING TOKEN");

  // ... unless the token has expired.
  provider_->current_token_.clear();
  const base::TimeDelta kSmallDelta = base::TimeDelta::FromMilliseconds(1);
  provider_->token_expiration_time_ = base::TimeTicks::Now() - kSmallDelta;
  token = provider_->GetSessionToken();
  EXPECT_FALSE(token.empty());
  EXPECT_EQ(token, provider_->current_token_);

  // The expiration time is always updated.
  provider_->GetSessionToken();
  base::TimeTicks expiration_time_1 = provider_->token_expiration_time_;
  base::PlatformThread::Sleep(kSmallDelta);
  provider_->GetSessionToken();
  base::TimeTicks expiration_time_2 = provider_->token_expiration_time_;
  EXPECT_GT(expiration_time_2, expiration_time_1);
  EXPECT_GE(expiration_time_2, expiration_time_1 + kSmallDelta);
}

TEST_F(SearchProviderTest, AnswersCache) {
  // Initial condition: empty cache.
  ASSERT_TRUE(provider_->last_answer_seen_.full_query_text.empty());

  AutocompleteResult result;
  ACMatches matches;
  AutocompleteMatch match1;
  match1.answer_contents = base::ASCIIToUTF16("m1");
  match1.answer_type = base::ASCIIToUTF16("2334");
  match1.fill_into_edit = base::ASCIIToUTF16("weather los angeles");

  AutocompleteMatch non_answer_match1;
  non_answer_match1.fill_into_edit = base::ASCIIToUTF16("weather laguna beach");

  // Test that an answer in the first slot populates the cache.
  matches.push_back(match1);
  matches.push_back(non_answer_match1);
  result.AppendMatches(matches);
  provider_->RegisterDisplayedAnswers(result);
  EXPECT_EQ(base::ASCIIToUTF16("weather los angeles"),
            provider_->last_answer_seen_.full_query_text);
  EXPECT_EQ(base::ASCIIToUTF16("2334"),
            provider_->last_answer_seen_.query_type);

  // Test that DoAnswersQuery retrieves data from cache.
  AutocompleteInput input(base::ASCIIToUTF16("weather l"),
                          base::string16::npos, base::string16(), GURL(),
                          metrics::OmniboxEventProto::INVALID_SPEC, false,
                          false, true, true,
                          ChromeAutocompleteSchemeClassifier(&profile_));
  provider_->DoAnswersQuery(input);
  EXPECT_EQ(base::ASCIIToUTF16("weather los angeles"),
            provider_->prefetch_data_.full_query_text);
  EXPECT_EQ(base::ASCIIToUTF16("2334"), provider_->prefetch_data_.query_type);

  // Mismatching input will return empty prefetch data.
  AutocompleteInput input2(base::ASCIIToUTF16("weather n"),
                           base::string16::npos, base::string16(), GURL(),
                           metrics::OmniboxEventProto::INVALID_SPEC, false,
                           false, true, true,
                           ChromeAutocompleteSchemeClassifier(&profile_));
  provider_->DoAnswersQuery(input2);
  EXPECT_TRUE(provider_->prefetch_data_.full_query_text.empty());
  EXPECT_TRUE(provider_->prefetch_data_.query_type.empty());
}
