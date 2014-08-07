// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_result.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/autocomplete_input.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/omnibox/autocomplete_match_type.h"
#include "components/omnibox/autocomplete_provider.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

using metrics::OmniboxEventProto;

namespace {

struct AutocompleteMatchTestData {
  std::string destination_url;
  AutocompleteMatch::Type type;
};

const AutocompleteMatchTestData kVerbatimMatches[] = {
  { "http://search-what-you-typed/",
    AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
  { "http://url-what-you-typed/", AutocompleteMatchType::URL_WHAT_YOU_TYPED },
};

const AutocompleteMatchTestData kNonVerbatimMatches[] = {
  { "http://search-history/", AutocompleteMatchType::SEARCH_HISTORY },
  { "http://history-title/", AutocompleteMatchType::HISTORY_TITLE },
};

// Adds |count| AutocompleteMatches to |matches|.
void PopulateAutocompleteMatchesFromTestData(
    const AutocompleteMatchTestData* data,
    size_t count,
    ACMatches* matches) {
  ASSERT_TRUE(matches != NULL);
  for (size_t i = 0; i < count; ++i) {
    AutocompleteMatch match;
    match.destination_url = GURL(data[i].destination_url);
    match.relevance =
        matches->empty() ? 1300 : (matches->back().relevance - 100);
    match.allowed_to_be_default_match = true;
    match.type = data[i].type;
    matches->push_back(match);
  }
}

}  // namespace

class AutocompleteResultTest : public testing::Test  {
 public:
  struct TestData {
    // Used to build a url for the AutocompleteMatch. The URL becomes
    // "http://" + ('a' + |url_id|) (e.g. an ID of 2 yields "http://b").
    int url_id;

    // ID of the provider.
    int provider_id;

    // Relevance score.
    int relevance;

    // Duplicate matches.
    std::vector<AutocompleteMatch> duplicate_matches;
  };

  AutocompleteResultTest() {
    // Destroy the existing FieldTrialList before creating a new one to avoid
    // a DCHECK.
    field_trial_list_.reset();
    field_trial_list_.reset(new base::FieldTrialList(
        new metrics::SHA1EntropyProvider("foo")));
    variations::testing::ClearAllVariationParams();
  }

  virtual void SetUp() OVERRIDE {
#if defined(OS_ANDROID)
    TemplateURLPrepopulateData::InitCountryCode(
        std::string() /* unknown country code */);
#endif
    test_util_.VerifyLoad();
  }

  // Configures |match| from |data|.
  static void PopulateAutocompleteMatch(const TestData& data,
                                        AutocompleteMatch* match);

  // Adds |count| AutocompleteMatches to |matches|.
  static void PopulateAutocompleteMatches(const TestData* data,
                                          size_t count,
                                          ACMatches* matches);

  // Asserts that |result| has |expected_count| matches matching |expected|.
  void AssertResultMatches(const AutocompleteResult& result,
                           const TestData* expected,
                           size_t expected_count);

  // Creates an AutocompleteResult from |last| and |current|. The two are
  // merged by |CopyOldMatches| and compared by |AssertResultMatches|.
  void RunCopyOldMatchesTest(const TestData* last, size_t last_size,
                             const TestData* current, size_t current_size,
                             const TestData* expected, size_t expected_size);

 protected:
  TemplateURLServiceTestUtil test_util_;

 private:
  scoped_ptr<base::FieldTrialList> field_trial_list_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteResultTest);
};

// static
void AutocompleteResultTest::PopulateAutocompleteMatch(
    const TestData& data,
    AutocompleteMatch* match) {
  match->provider = reinterpret_cast<AutocompleteProvider*>(data.provider_id);
  match->fill_into_edit = base::IntToString16(data.url_id);
  std::string url_id(1, data.url_id + 'a');
  match->destination_url = GURL("http://" + url_id);
  match->relevance = data.relevance;
  match->allowed_to_be_default_match = true;
  match->duplicate_matches = data.duplicate_matches;
}

// static
void AutocompleteResultTest::PopulateAutocompleteMatches(
    const TestData* data,
    size_t count,
    ACMatches* matches) {
  for (size_t i = 0; i < count; ++i) {
    AutocompleteMatch match;
    PopulateAutocompleteMatch(data[i], &match);
    matches->push_back(match);
  }
}

