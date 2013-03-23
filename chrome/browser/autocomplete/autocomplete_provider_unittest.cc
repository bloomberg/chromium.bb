// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_provider.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "testing/gtest/include/gtest/gtest.h"

static std::ostream& operator<<(std::ostream& os,
                                const AutocompleteResult::const_iterator& it) {
  return os << static_cast<const AutocompleteMatch*>(&(*it));
}

namespace {
const size_t kResultsPerProvider = 3;
const char kTestTemplateURLKeyword[] = "t";
}

// Autocomplete provider that provides known results. Note that this is
// refcounted so that it can also be a task on the message loop.
class TestProvider : public AutocompleteProvider {
 public:
  TestProvider(int relevance, const string16& prefix,
               Profile* profile,
               const string16 match_keyword)
      : AutocompleteProvider(NULL, profile, AutocompleteProvider::TYPE_SEARCH),
        relevance_(relevance),
        prefix_(prefix),
        match_keyword_(match_keyword) {
  }

  virtual void Start(const AutocompleteInput& input,
                     bool minimal_changes) OVERRIDE;

  void set_listener(AutocompleteProviderListener* listener) {
    listener_ = listener;
  }

 private:
  virtual ~TestProvider() {}

  void Run();

  void AddResults(int start_at, int num);
  void AddResultsWithSearchTermsArgs(
      int start_at,
      int num,
      AutocompleteMatch::Type type,
      const TemplateURLRef::SearchTermsArgs& search_terms_args);

  int relevance_;
  const string16 prefix_;
  const string16 match_keyword_;
};

void TestProvider::Start(const AutocompleteInput& input,
                         bool minimal_changes) {
  if (minimal_changes)
    return;

  matches_.clear();

  // Generate 4 results synchronously, the rest later.
  AddResults(0, 1);
  AddResultsWithSearchTermsArgs(
      1, 1, AutocompleteMatch::SEARCH_WHAT_YOU_TYPED,
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("echo")));
  AddResultsWithSearchTermsArgs(
      2, 1, AutocompleteMatch::NAVSUGGEST,
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("nav")));
  AddResultsWithSearchTermsArgs(
      3, 1, AutocompleteMatch::SEARCH_SUGGEST,
      TemplateURLRef::SearchTermsArgs(ASCIIToUTF16("query")));

  if (input.matches_requested() == AutocompleteInput::ALL_MATCHES) {
    done_ = false;
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(&TestProvider::Run,
                                                           this));
  }
}

void TestProvider::Run() {
  DCHECK_GT(kResultsPerProvider, 0U);
  AddResults(1, kResultsPerProvider);
  done_ = true;
  DCHECK(listener_);
  listener_->OnProviderUpdate(true);
}

void TestProvider::AddResults(int start_at, int num) {
  AddResultsWithSearchTermsArgs(start_at,
                                num,
                                AutocompleteMatch::URL_WHAT_YOU_TYPED,
                                TemplateURLRef::SearchTermsArgs(string16()));
}

void TestProvider::AddResultsWithSearchTermsArgs(
    int start_at,
    int num,
    AutocompleteMatch::Type type,
    const TemplateURLRef::SearchTermsArgs& search_terms_args) {
  for (int i = start_at; i < num; i++) {
    AutocompleteMatch match(this, relevance_ - i, false, type);

    match.fill_into_edit = prefix_ + UTF8ToUTF16(base::IntToString(i));
    match.destination_url = GURL(UTF16ToUTF8(match.fill_into_edit));

    match.contents = match.fill_into_edit;
    match.contents_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    match.description = match.fill_into_edit;
    match.description_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));
    match.search_terms_args.reset(
        new TemplateURLRef::SearchTermsArgs(search_terms_args));
    if (!match_keyword_.empty()) {
      match.keyword = match_keyword_;
      ASSERT_TRUE(match.GetTemplateURL(profile_, false) != NULL);
    }

    matches_.push_back(match);
  }
}

