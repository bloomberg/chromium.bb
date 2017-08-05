// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

bool EqualClassifications(const ACMatchClassifications& lhs,
                          const ACMatchClassifications& rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (size_t n = 0; n < lhs.size(); ++n)
    if (lhs[n].style != rhs[n].style || lhs[n].offset != rhs[n].offset)
      return false;
  return true;
}

}  // namespace

TEST(AutocompleteMatchTest, MoreRelevant) {
  struct RelevantCases {
    int r1;
    int r2;
    bool expected_result;
  } cases[] = {
    {  10,   0, true  },
    {  10,  -5, true  },
    {  -5,  10, false },
    {   0,  10, false },
    { -10,  -5, false  },
    {  -5, -10, true },
  };

  AutocompleteMatch m1(NULL, 0, false,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  AutocompleteMatch m2(NULL, 0, false,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);

  for (size_t i = 0; i < arraysize(cases); ++i) {
    m1.relevance = cases[i].r1;
    m2.relevance = cases[i].r2;
    EXPECT_EQ(cases[i].expected_result,
              AutocompleteMatch::MoreRelevant(m1, m2));
  }
}

TEST(AutocompleteMatchTest, MergeClassifications) {
  // Merging two empty vectors should result in an empty vector.
  EXPECT_EQ(std::string(),
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ACMatchClassifications(),
              AutocompleteMatch::ACMatchClassifications())));

  // If one vector is empty and the other is "trivial" but non-empty (i.e. (0,
  // NONE)), the non-empty vector should be returned.
  EXPECT_EQ("0,0",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,0"),
              AutocompleteMatch::ACMatchClassifications())));
  EXPECT_EQ("0,0",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ACMatchClassifications(),
              AutocompleteMatch::ClassificationsFromString("0,0"))));

  // Ditto if the one-entry vector is non-trivial.
  EXPECT_EQ("0,1",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,1"),
              AutocompleteMatch::ACMatchClassifications())));
  EXPECT_EQ("0,1",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ACMatchClassifications(),
              AutocompleteMatch::ClassificationsFromString("0,1"))));

  // Merge an unstyled one-entry vector with a styled one-entry vector.
  EXPECT_EQ("0,1",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,0"),
              AutocompleteMatch::ClassificationsFromString("0,1"))));

  // Test simple cases of overlap.
  EXPECT_EQ("0,3," "1,2",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,1," "1,0"),
              AutocompleteMatch::ClassificationsFromString("0,2"))));
  EXPECT_EQ("0,3," "1,2",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,2"),
              AutocompleteMatch::ClassificationsFromString("0,1," "1,0"))));

  // Test the case where both vectors have classifications at the same
  // positions.
  EXPECT_EQ("0,3",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString("0,1," "1,2"),
              AutocompleteMatch::ClassificationsFromString("0,2," "1,1"))));

  // Test an arbitrary complicated case.
  EXPECT_EQ("0,2," "1,0," "2,1," "4,3," "5,7," "6,3," "7,7," "15,1," "17,0",
      AutocompleteMatch::ClassificationsToString(
          AutocompleteMatch::MergeClassifications(
              AutocompleteMatch::ClassificationsFromString(
                  "0,0," "2,1," "4,3," "7,7," "10,6," "15,0"),
              AutocompleteMatch::ClassificationsFromString(
                  "0,2," "1,0," "5,7," "6,1," "17,0"))));
}