void AutocompleteResultTest::AssertResultMatches(
    const AutocompleteResult& result,
    const TestData* expected,
    size_t expected_count) {
  ASSERT_EQ(expected_count, result.size());
  for (size_t i = 0; i < expected_count; ++i) {
    AutocompleteMatch expected_match;
    PopulateAutocompleteMatch(expected[i], &expected_match);
    const AutocompleteMatch& match = *(result.begin() + i);
    EXPECT_EQ(expected_match.provider, match.provider) << i;
    EXPECT_EQ(expected_match.relevance, match.relevance) << i;
    EXPECT_EQ(expected_match.destination_url.spec(),
              match.destination_url.spec()) << i;
  }
}

void AutocompleteResultTest::RunCopyOldMatchesTest(
    const TestData* last, size_t last_size,
    const TestData* current, size_t current_size,
    const TestData* expected, size_t expected_size) {
  AutocompleteInput input(base::ASCIIToUTF16("a"), base::string16::npos,
                          base::string16(), GURL(),
                          OmniboxEventProto::INVALID_SPEC, false, false, false,
                          true,
                          ChromeAutocompleteSchemeClassifier(
                              test_util_.profile()));

  ACMatches last_matches;
  PopulateAutocompleteMatches(last, last_size, &last_matches);
  AutocompleteResult last_result;
  last_result.AppendMatches(last_matches);
  last_result.SortAndCull(input, test_util_.model());

  ACMatches current_matches;
  PopulateAutocompleteMatches(current, current_size, &current_matches);
  AutocompleteResult current_result;
  current_result.AppendMatches(current_matches);
  current_result.SortAndCull(input, test_util_.model());
  current_result.CopyOldMatches(input, last_result, test_util_.model());

  AssertResultMatches(current_result, expected, expected_size);
}

// Assertion testing for AutocompleteResult::Swap.
TEST_F(AutocompleteResultTest, Swap) {
  AutocompleteResult r1;
  AutocompleteResult r2;

  // Swap with empty shouldn't do anything interesting.
  r1.Swap(&r2);
  EXPECT_EQ(r1.end(), r1.default_match());
  EXPECT_EQ(r2.end(), r2.default_match());

  // Swap with a single match.
  ACMatches matches;
  AutocompleteMatch match;
  match.relevance = 1;
  match.allowed_to_be_default_match = true;
  AutocompleteInput input(base::ASCIIToUTF16("a"), base::string16::npos,
                          base::string16(), GURL(),
                          OmniboxEventProto::INVALID_SPEC, false, false, false,
                          true, ChromeAutocompleteSchemeClassifier(
                              test_util_.profile()));
  matches.push_back(match);
  r1.AppendMatches(matches);
  r1.SortAndCull(input, test_util_.model());
  EXPECT_EQ(r1.begin(), r1.default_match());
  EXPECT_EQ("http://a/", r1.alternate_nav_url().spec());
  r1.Swap(&r2);
  EXPECT_TRUE(r1.empty());
  EXPECT_EQ(r1.end(), r1.default_match());
  EXPECT_TRUE(r1.alternate_nav_url().is_empty());
  ASSERT_FALSE(r2.empty());
  EXPECT_EQ(r2.begin(), r2.default_match());
  EXPECT_EQ("http://a/", r2.alternate_nav_url().spec());
}

// Tests that if the new results have a lower max relevance score than last,
// any copied results have their relevance shifted down.
TEST_F(AutocompleteResultTest, CopyOldMatches) {
  TestData last[] = {
    { 0, 0, 1000 },
    { 1, 0, 500 },
  };
  TestData current[] = {
    { 2, 0, 400 },
  };
  TestData result[] = {
    { 2, 0, 400 },
    { 1, 0, 399 },
  };

  ASSERT_NO_FATAL_FAILURE(
      RunCopyOldMatchesTest(last, ARRAYSIZE_UNSAFE(last),
                            current, ARRAYSIZE_UNSAFE(current),
                            result, ARRAYSIZE_UNSAFE(result)));
}