class AutocompleteProviderTest : public testing::Test,
                                 public content::NotificationObserver {
 protected:
  struct KeywordTestData {
    const string16 fill_into_edit;
    const string16 keyword;
    const bool expected_keyword_result;
  };

  struct AssistedQueryStatsTestData {
    const AutocompleteMatch::Type match_type;
    const std::string expected_aqs;
  };

 protected:
   // Registers a test TemplateURL under the given keyword.
  void RegisterTemplateURL(const string16 keyword,
                           const std::string& template_url);

  // Resets |controller_| with two TestProviders.  |provider1_ptr| and
  // |provider2_ptr| are updated to point to the new providers if non-NULL.
  void ResetControllerWithTestProviders(bool same_destinations,
                                        TestProvider** provider1_ptr,
                                        TestProvider** provider2_ptr);

  // Runs a query on the input "a", and makes sure both providers' input is
  // properly collected.
  void RunTest();

  void RunRedundantKeywordTest(const KeywordTestData* match_data, size_t size);

  void RunAssistedQueryStatsTest(
      const AssistedQueryStatsTestData* aqs_test_data,
      size_t size);

  void RunQuery(const string16 query);

  void ResetControllerWithKeywordAndSearchProviders();
  void ResetControllerWithKeywordProvider();
  void RunExactKeymatchTest(bool allow_exact_keyword_match);

  AutocompleteResult result_;
  scoped_ptr<AutocompleteController> controller_;

 private:
  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  MessageLoopForUI message_loop_;
  content::NotificationRegistrar registrar_;
  TestingProfile profile_;
};

void AutocompleteProviderTest::RegisterTemplateURL(
    const string16 keyword,
    const std::string& template_url) {
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &TemplateURLServiceFactory::BuildInstanceFor);
  TemplateURLData data;
  data.SetURL(template_url);
  data.SetKeyword(keyword);
  TemplateURL* default_t_url = new TemplateURL(&profile_, data);
  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(&profile_);
  turl_model->Add(default_t_url);
  turl_model->SetDefaultSearchProvider(default_t_url);
  turl_model->Load();
  TemplateURLID default_provider_id = default_t_url->id();
  ASSERT_NE(0, default_provider_id);
}

void AutocompleteProviderTest::ResetControllerWithTestProviders(
    bool same_destinations,
    TestProvider** provider1_ptr,
    TestProvider** provider2_ptr) {
  // TODO: Move it outside this method, after refactoring the existing
  // unit tests.  Specifically:
  //   (1) Make sure that AutocompleteMatch.keyword is set iff there is
  //       a corresponding call to RegisterTemplateURL; otherwise the
  //       controller flow will crash; this practically means that
  //       RunTests/ResetControllerXXX/RegisterTemplateURL should
  //       be coordinated with each other.
  //   (2) Inject test arguments rather than rely on the hardcoded values, e.g.
  //       don't rely on kResultsPerProvided and default relevance ordering
  //       (B > A).
  RegisterTemplateURL(ASCIIToUTF16(kTestTemplateURLKeyword),
                      "http://aqs/{searchTerms}/{google:assistedQueryStats}");

  ACProviders providers;

  // Construct two new providers, with either the same or different prefixes.
  TestProvider* provider1 = new TestProvider(
      kResultsPerProvider,
      ASCIIToUTF16("http://a"),
      &profile_,
      ASCIIToUTF16(kTestTemplateURLKeyword));
  provider1->AddRef();
  providers.push_back(provider1);

  TestProvider* provider2 = new TestProvider(
      kResultsPerProvider * 2,
      same_destinations ? ASCIIToUTF16("http://a") : ASCIIToUTF16("http://b"),
      &profile_,
      string16());
  provider2->AddRef();
  providers.push_back(provider2);

  // Reset the controller to contain our new providers.
  controller_.reset(new AutocompleteController(&profile_, NULL, 0));
  // We're going to swap the providers vector, but the old vector should be
  // empty so no elements need to be freed at this point.
  EXPECT_TRUE(controller_->providers_.empty());
  controller_->providers_.swap(providers);
  provider1->set_listener(controller_.get());
  provider2->set_listener(controller_.get());

  // The providers don't complete synchronously, so listen for "result updated"
  // notifications.
  registrar_.Add(this,
                 chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
                 content::Source<AutocompleteController>(controller_.get()));

  if (provider1_ptr)
    *provider1_ptr = provider1;
  if (provider2_ptr)
    *provider2_ptr = provider2;
}

