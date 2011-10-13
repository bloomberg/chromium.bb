// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

class InMemoryURLIndexTypesTest : public testing::Test {
};

TEST_F(InMemoryURLIndexTypesTest, StaticFunctions) {
  // Test WordVectorFromString16
  string16 string_a(ASCIIToUTF16("http://www.google.com/ frammy the brammy"));
  String16Vector string_vec = String16VectorFromString16(string_a, false);
  ASSERT_EQ(7U, string_vec.size());
  // See if we got the words we expected.
  EXPECT_EQ(UTF8ToUTF16("http"), string_vec[0]);
  EXPECT_EQ(UTF8ToUTF16("www"), string_vec[1]);
  EXPECT_EQ(UTF8ToUTF16("google"), string_vec[2]);
  EXPECT_EQ(UTF8ToUTF16("com"), string_vec[3]);
  EXPECT_EQ(UTF8ToUTF16("frammy"), string_vec[4]);
  EXPECT_EQ(UTF8ToUTF16("the"), string_vec[5]);
  EXPECT_EQ(UTF8ToUTF16("brammy"), string_vec[6]);

  string_vec = String16VectorFromString16(string_a, true);
  ASSERT_EQ(5U, string_vec.size());
  EXPECT_EQ(UTF8ToUTF16("http://"), string_vec[0]);
  EXPECT_EQ(UTF8ToUTF16("www.google.com/"), string_vec[1]);
  EXPECT_EQ(UTF8ToUTF16("frammy"), string_vec[2]);
  EXPECT_EQ(UTF8ToUTF16("the"), string_vec[3]);
  EXPECT_EQ(UTF8ToUTF16("brammy"), string_vec[4]);

  // Test WordSetFromString16
  string16 string_b(ASCIIToUTF16(
      "http://web.google.com/search Google Web Search"));
  String16Set string_set = String16SetFromString16(string_b);
  EXPECT_EQ(5U, string_set.size());
  // See if we got the words we expected.
  EXPECT_TRUE(string_set.find(UTF8ToUTF16("com")) != string_set.end());
  EXPECT_TRUE(string_set.find(UTF8ToUTF16("google")) != string_set.end());
  EXPECT_TRUE(string_set.find(UTF8ToUTF16("http")) != string_set.end());
  EXPECT_TRUE(string_set.find(UTF8ToUTF16("search")) != string_set.end());
  EXPECT_TRUE(string_set.find(UTF8ToUTF16("web")) != string_set.end());

  // Test SortAndDeoverlap
  TermMatches matches_a;
  matches_a.push_back(TermMatch(1, 13, 10));
  matches_a.push_back(TermMatch(2, 23, 10));
  matches_a.push_back(TermMatch(3, 3, 10));
  matches_a.push_back(TermMatch(4, 40, 5));
  TermMatches matches_b = SortAndDeoverlapMatches(matches_a);
  // Nothing should have been eliminated.
  EXPECT_EQ(matches_a.size(), matches_b.size());
  // The order should now be 3, 1, 2, 4.
  EXPECT_EQ(3, matches_b[0].term_num);
  EXPECT_EQ(1, matches_b[1].term_num);
  EXPECT_EQ(2, matches_b[2].term_num);
  EXPECT_EQ(4, matches_b[3].term_num);
  matches_a.push_back(TermMatch(5, 18, 10));
  matches_a.push_back(TermMatch(6, 38, 5));
  matches_b = SortAndDeoverlapMatches(matches_a);
  // Two matches should have been eliminated.
  EXPECT_EQ(matches_a.size() - 2, matches_b.size());
  // The order should now be 3, 1, 2, 6.
  EXPECT_EQ(3, matches_b[0].term_num);
  EXPECT_EQ(1, matches_b[1].term_num);
  EXPECT_EQ(2, matches_b[2].term_num);
  EXPECT_EQ(6, matches_b[3].term_num);

  // Test MatchTermInString
  TermMatches matches_c = MatchTermInString(
      UTF8ToUTF16("x"), UTF8ToUTF16("axbxcxdxex fxgx/hxixjx.kx"), 123);
  const size_t expected_offsets[] = { 1, 3, 5, 7, 9, 12, 14, 17, 19, 21, 24 };
  ASSERT_EQ(arraysize(expected_offsets), matches_c.size());
  for (size_t i = 0; i < arraysize(expected_offsets); ++i)
    EXPECT_EQ(expected_offsets[i], matches_c[i].offset);
}

TEST_F(InMemoryURLIndexTypesTest, OffsetsAndTermMatches) {
  // Test OffsetsFromTermMatches
  history::TermMatches matches_a;
  matches_a.push_back(history::TermMatch(1, 1, 2));
  matches_a.push_back(history::TermMatch(2, 4, 3));
  matches_a.push_back(history::TermMatch(3, 9, 1));
  matches_a.push_back(history::TermMatch(3, 10, 1));
  matches_a.push_back(history::TermMatch(4, 14, 5));
  std::vector<size_t> offsets = OffsetsFromTermMatches(matches_a);
  const size_t expected_offsets_a[] = {1, 4, 9, 10, 14};
  ASSERT_EQ(offsets.size(), arraysize(expected_offsets_a));
  for (size_t i = 0; i < offsets.size(); ++i)
    EXPECT_EQ(expected_offsets_a[i], offsets[i]);

  // Test ReplaceOffsetsInTermMatches
  offsets[2] = string16::npos;
  history::TermMatches matches_b =
      ReplaceOffsetsInTermMatches(matches_a, offsets);
  const size_t expected_offsets_b[] = {1, 4, 10, 14};
  ASSERT_EQ(arraysize(expected_offsets_b), matches_b.size());
  for (size_t i = 0; i < matches_b.size(); ++i)
    EXPECT_EQ(expected_offsets_b[i], matches_b[i].offset);
}

}  // namespace history