// Tests that matches are copied correctly from two distinct providers.
TEST_F(AutocompleteResultTest, CopyOldMatches2) {
  TestData last[] = {
    { 0, 0, 1000 },
    { 1, 1, 500 },
    { 2, 0, 400 },
    { 3, 1, 300 },
  };
  TestData current[] = {
    { 4, 0, 1100 },
    { 5, 1, 550 },
  };
  TestData result[] = {
    { 4, 0, 1100 },
    { 5, 1, 550 },
    { 2, 0, 400 },
    { 3, 1, 300 },
  };

  ASSERT_NO_FATAL_FAILURE(
      RunCopyOldMatchesTest(last, ARRAYSIZE_UNSAFE(last),
                            current, ARRAYSIZE_UNSAFE(current),
                            result, ARRAYSIZE_UNSAFE(result)));
}

// Tests that matches with empty destination URLs aren't treated as duplicates
// and culled.
TEST_F(AutocompleteResultTest, SortAndCullEmptyDestinationURLs) {
  TestData data[] = {
    { 1, 0, 500 },
    { 0, 0, 1100 },
    { 1, 0, 1000 },
    { 0, 0, 1300 },
    { 0, 0, 1200 },
  };

  ACMatches matches;
  PopulateAutocompleteMatches(data, arraysize(data), &matches);
  matches[1].destination_url = GURL();
  matches[3].destination_url = GURL();
  matches[4].destination_url = GURL();

  AutocompleteResult result;
  result.AppendMatches(matches);
  AutocompleteInput input(base::string16(), base::string16::npos,
                          base::string16(), GURL(),
                          OmniboxEventProto::INVALID_SPEC, false, false, false,
                          true,
                          ChromeAutocompleteSchemeClassifier(
                              test_util_.profile()));
  result.SortAndCull(input, test_util_.model());

  // Of the two results with the same non-empty destination URL, the
  // lower-relevance one should be dropped.  All of the results with empty URLs
  // should be kept.
  ASSERT_EQ(4U, result.size());
  EXPECT_TRUE(result.match_at(0)->destination_url.is_empty());
  EXPECT_EQ(1300, result.match_at(0)->relevance);
  EXPECT_TRUE(result.match_at(1)->destination_url.is_empty());
  EXPECT_EQ(1200, result.match_at(1)->relevance);
  EXPECT_TRUE(result.match_at(2)->destination_url.is_empty());
  EXPECT_EQ(1100, result.match_at(2)->relevance);
  EXPECT_EQ("http://b/", result.match_at(3)->destination_url.spec());
  EXPECT_EQ(1000, result.match_at(3)->relevance);
}

TEST_F(AutocompleteResultTest, SortAndCullDuplicateSearchURLs) {
  // Register a template URL that corresponds to 'foo' search engine.
  TemplateURLData url_data;
  url_data.short_name = base::ASCIIToUTF16("unittest");
  url_data.SetKeyword(base::ASCIIToUTF16("foo"));
  url_data.SetURL("http://www.foo.com/s?q={searchTerms}");
  test_util_.model()->Add(new TemplateURL(url_data));

  TestData data[] = {
    { 0, 0, 1300 },
    { 1, 0, 1200 },
    { 2, 0, 1100 },
    { 3, 0, 1000 },
    { 4, 1, 900 },
  };

  ACMatches matches;
  PopulateAutocompleteMatches(data, arraysize(data), &matches);
  matches[0].destination_url = GURL("http://www.foo.com/s?q=foo");
  matches[1].destination_url = GURL("http://www.foo.com/s?q=foo2");
  matches[2].destination_url = GURL("http://www.foo.com/s?q=foo&oq=f");
  matches[3].destination_url = GURL("http://www.foo.com/s?q=foo&aqs=0");
  matches[4].destination_url = GURL("http://www.foo.com/");

  AutocompleteResult result;
  result.AppendMatches(matches);
  AutocompleteInput input(base::string16(), base::string16::npos,
                          base::string16(), GURL(),
                          OmniboxEventProto::INVALID_SPEC, false, false, false,
                          true,
                          ChromeAutocompleteSchemeClassifier(
                              test_util_.profile()));
  result.SortAndCull(input, test_util_.model());

  // We expect the 3rd and 4th results to be removed.
  ASSERT_EQ(3U, result.size());
  EXPECT_EQ("http://www.foo.com/s?q=foo",
            result.match_at(0)->destination_url.spec());
  EXPECT_EQ(1300, result.match_at(0)->relevance);
  EXPECT_EQ("http://www.foo.com/s?q=foo2",
            result.match_at(1)->destination_url.spec());
  EXPECT_EQ(1200, result.match_at(1)->relevance);
  EXPECT_EQ("http://www.foo.com/",
            result.match_at(2)->destination_url.spec());
  EXPECT_EQ(900, result.match_at(2)->relevance);
}