void AutocompleteProviderTest::
    ResetControllerWithKeywordAndSearchProviders() {
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &TemplateURLServiceFactory::BuildInstanceFor);

  // Reset the default TemplateURL.
  TemplateURLData data;
  data.SetURL("http://defaultturl/{searchTerms}");
  TemplateURL* default_t_url = new TemplateURL(&profile_, data);
  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(&profile_);
  turl_model->Add(default_t_url);
  turl_model->SetDefaultSearchProvider(default_t_url);
  TemplateURLID default_provider_id = default_t_url->id();
  ASSERT_NE(0, default_provider_id);

  // Create another TemplateURL for KeywordProvider.
  data.short_name = ASCIIToUTF16("k");
  data.SetKeyword(ASCIIToUTF16("k"));
  data.SetURL("http://keyword/{searchTerms}");
  TemplateURL* keyword_t_url = new TemplateURL(&profile_, data);
  turl_model->Add(keyword_t_url);
  ASSERT_NE(0, keyword_t_url->id());

  controller_.reset(new AutocompleteController(
      &profile_, NULL,
      AutocompleteProvider::TYPE_KEYWORD | AutocompleteProvider::TYPE_SEARCH));
}

void AutocompleteProviderTest::ResetControllerWithKeywordProvider() {
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, &TemplateURLServiceFactory::BuildInstanceFor);

  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(&profile_);

  // Create a TemplateURL for KeywordProvider.
  TemplateURLData data;
  data.short_name = ASCIIToUTF16("foo.com");
  data.SetKeyword(ASCIIToUTF16("foo.com"));
  data.SetURL("http://foo.com/{searchTerms}");
  TemplateURL* keyword_t_url = new TemplateURL(&profile_, data);
  turl_model->Add(keyword_t_url);
  ASSERT_NE(0, keyword_t_url->id());

  // Create another TemplateURL for KeywordProvider.
  data.short_name = ASCIIToUTF16("bar.com");
  data.SetKeyword(ASCIIToUTF16("bar.com"));
  data.SetURL("http://bar.com/{searchTerms}");
  keyword_t_url = new TemplateURL(&profile_, data);
  turl_model->Add(keyword_t_url);
  ASSERT_NE(0, keyword_t_url->id());

  controller_.reset(new AutocompleteController(
      &profile_, NULL, AutocompleteProvider::TYPE_KEYWORD));
}

void AutocompleteProviderTest::RunTest() {
  RunQuery(ASCIIToUTF16("a"));
}

void AutocompleteProviderTest::RunRedundantKeywordTest(
    const KeywordTestData* match_data,
    size_t size) {
  ACMatches matches;
  for (size_t i = 0; i < size; ++i) {
    AutocompleteMatch match;
    match.fill_into_edit = match_data[i].fill_into_edit;
    match.transition = content::PAGE_TRANSITION_KEYWORD;
    match.keyword = match_data[i].keyword;
    matches.push_back(match);
  }

  AutocompleteResult result;
  result.AppendMatches(matches);
  controller_->UpdateAssociatedKeywords(&result);

  for (size_t j = 0; j < result.size(); ++j) {
    EXPECT_EQ(match_data[j].expected_keyword_result,
        result.match_at(j)->associated_keyword.get() != NULL);
  }
}

void AutocompleteProviderTest::RunAssistedQueryStatsTest(
    const AssistedQueryStatsTestData* aqs_test_data,
    size_t size) {
  // Prepare input.
  const size_t kMaxRelevance = 1000;
  ACMatches matches;
  for (size_t i = 0; i < size; ++i) {
    AutocompleteMatch match(NULL, kMaxRelevance - i, false,
                            aqs_test_data[i].match_type);
    match.keyword = ASCIIToUTF16(kTestTemplateURLKeyword);
    match.search_terms_args.reset(
        new TemplateURLRef::SearchTermsArgs(string16()));
    matches.push_back(match);
  }
  result_.Reset();
  result_.AppendMatches(matches);

  // Update AQS.
  controller_->UpdateAssistedQueryStats(&result_);

  // Verify data.
  for (size_t i = 0; i < size; ++i) {
    EXPECT_EQ(aqs_test_data[i].expected_aqs,
              result_.match_at(i)->search_terms_args->assisted_query_stats);
  }
}

void AutocompleteProviderTest::RunQuery(const string16 query) {
  result_.Reset();
  controller_->Start(AutocompleteInput(
      query, string16::npos, string16(), GURL(), true, false, true,
      AutocompleteInput::ALL_MATCHES));

  if (!controller_->done())
    // The message loop will terminate when all autocomplete input has been
    // collected.
    MessageLoop::current()->Run();
}