TEST(AutocompleteMatchTest, InlineNontailPrefix) {
  struct TestData {
    std::string contents;
    ACMatchClassifications before_contents_class, after_contents_class;
  } cases[] = {
      {"1234567890123456",
       // should just shift the NONE
       {{0, ACMatchClassification::NONE}},
       {{0, ACMatchClassification::DIM}, {8, ACMatchClassification::NONE}}},
      {"1234567890123456",
       // should just shift the NONE
       {{0, ACMatchClassification::NONE}, {10, ACMatchClassification::MATCH}},
       {{0, ACMatchClassification::DIM},
        {8, ACMatchClassification::NONE},
        {10, ACMatchClassification::MATCH}}},
      {"1234567890123456",
       // should nuke NONE
       {{0, ACMatchClassification::NONE}, {8, ACMatchClassification::MATCH}},
       {{0, ACMatchClassification::DIM}, {8, ACMatchClassification::MATCH}}},
      {"1234567890123456",
       // should nuke NONE and shift MATCH
       {{0, ACMatchClassification::NONE}, {4, ACMatchClassification::MATCH}},
       {{0, ACMatchClassification::DIM}, {8, ACMatchClassification::MATCH}}},
      {"1234567890123456",
       // should nuke NONE, shift MATCH and preserve NONE
       {{0, ACMatchClassification::NONE},
        {4, ACMatchClassification::MATCH},
        {12, ACMatchClassification::NONE}},
       {{0, ACMatchClassification::DIM},
        {8, ACMatchClassification::MATCH},
        {12, ACMatchClassification::NONE}}},
      {"12345678",
       // should have only DIM when done
       {{0, ACMatchClassification::NONE}, {4, ACMatchClassification::MATCH}},
       {{0, ACMatchClassification::DIM}}},
      {"1234567890123456",
       // should nuke several classifications and shift MATCH
       {{0, ACMatchClassification::NONE},
        {1, ACMatchClassification::NONE},
        {2, ACMatchClassification::NONE},
        {4, ACMatchClassification::MATCH}},
       {{0, ACMatchClassification::DIM}, {8, ACMatchClassification::MATCH}}},
      {"1234567190123456",
       // should do nothing since prefix doesn't match
       {{0, ACMatchClassification::NONE}, {4, ACMatchClassification::MATCH}},
       {{0, ACMatchClassification::NONE}, {4, ACMatchClassification::MATCH}}},
  };
  for (const auto& test_case : cases) {
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;
    match.contents = base::UTF8ToUTF16(test_case.contents);
    match.contents_class = test_case.before_contents_class;
    match.InlineTailPrefix(base::UTF8ToUTF16("12345678"));
    EXPECT_TRUE(EqualClassifications(match.contents_class,
                                     test_case.after_contents_class));
  }
}

TEST(AutocompleteMatchTest, InlineTailPrefix) {
  struct TestData {
    std::string before_contents, after_contents;
    ACMatchClassifications before_contents_class, after_contents_class;
  } cases[] = {
      {"90123456",
       "1234567890123456",
       // should prepend DIM and offset rest
       {{0, ACMatchClassification::NONE}, {2, ACMatchClassification::MATCH}},
       {{0, ACMatchClassification::DIM},
        {8, ACMatchClassification::NONE},
        {10, ACMatchClassification::MATCH}}},
  };
  for (const auto& test_case : cases) {
    AutocompleteMatch match;
    match.type = AutocompleteMatchType::SEARCH_SUGGEST_TAIL;
    match.contents = base::UTF8ToUTF16(test_case.before_contents);
    match.contents_class = test_case.before_contents_class;
    match.InlineTailPrefix(base::UTF8ToUTF16("12345678"));
    EXPECT_EQ(match.contents, base::UTF8ToUTF16(test_case.after_contents));
    EXPECT_TRUE(EqualClassifications(match.contents_class,
                                     test_case.after_contents_class));
  }
}

