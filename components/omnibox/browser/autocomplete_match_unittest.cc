// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_match.h"

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"

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
        GURL(), metrics::OmniboxEventProto::INVALID_SPEC, false, false, true,
        true, false, TestSchemeClassifier());
    AutocompleteMatch m1(NULL, 100, false,
                         AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    m1.destination_url = GURL(cases[i].url1);
    m1.ComputeStrippedDestinationURL(input, "zh-CN", NULL);
    AutocompleteMatch m2(NULL, 100, false,
                         AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    m2.destination_url = GURL(cases[i].url2);
    m2.ComputeStrippedDestinationURL(input, "zh-CN", NULL);
    EXPECT_EQ(cases[i].expected_duplicate,
              AutocompleteMatch::DestinationsEqual(m1, m2));
  }
}