TEST_F(AutocompleteResultTest, SortAndCullWithMatchDups) {
  // Register a template URL that corresponds to 'foo' search engine.
  TemplateURLData url_data;
  url_data.short_name = base::ASCIIToUTF16("unittest");
  url_data.SetKeyword(base::ASCIIToUTF16("foo"));
  url_data.SetURL("http://www.foo.com/s?q={searchTerms}");
  test_util_.model()->Add(new TemplateURL(url_data));

  AutocompleteMatch dup_match;
  dup_match.destination_url = GURL("http://www.foo.com/s?q=foo&oq=dup");
  std::vector<AutocompleteMatch> dups;
  dups.push_back(dup_match);

  TestData data[] = {
    { 0, 0, 1300, dups },
    { 1, 0, 1200 },
    { 2, 0, 1100 },
    { 3, 0, 1000, dups },
    { 4, 1, 900 },
    { 5, 0, 800 },
  };

  ACMatches matches;
  PopulateAutocompleteMatches(data, arraysize(data), &matches);
  matches[0].destination_url = GURL("http://www.foo.com/s?q=foo");
  matches[1].destination_url = GURL("http://www.foo.com/s?q=foo2");
  matches[2].destination_url = GURL("http://www.foo.com/s?q=foo&oq=f");
  matches[3].destination_url = GURL("http://www.foo.com/s?q=foo&aqs=0");
  matches[4].destination_url = GURL("http://www.foo.com/");
  matches[5].destination_url = GURL("http://www.foo.com/s?q=foo2&oq=f");

  AutocompleteResult result;
  result.AppendMatches(matches);
  AutocompleteInput input(base::string16(), base::string16::npos,
                          base::string16(), GURL(),
                          OmniboxEventProto::INVALID_SPEC, false, false, false,
                          true,
                          ChromeAutocompleteSchemeClassifier(
                              test_util_.profile()));
  result.SortAndCull(input, test_util_.model());

  // Expect 3 unique results after SortAndCull().
  ASSERT_EQ(3U, result.size());

  // Check that 3rd and 4th result got added to the first result as dups
  // and also duplicates of the 4th match got copied.
  ASSERT_EQ(4U, result.match_at(0)->duplicate_matches.size());
  const AutocompleteMatch* first_match = result.match_at(0);
  EXPECT_EQ(matches[2].destination_url,
            first_match->duplicate_matches.at(1).destination_url);
  EXPECT_EQ(dup_match.destination_url,
            first_match->duplicate_matches.at(2).destination_url);
  EXPECT_EQ(matches[3].destination_url,
            first_match->duplicate_matches.at(3).destination_url);

  // Check that 6th result started a new list of dups for the second result.
  ASSERT_EQ(1U, result.match_at(1)->duplicate_matches.size());
  EXPECT_EQ(matches[5].destination_url,
            result.match_at(1)->duplicate_matches.at(0).destination_url);
}

TEST_F(AutocompleteResultTest, SortAndCullWithDemotionsByType) {
  // Add some matches.
  ACMatches matches;
  const AutocompleteMatchTestData data[] = {
    { "http://history-url/", AutocompleteMatchType::HISTORY_URL },
    { "http://search-what-you-typed/",
      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
    { "http://history-title/", AutocompleteMatchType::HISTORY_TITLE },
    { "http://search-history/", AutocompleteMatchType::SEARCH_HISTORY },
  };
  PopulateAutocompleteMatchesFromTestData(data, arraysize(data), &matches);

  // Demote the search history match relevance score.
  matches.back().relevance = 500;

  // Add a rule demoting history-url and killing history-title.
  {
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kDemoteByTypeRule) + ":3:*"] =
        "1:50,7:100,2:0";  // 3 == HOME_PAGE
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");

  AutocompleteResult result;
  result.AppendMatches(matches);
  AutocompleteInput input(base::string16(), base::string16::npos,
                          base::string16(), GURL(),
                          OmniboxEventProto::HOME_PAGE, false, false, false,
                          true,
                          ChromeAutocompleteSchemeClassifier(
                              test_util_.profile()));
  result.SortAndCull(input, test_util_.model());

  // Check the new ordering.  The history-title results should be omitted.
  // We cannot check relevance scores because the matches are sorted by
  // demoted relevance but the actual relevance scores are not modified.
  ASSERT_EQ(3u, result.size());
  EXPECT_EQ("http://search-what-you-typed/",
            result.match_at(0)->destination_url.spec());
  EXPECT_EQ("http://history-url/",
            result.match_at(1)->destination_url.spec());
  EXPECT_EQ("http://search-history/",
            result.match_at(2)->destination_url.spec());
}