TEST(AutocompleteMatchTest, FormatUrlForSuggestionDisplay) {
  // This test does not need to verify url_formatter's functionality in-depth,
  // since url_formatter has its own unit tests. This test is to validate that
  // flipping feature flags and varying the trim_scheme parameter toggles the
  // correct behavior within AutocompleteMatch::GetFormatTypes.
  struct FormatUrlTestData {
    const std::string url;
    bool preserve_scheme;
    bool preserve_subdomain;
    bool preserve_after_host;
    const wchar_t* expected_result;

    void Validate() {
      SCOPED_TRACE(testing::Message()
                   << " url=" << url << " preserve_scheme=" << preserve_scheme
                   << " preserve_subdomain=" << preserve_subdomain
                   << " preserve_after_host=" << preserve_after_host
                   << " expected_result=" << expected_result);
      auto format_types = AutocompleteMatch::GetFormatTypes(
          preserve_scheme, preserve_subdomain, preserve_after_host);
      EXPECT_EQ(base::WideToUTF16(expected_result),
                url_formatter::FormatUrl(GURL(url), format_types,
                                         net::UnescapeRule::SPACES, nullptr,
                                         nullptr, nullptr));
    };
  };

  FormatUrlTestData normal_cases[] = {
      // Test trim_scheme parameter without any feature flags.
      {"http://google.com", false, true, true, L"google.com"},
      {"https://google.com", false, true, true, L"https://google.com"},
      {"http://google.com", true, true, true, L"http://google.com"},
      {"https://google.com", true, true, true, L"https://google.com"},

      // Verify that trivial subdomains are preserved in the normal case.
      {"http://www.google.com", false, false, false, L"www.google.com"},

      // Test that paths are preserved in the default case.
      {"http://google.com/foobar", false, false, false, L"google.com/foobar"},
  };
  for (FormatUrlTestData& test_case : normal_cases)
    test_case.Validate();

  // Test the hide-scheme feature flag with the trim_scheme parameter.
  std::unique_ptr<base::test::ScopedFeatureList> feature_list(
      new base::test::ScopedFeatureList);
  feature_list->InitAndEnableFeature(
      omnibox::kUIExperimentHideSuggestionUrlScheme);

  FormatUrlTestData omit_scheme_cases[] = {
      {"http://google.com", false, false, false, L"google.com"},
      {"https://google.com", false, false, false, L"google.com"},
      {"http://google.com", true, false, false, L"http://google.com"},
      {"https://google.com", true, false, false, L"https://google.com"},
  };
  for (FormatUrlTestData& test_case : omit_scheme_cases)
    test_case.Validate();

  // Test the trim trivial subdomains feature flag.
  feature_list.reset(new base::test::ScopedFeatureList);
  feature_list->InitAndEnableFeature(
      omnibox::kUIExperimentHideSuggestionUrlTrivialSubdomains);

  FormatUrlTestData trim_trivial_subdomains_cases[] = {
      {"http://www.m.google.com", false, false, false, L"google.com"},
      {"http://www.m.google.com", false, true, false, L"www.m.google.com"},
  };

  for (FormatUrlTestData& test_case : trim_trivial_subdomains_cases)
    test_case.Validate();

  // Test the elide-after-host feature flag.
  feature_list.reset(new base::test::ScopedFeatureList);
  feature_list->InitAndEnableFeature(
      omnibox::kUIExperimentElideSuggestionUrlAfterHost);

  FormatUrlTestData hide_path_cases[] = {
      {"http://google.com/foobar", false, false, false,
       L"google.com/\x2026\x0000"},
      {"http://google.com/foobar", false, false, true, L"google.com/foobar"},
  };
  for (FormatUrlTestData& test_case : hide_path_cases)
    test_case.Validate();
}