void AutocompleteProviderTest::RunExactKeymatchTest(
    bool allow_exact_keyword_match) {
  // Send the controller input which exactly matches the keyword provider we
  // created in ResetControllerWithKeywordAndSearchProviders().  The default
  // match should thus be a search-other-engine match iff
  // |allow_exact_keyword_match| is true.  Regardless, the match should
  // be from SearchProvider.  (It provides all verbatim search matches,
  // keyword or not.)
  controller_->Start(AutocompleteInput(
      ASCIIToUTF16("k test"), string16::npos, string16(), GURL(), true, false,
      allow_exact_keyword_match, AutocompleteInput::SYNCHRONOUS_MATCHES));
  EXPECT_TRUE(controller_->done());
  EXPECT_EQ(AutocompleteProvider::TYPE_SEARCH,
      controller_->result().default_match()->provider->type());
  EXPECT_EQ(allow_exact_keyword_match ?
      AutocompleteMatch::SEARCH_OTHER_ENGINE :
      AutocompleteMatch::SEARCH_WHAT_YOU_TYPED,
      controller_->result().default_match()->type);
}

void AutocompleteProviderTest::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (controller_->done()) {
    result_.CopyFrom(controller_->result());
    MessageLoop::current()->Quit();
  }
}

// Tests that the default selection is set properly when updating results.
TEST_F(AutocompleteProviderTest, Query) {
  TestProvider* provider1 = NULL;
  TestProvider* provider2 = NULL;
  ResetControllerWithTestProviders(false, &provider1, &provider2);
  RunTest();

  // Make sure the default match gets set to the highest relevance match.  The
  // highest relevance matches should come from the second provider.
  EXPECT_EQ(kResultsPerProvider * 2, result_.size());  // two providers
  ASSERT_NE(result_.end(), result_.default_match());
  EXPECT_EQ(provider2, result_.default_match()->provider);
}

// Tests assisted query stats.
TEST_F(AutocompleteProviderTest, AssistedQueryStats) {
  ResetControllerWithTestProviders(false, NULL, NULL);
  RunTest();

  EXPECT_EQ(kResultsPerProvider * 2, result_.size());  // two providers

  // Now, check the results from the second provider, as they should not have
  // assisted query stats set.
  for (size_t i = 0; i < kResultsPerProvider; ++i) {
    EXPECT_TRUE(
        result_.match_at(i)->search_terms_args->assisted_query_stats.empty());
  }
  // The first provider has a test keyword, so AQS should be non-empty.
  for (size_t i = kResultsPerProvider; i < kResultsPerProvider * 2; ++i) {
    EXPECT_FALSE(
        result_.match_at(i)->search_terms_args->assisted_query_stats.empty());
  }
}

TEST_F(AutocompleteProviderTest, RemoveDuplicates) {
  TestProvider* provider1 = NULL;
  TestProvider* provider2 = NULL;
  ResetControllerWithTestProviders(true, &provider1, &provider2);
  RunTest();

  // Make sure all the first provider's results were eliminated by the second
  // provider's.
  EXPECT_EQ(kResultsPerProvider, result_.size());
  for (AutocompleteResult::const_iterator i(result_.begin());
       i != result_.end(); ++i)
    EXPECT_EQ(provider2, i->provider);
}

TEST_F(AutocompleteProviderTest, AllowExactKeywordMatch) {
  ResetControllerWithKeywordAndSearchProviders();
  RunExactKeymatchTest(true);
  RunExactKeymatchTest(false);
}

// Test that redundant associated keywords are removed.
TEST_F(AutocompleteProviderTest, RedundantKeywordsIgnoredInResult) {
  ResetControllerWithKeywordProvider();

  // Get the controller's internal members in the correct state.
  RunQuery(ASCIIToUTF16("fo"));

  {
    KeywordTestData duplicate_url[] = {
      { ASCIIToUTF16("fo"), string16(), false },
      { ASCIIToUTF16("foo.com"), string16(), true },
      { ASCIIToUTF16("foo.com"), string16(), false }
    };

    SCOPED_TRACE("Duplicate url");
    RunRedundantKeywordTest(duplicate_url, ARRAYSIZE_UNSAFE(duplicate_url));
  }

  {
    KeywordTestData keyword_match[] = {
      { ASCIIToUTF16("foo.com"), ASCIIToUTF16("foo.com"), false },
      { ASCIIToUTF16("foo.com"), string16(), false }
    };

    SCOPED_TRACE("Duplicate url with keyword match");
    RunRedundantKeywordTest(keyword_match, ARRAYSIZE_UNSAFE(keyword_match));
  }

  {
    KeywordTestData multiple_keyword[] = {
      { ASCIIToUTF16("fo"), string16(), false },
      { ASCIIToUTF16("foo.com"), string16(), true },
      { ASCIIToUTF16("foo.com"), string16(), false },
      { ASCIIToUTF16("bar.com"), string16(), true },
    };

    SCOPED_TRACE("Duplicate url with multiple keywords");
    RunRedundantKeywordTest(multiple_keyword,
        ARRAYSIZE_UNSAFE(multiple_keyword));
  }
}