TEST_F(AutocompleteResultTest, SortAndCullWithMatchDupsAndDemotionsByType) {
  // Add some matches.
  ACMatches matches;
  const AutocompleteMatchTestData data[] = {
    { "http://search-what-you-typed/",
      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED },
    { "http://dup-url/", AutocompleteMatchType::HISTORY_URL },
    { "http://dup-url/", AutocompleteMatchType::NAVSUGGEST },
    { "http://search-url/", AutocompleteMatchType::SEARCH_SUGGEST },
    { "http://history-url/", AutocompleteMatchType::HISTORY_URL },
  };
  PopulateAutocompleteMatchesFromTestData(data, arraysize(data), &matches);

  // Add a rule demoting HISTORY_URL.
  {
    std::map<std::string, std::string> params;
    params[std::string(OmniboxFieldTrial::kDemoteByTypeRule) + ":8:*"] =
        "1:50";  // 8 == INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "C", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "C");

  {
    AutocompleteResult result;
    result.AppendMatches(matches);
    AutocompleteInput input(
        base::string16(), base::string16::npos, base::string16(), GURL(),
        OmniboxEventProto::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS, false,
        false, false, true,
        ChromeAutocompleteSchemeClassifier(test_util_.profile()));
    result.SortAndCull(input, test_util_.model());

    // The NAVSUGGEST dup-url stay above search-url since the navsuggest
    // variant should not be demoted.
    ASSERT_EQ(4u, result.size());
    EXPECT_EQ("http://search-what-you-typed/",
              result.match_at(0)->destination_url.spec());
    EXPECT_EQ("http://dup-url/",
              result.match_at(1)->destination_url.spec());
    EXPECT_EQ(AutocompleteMatchType::NAVSUGGEST,
              result.match_at(1)->type);
    EXPECT_EQ("http://search-url/",
              result.match_at(2)->destination_url.spec());
    EXPECT_EQ("http://history-url/",
              result.match_at(3)->destination_url.spec());
  }
}

TEST_F(AutocompleteResultTest, SortAndCullReorderForDefaultMatch) {
  TestData data[] = {
    { 0, 0, 1300 },
    { 1, 0, 1200 },
    { 2, 0, 1100 },
    { 3, 0, 1000 }
  };

  {
    // Check that reorder doesn't do anything if the top result
    // is already a legal default match (which is the default from
    // PopulateAutocompleteMatches()).
    ACMatches matches;
    PopulateAutocompleteMatches(data, arraysize(data), &matches);
    AutocompleteResult result;
    result.AppendMatches(matches);
    AutocompleteInput input(base::string16(), base::string16::npos,
                            base::string16(), GURL(),
                            OmniboxEventProto::HOME_PAGE, false, false, false,
                            true,
                            ChromeAutocompleteSchemeClassifier(
                                test_util_.profile()));
    result.SortAndCull(input, test_util_.model());
    AssertResultMatches(result, data, 4);
  }

  {
    // Check that reorder swaps up a result appropriately.
    ACMatches matches;
    PopulateAutocompleteMatches(data, arraysize(data), &matches);
    matches[0].allowed_to_be_default_match = false;
    matches[1].allowed_to_be_default_match = false;
    AutocompleteResult result;
    result.AppendMatches(matches);
    AutocompleteInput input(base::string16(), base::string16::npos,
                            base::string16(), GURL(),
                            OmniboxEventProto::HOME_PAGE, false, false, false,
                            true,
                            ChromeAutocompleteSchemeClassifier(
                                test_util_.profile()));
    result.SortAndCull(input, test_util_.model());
    ASSERT_EQ(4U, result.size());
    EXPECT_EQ("http://c/", result.match_at(0)->destination_url.spec());
    EXPECT_EQ("http://a/", result.match_at(1)->destination_url.spec());
    EXPECT_EQ("http://b/", result.match_at(2)->destination_url.spec());
    EXPECT_EQ("http://d/", result.match_at(3)->destination_url.spec());
  }
}



TEST_F(AutocompleteResultTest, SortAndCullWithDisableInlining) {
  TestData data[] = {
    { 0, 0, 1300 },
    { 1, 0, 1200 },
    { 2, 0, 1100 },
    { 3, 0, 1000 }
  };

  {
    // Check that with the field trial disabled, we keep keep the first match
    // first even if it has an inline autocompletion.
    ACMatches matches;
    PopulateAutocompleteMatches(data, arraysize(data), &matches);
    matches[0].inline_autocompletion = base::ASCIIToUTF16("completion");
    AutocompleteResult result;
    result.AppendMatches(matches);
    AutocompleteInput input(base::string16(), base::string16::npos,
                            base::string16(), GURL(),
                            OmniboxEventProto::HOME_PAGE, false, false, false,
                            true,
                            ChromeAutocompleteSchemeClassifier(
                                test_util_.profile()));
    result.SortAndCull(input, test_util_.model());
    AssertResultMatches(result, data, 4);
  }

  // Enable the field trial to disable inlining.
  {
    std::map<std::string, std::string> params;
    params[OmniboxFieldTrial::kDisableInliningRule] = "true";
    ASSERT_TRUE(variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "D", params));
  }
  base::FieldTrialList::CreateFieldTrial(
      OmniboxFieldTrial::kBundledExperimentFieldTrialName, "D");

  {
    // Now the first match should be demoted past the second.
    ACMatches matches;
    PopulateAutocompleteMatches(data, arraysize(data), &matches);
    matches[0].inline_autocompletion = base::ASCIIToUTF16("completion");
    AutocompleteResult result;
    result.AppendMatches(matches);
    AutocompleteInput input(base::string16(), base::string16::npos,
                            base::string16(), GURL(),
                            OmniboxEventProto::HOME_PAGE, false, false, false,
                            true,
                            ChromeAutocompleteSchemeClassifier(
                                test_util_.profile()));
    result.SortAndCull(input, test_util_.model());
    ASSERT_EQ(4U, result.size());
    EXPECT_EQ("http://b/", result.match_at(0)->destination_url.spec());
    EXPECT_EQ("http://a/", result.match_at(1)->destination_url.spec());
    EXPECT_EQ("http://c/", result.match_at(2)->destination_url.spec());
    EXPECT_EQ("http://d/", result.match_at(3)->destination_url.spec());
  }

  {
    // But if there was no inline autocompletion on the first match, then
    // the order should stay the same.  This is true even if there are
    // inline autocompletions elsewhere.
    ACMatches matches;
    PopulateAutocompleteMatches(data, arraysize(data), &matches);
    matches[2].inline_autocompletion = base::ASCIIToUTF16("completion");
    AutocompleteResult result;
    result.AppendMatches(matches);
    AutocompleteInput input(base::string16(), base::string16::npos,
                            base::string16(), GURL(),
                            OmniboxEventProto::HOME_PAGE, false, false, false,
                            true,
                            ChromeAutocompleteSchemeClassifier(
                                test_util_.profile()));
    result.SortAndCull(input, test_util_.model());
    AssertResultMatches(result, data, 4);
  }

  {
    // Try a more complicated situation.
    ACMatches matches;
    PopulateAutocompleteMatches(data, arraysize(data), &matches);
    matches[0].allowed_to_be_default_match = false;
    matches[1].inline_autocompletion = base::ASCIIToUTF16("completion");
    AutocompleteResult result;
    result.AppendMatches(matches);
    AutocompleteInput input(base::string16(), base::string16::npos,
                            base::string16(), GURL(),
                            OmniboxEventProto::HOME_PAGE, false, false, false,
                            true,
                            ChromeAutocompleteSchemeClassifier(
                                test_util_.profile()));
    result.SortAndCull(input, test_util_.model());
    ASSERT_EQ(4U, result.size());
    EXPECT_EQ("http://c/", result.match_at(0)->destination_url.spec());
    EXPECT_EQ("http://a/", result.match_at(1)->destination_url.spec());
    EXPECT_EQ("http://b/", result.match_at(2)->destination_url.spec());
    EXPECT_EQ("http://d/", result.match_at(3)->destination_url.spec());
  }

  {
    // Try another complicated situation.
    ACMatches matches;
    PopulateAutocompleteMatches(data, arraysize(data), &matches);
    matches[0].inline_autocompletion = base::ASCIIToUTF16("completion");
    matches[1].allowed_to_be_default_match = false;
    AutocompleteResult result;
    result.AppendMatches(matches);
    AutocompleteInput input(base::string16(), base::string16::npos,
                            base::string16(), GURL(),
                            OmniboxEventProto::HOME_PAGE, false, false, false,
                            true,
                            ChromeAutocompleteSchemeClassifier(
                                test_util_.profile()));
    result.SortAndCull(input, test_util_.model());
    ASSERT_EQ(4U, result.size());
    EXPECT_EQ("http://c/", result.match_at(0)->destination_url.spec());
    EXPECT_EQ("http://a/", result.match_at(1)->destination_url.spec());
    EXPECT_EQ("http://b/", result.match_at(2)->destination_url.spec());
    EXPECT_EQ("http://d/", result.match_at(3)->destination_url.spec());
  }

  {
    // Check that disaster doesn't strike if we can't demote the top inline
    // autocompletion because every match either has a completion or isn't
    // allowed to be the default match.  In this case, we should leave
    // everything untouched.
    ACMatches matches;
    PopulateAutocompleteMatches(data, arraysize(data), &matches);
    matches[0].inline_autocompletion = base::ASCIIToUTF16("completion");
    matches[1].allowed_to_be_default_match = false;
    matches[2].allowed_to_be_default_match = false;
    matches[3].inline_autocompletion = base::ASCIIToUTF16("completion");
    AutocompleteResult result;
    result.AppendMatches(matches);
    AutocompleteInput input(base::string16(), base::string16::npos,
                            base::string16(), GURL(),
                            OmniboxEventProto::HOME_PAGE, false, false, false,
                            true,
                            ChromeAutocompleteSchemeClassifier(
                                test_util_.profile()));
    result.SortAndCull(input, test_util_.model());
    AssertResultMatches(result, data, 4);
  }

  {
    // Check a similar situation, except in this case the top match is not
    // allowed to the default match, so it still needs to be demoted so we
    // get a legal default match first.  That match will have an inline
    // autocompletion because we don't have any better options.
    ACMatches matches;
    PopulateAutocompleteMatches(data, arraysize(data), &matches);
    matches[0].allowed_to_be_default_match = false;
    matches[1].inline_autocompletion = base::ASCIIToUTF16("completion");
    matches[2].allowed_to_be_default_match = false;
    matches[3].inline_autocompletion = base::ASCIIToUTF16("completion");
    AutocompleteResult result;
    result.AppendMatches(matches);
    AutocompleteInput input(base::string16(), base::string16::npos,
                            base::string16(), GURL(),
                            OmniboxEventProto::HOME_PAGE, false, false, false,
                            true,
                            ChromeAutocompleteSchemeClassifier(
                                test_util_.profile()));
    result.SortAndCull(input, test_util_.model());
    ASSERT_EQ(4U, result.size());
    EXPECT_EQ("http://b/", result.match_at(0)->destination_url.spec());
    EXPECT_EQ("http://a/", result.match_at(1)->destination_url.spec());
    EXPECT_EQ("http://c/", result.match_at(2)->destination_url.spec());
    EXPECT_EQ("http://d/", result.match_at(3)->destination_url.spec());
  }
}

