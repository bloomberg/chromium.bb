// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "testing/gtest/include/gtest/gtest.h"

class AutocompleteResultTest : public testing::Test  {
 public:
  struct TestData {
    // Used to build a url for the AutocompleteMatch. The URL becomes
    // 'http://' + |url_id|.
    int url_id;

    // ID of the provider.
    int provider_id;

    // Relevance score.
    int relevance;
  };

  AutocompleteResultTest() {}

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

 private:
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
  AutocompleteInput input(ASCIIToUTF16("a"), string16(), false, false, false,
                          false);

  ACMatches last_matches;
  PopulateAutocompleteMatches(last, last_size, &last_matches);
  AutocompleteResult last_result;
  last_result.AppendMatches(last_matches);
  last_result.SortAndCull(input);

  ACMatches current_matches;
  PopulateAutocompleteMatches(current, current_size, &current_matches);
  AutocompleteResult current_result;
  current_result.AppendMatches(current_matches);
  current_result.SortAndCull(input);
  current_result.CopyOldMatches(input, last_result);

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
  AutocompleteInput input(ASCIIToUTF16("a"), string16(), false, false, false,
                          false);
  matches.push_back(match);
  r1.AppendMatches(matches);
  r1.SortAndCull(input);
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