TEST_F(AutocompleteProviderTest, UpdateAssistedQueryStats) {
  ResetControllerWithTestProviders(false, NULL, NULL);

  {
    AssistedQueryStatsTestData test_data[] = {
      //  MSVC doesn't support zero-length arrays, so supply some dummy data.
      { AutocompleteMatch::SEARCH_WHAT_YOU_TYPED, "" }
    };
    SCOPED_TRACE("No matches");
    // Note: We pass 0 here to ignore the dummy data above.
    RunAssistedQueryStatsTest(test_data, 0);
  }

  {
    AssistedQueryStatsTestData test_data[] = {
      { AutocompleteMatch::SEARCH_WHAT_YOU_TYPED, "chrome.0.57" }
    };
    SCOPED_TRACE("One match");
    RunAssistedQueryStatsTest(test_data, ARRAYSIZE_UNSAFE(test_data));
  }

  {
    AssistedQueryStatsTestData test_data[] = {
      { AutocompleteMatch::SEARCH_WHAT_YOU_TYPED, "chrome.0.57j58j5l2j0l3j59" },
      { AutocompleteMatch::URL_WHAT_YOU_TYPED, "chrome.1.57j58j5l2j0l3j59" },
      { AutocompleteMatch::NAVSUGGEST, "chrome.2.57j58j5l2j0l3j59" },
      { AutocompleteMatch::NAVSUGGEST, "chrome.3.57j58j5l2j0l3j59" },
      { AutocompleteMatch::SEARCH_SUGGEST, "chrome.4.57j58j5l2j0l3j59" },
      { AutocompleteMatch::SEARCH_SUGGEST, "chrome.5.57j58j5l2j0l3j59" },
      { AutocompleteMatch::SEARCH_SUGGEST, "chrome.6.57j58j5l2j0l3j59" },
      { AutocompleteMatch::SEARCH_HISTORY, "chrome.7.57j58j5l2j0l3j59" },
    };
    SCOPED_TRACE("Multiple matches");
    RunAssistedQueryStatsTest(test_data, ARRAYSIZE_UNSAFE(test_data));
  }
}

TEST_F(AutocompleteProviderTest, GetDestinationURL) {
  ResetControllerWithKeywordAndSearchProviders();

  // For the destination URL to have aqs parameters for query formulation time
  // and the field trial triggered bit, many conditions need to be satisfied.
  AutocompleteMatch match(NULL, 1100, false,
                          AutocompleteMatch::SEARCH_SUGGEST);
  GURL url = controller_->
      GetDestinationURL(match, base::TimeDelta::FromMilliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // The protocol needs to be https.
  RegisterTemplateURL(ASCIIToUTF16(kTestTemplateURLKeyword),
                      "https://aqs/{searchTerms}/{google:assistedQueryStats}");
  url = controller_->GetDestinationURL(match,
                                       base::TimeDelta::FromMilliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // There needs to be a keyword provider.
  match.keyword = ASCIIToUTF16(kTestTemplateURLKeyword);
  url = controller_->GetDestinationURL(match,
                                       base::TimeDelta::FromMilliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // search_terms_args needs to be set.
  match.search_terms_args.reset(
      new TemplateURLRef::SearchTermsArgs(string16()));
  url = controller_->GetDestinationURL(match,
                                       base::TimeDelta::FromMilliseconds(2456));
  EXPECT_TRUE(url.path().empty());

  // assisted_query_stats needs to have been previously set.
  match.search_terms_args->assisted_query_stats = "chrome.0.57j58j5l2j0l3j59";
  url = controller_->GetDestinationURL(match,
                                       base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ("//aqs=chrome.0.57j58j5l2j0l3j59.2456j0&", url.path());

  // Test field trial triggered bit set.
  controller_->search_provider_->field_trial_triggered_in_session_ = true;
  EXPECT_TRUE(
      controller_->search_provider_->field_trial_triggered_in_session());
  url = controller_->GetDestinationURL(match,
                                       base::TimeDelta::FromMilliseconds(2456));
  EXPECT_EQ("//aqs=chrome.0.57j58j5l2j0l3j59.2456j1&", url.path());
}