TEST_F(AutocompleteResultTest, ShouldHideTopMatch) {
  base::FieldTrialList::CreateFieldTrial("InstantExtended",
                                         "Group1 hide_verbatim:1");
  ACMatches matches;

  // Case 1: Top match is a verbatim match.
  PopulateAutocompleteMatchesFromTestData(kVerbatimMatches, 1, &matches);
  AutocompleteResult result;
  result.AppendMatches(matches);
  EXPECT_TRUE(result.ShouldHideTopMatch());
  matches.clear();
  result.Reset();

  // Case 2: If the verbatim first match is followed by another verbatim match,
  // don't hide the top verbatim match.
  PopulateAutocompleteMatchesFromTestData(kVerbatimMatches,
                                          arraysize(kVerbatimMatches),
                                          &matches);
  result.AppendMatches(matches);
  EXPECT_FALSE(result.ShouldHideTopMatch());
  matches.clear();
  result.Reset();

  // Case 3: Top match is not a verbatim match. Do not hide the top match.
  PopulateAutocompleteMatchesFromTestData(kNonVerbatimMatches, 1, &matches);
  PopulateAutocompleteMatchesFromTestData(kVerbatimMatches,
                                          arraysize(kVerbatimMatches),
                                          &matches);
  result.AppendMatches(matches);
  EXPECT_FALSE(result.ShouldHideTopMatch());
}