TEST(AutocompleteMatchTest, SupportsDeletion) {
  // A non-deletable match with no duplicates.
  AutocompleteMatch m(NULL, 0, false,
                      AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  EXPECT_FALSE(m.SupportsDeletion());

  // A deletable match with no duplicates.
  AutocompleteMatch m1(NULL, 0, true,
                       AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  EXPECT_TRUE(m1.SupportsDeletion());

  // A non-deletable match, with non-deletable duplicates.
  m.duplicate_matches.push_back(AutocompleteMatch(
      NULL, 0, false, AutocompleteMatchType::URL_WHAT_YOU_TYPED));
  m.duplicate_matches.push_back(AutocompleteMatch(
      NULL, 0, false, AutocompleteMatchType::URL_WHAT_YOU_TYPED));
  EXPECT_FALSE(m.SupportsDeletion());

  // A non-deletable match, with at least one deletable duplicate.
  m.duplicate_matches.push_back(AutocompleteMatch(
      NULL, 0, true, AutocompleteMatchType::URL_WHAT_YOU_TYPED));
  EXPECT_TRUE(m.SupportsDeletion());
}

TEST(AutocompleteMatchTest, Duplicates) {
  struct DuplicateCases {
    const wchar_t* input;
    const std::string url1;
    const std::string url2;
    const bool expected_duplicate;
  } cases[] = {
    { L"g", "http://www.google.com/",  "https://www.google.com/",    true },
    { L"g", "http://www.google.com/",  "http://www.google.com",      true },
    { L"g", "http://google.com/",      "http://www.google.com/",     true },
    { L"g", "http://www.google.com/",  "HTTP://www.GOOGLE.com/",     true },
    { L"g", "http://www.google.com/",  "http://www.google.com",      true },
    { L"g", "https://www.google.com/", "http://google.com",          true },
    { L"g", "http://www.google.com/",  "wss://www.google.com/",      false },
    { L"g", "http://www.google.com/1", "http://www.google.com/1/",   false },
    { L"g", "http://www.google.com/",  "http://www.google.com/1",    false },
    { L"g", "http://www.google.com/",  "http://www.goo.com/",        false },
    { L"g", "http://www.google.com/",  "http://w2.google.com/",      false },
    { L"g", "http://www.google.com/",  "http://m.google.com/",       false },
    { L"g", "http://www.google.com/",  "http://www.google.com/?foo", false },

    // Don't allow URLs with different schemes to be considered duplicates for
    // certain inputs.
    { L"http://g", "http://google.com/",
                   "https://google.com/",  false },
    { L"http://g", "http://blah.com/",
                   "https://blah.com/",    true  },
    { L"http://g", "http://google.com/1",
                   "https://google.com/1", false },
    { L"http://g hello",    "http://google.com/",
                            "https://google.com/", false },
    { L"hello http://g",    "http://google.com/",
                            "https://google.com/", false },
    { L"hello http://g",    "http://blah.com/",
                            "https://blah.com/",   true  },
    { L"http://b http://g", "http://google.com/",
                            "https://google.com/", false },
    { L"http://b http://g", "http://blah.com/",
                            "https://blah.com/",   false },

    // If the user types unicode that matches the beginning of a
    // punycode-encoded hostname then consider that a match.
    { L"x",               "http://xn--1lq90ic7f1rc.cn/",
                          "https://xn--1lq90ic7f1rc.cn/", true  },
    { L"http://\x5317 x", "http://xn--1lq90ic7f1rc.cn/",
                          "https://xn--1lq90ic7f1rc.cn/", false },
    { L"http://\x89c6 x", "http://xn--1lq90ic7f1rc.cn/",
                          "https://xn--1lq90ic7f1rc.cn/", true  },
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    SCOPED_TRACE("input=" + base::WideToUTF8(cases[i].input) +
                 " url1=" + cases[i].url1 + " url2=" + cases[i].url2);
    AutocompleteInput input(
        base::WideToUTF16(cases[i].input), base::string16::npos, std::string(),
        GURL(), base::string16(), metrics::OmniboxEventProto::INVALID_SPEC,
        false, false, true, true, false, TestSchemeClassifier());
    AutocompleteMatch m1(nullptr, 100, false,
                         AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    m1.destination_url = GURL(cases[i].url1);
    m1.ComputeStrippedDestinationURL(input, nullptr);
    AutocompleteMatch m2(nullptr, 100, false,
                         AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    m2.destination_url = GURL(cases[i].url2);
    m2.ComputeStrippedDestinationURL(input, nullptr);
    EXPECT_EQ(cases[i].expected_duplicate,
              AutocompleteMatch::DestinationsEqual(m1, m2));
  }
}