TEST_F(AutocompleteResultTest, ShouldHideTopMatchAfterCopy) {
  base::FieldTrialList::CreateFieldTrial("InstantExtended",
                                         "Group1 hide_verbatim:1");
  ACMatches matches;

  // Case 1: Top match is a verbatim match followed by only copied matches.
  PopulateAutocompleteMatchesFromTestData(kVerbatimMatches,
                                          arraysize(kVerbatimMatches),
                                          &matches);
  for (size_t i = 1; i < arraysize(kVerbatimMatches); ++i)
    matches[i].from_previous = true;
  AutocompleteResult result;
  result.AppendMatches(matches);
  EXPECT_TRUE(result.ShouldHideTopMatch());
  result.Reset();

  // Case 2: The copied matches are then followed by a non-verbatim match.
  PopulateAutocompleteMatchesFromTestData(kNonVerbatimMatches, 1, &matches);
  result.AppendMatches(matches);
  EXPECT_TRUE(result.ShouldHideTopMatch());
  result.Reset();

  // Case 3: The copied matches are instead followed by a verbatim match.
  matches.back().from_previous = true;
  PopulateAutocompleteMatchesFromTestData(kVerbatimMatches, 1, &matches);
  result.AppendMatches(matches);
  EXPECT_FALSE(result.ShouldHideTopMatch());
}

TEST_F(AutocompleteResultTest, DoNotHideTopMatch_FieldTrialFlagDisabled) {
  // This test config is identical to ShouldHideTopMatch test ("Case 1") except
  // that the "hide_verbatim" flag is disabled in the field trials.
  base::FieldTrialList::CreateFieldTrial("InstantExtended",
                                         "Group1 hide_verbatim:0");
  ACMatches matches;
  PopulateAutocompleteMatchesFromTestData(kVerbatimMatches, 1, &matches);
  AutocompleteResult result;
  result.AppendMatches(matches);
  // Field trial flag "hide_verbatim" is disabled. Do not hide top match.
  EXPECT_FALSE(result.ShouldHideTopMatch());
}

TEST_F(AutocompleteResultTest, TopMatchIsStandaloneVerbatimMatch) {
  ACMatches matches;
  AutocompleteResult result;
  result.AppendMatches(matches);

  // Case 1: Result set is empty.
  EXPECT_FALSE(result.TopMatchIsStandaloneVerbatimMatch());

  // Case 2: Top match is not a verbatim match.
  PopulateAutocompleteMatchesFromTestData(kNonVerbatimMatches, 1, &matches);
  result.AppendMatches(matches);
  EXPECT_FALSE(result.TopMatchIsStandaloneVerbatimMatch());
  result.Reset();
  matches.clear();

  // Case 3: Top match is a verbatim match.
  PopulateAutocompleteMatchesFromTestData(kVerbatimMatches, 1, &matches);
  result.AppendMatches(matches);
  EXPECT_TRUE(result.TopMatchIsStandaloneVerbatimMatch());
  result.Reset();
  matches.clear();

  // Case 4: Standalone verbatim match found in AutocompleteResult.
  PopulateAutocompleteMatchesFromTestData(kVerbatimMatches, 1, &matches);
  PopulateAutocompleteMatchesFromTestData(kNonVerbatimMatches, 1, &matches);
  result.AppendMatches(matches);
  EXPECT_TRUE(result.TopMatchIsStandaloneVerbatimMatch());
  result.Reset();
  matches.clear();

  // Case 5: Multiple verbatim matches found in AutocompleteResult.
  PopulateAutocompleteMatchesFromTestData(kVerbatimMatches,
                                          arraysize(kVerbatimMatches),
                                          &matches);
  result.AppendMatches(matches);
  EXPECT_FALSE(result.ShouldHideTopMatch());
}
