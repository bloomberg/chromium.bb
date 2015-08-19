// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/third_party/qsufsort.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(QSufSortTest, Sort) {
  const char* test_cases[] = {
    "",
    "a",
    "za",
    "CACAO",
    "banana",
    "tobeornottobe",
    "The quick brown fox jumps over the lazy dog.",
    "elephantelephantelephantelephantelephant",
    "-------------------------",
    "011010011001011010010110011010010",
    "3141592653589793238462643383279502884197169399375105",
    "\xFF\xFE\xFF\xFE\xFD\x80\x30\x31\x32\x80\x30\xFF\x01\xAB\xCD",
  };

  for (size_t idx = 0; idx < arraysize(test_cases); ++idx) {
    int len = static_cast<int>(::strlen(test_cases[idx]));
    const unsigned char* s =
        reinterpret_cast<const unsigned char*>(test_cases[idx]);

    // Generate the suffix array as I.
    std::vector<int> I(len + 1);
    std::vector<int> V(len + 1);
    courgette::qsuf::qsufsort<int*>(&I[0], &V[0], s, len);

    // Expect that I[] is a permutation of [0, len].
    std::vector<int> I_sorted(I);
    std::sort(I_sorted.begin(), I_sorted.end());
    for (int i = 0; i < len + 1; ++i) {
      EXPECT_EQ(i, I_sorted[i]) << "test_case[" << idx << "]";
    }

    // First string must be empty string.
    EXPECT_EQ(len, I[0]) << "test_case[" << idx << "]";

    // Expect that the |len + 1| suffixes are strictly ordered.
    const unsigned char* end = s + len;
    for (int i = 0; i < len; ++i) {
      const unsigned char* suf1 = s + I[i];
      const unsigned char* suf2 = s + I[i + 1];
      bool is_less = std::lexicographical_compare(suf1, end, suf2, end);
      EXPECT_TRUE(is_less) << "test_case[" << idx << "]";
    }
  }
}

TEST(QSufSortTest, Search) {
  // Initialize main string and the suffix array.
  // Positions:          00000000001111111111122222222233333333334444
  //                     01234567890123456789012345678901234567890123
  const char* old_str = "the quick brown fox jumps over the lazy dog.";
  int old_size = static_cast<int>(::strlen(old_str));
  const unsigned char* old_buf =
        reinterpret_cast<const unsigned char*>(old_str);
  std::vector<int> I(old_size + 1);
  std::vector<int> V(old_size + 1);
  courgette::qsuf::qsufsort<int*>(&I[0], &V[0], old_buf, old_size);

  // Test queries.
  const struct {
    int exp_pos;  // -1 means "don't care".
    int exp_match_len;
    const char* query_str;
  } test_cases[] = {
    // Entire string.
    {0, 44, "the quick brown fox jumps over the lazy dog."},
    // Empty string.
    {-1, 0, ""},  // Current algorithm does not enforce |pos| == 0.
    // Exact and unique suffix match.
    {43, 1, "."},
    {31, 13, "the lazy dog."},
    // Exact and unique non-suffix match.
    {4, 5, "quick"},
    {0, 9, "the quick"},  // Unique prefix.
    // Entire word match with mutiple results: take lexicographical first.
    {31, 3, "the"},  // Non-unique prefix: "the l"... < "the q"...
    {9, 1, " "},  // " brown"... wins.
    // Partial and unique match of query prefix.
    {16, 10, "fox jumps with the hosps"},
    // Partial and multiple match of query prefix: no guarantees on |pos|.
    // Take lexicographical first for matching portion *only*, so same results:
    {-1, 4, "the apple"},  // query      < "the l"... < "the q"...
    {-1, 4, "the opera"},  // "the l"... < query      < "the q"...
    {-1, 4, "the zebra"},  // "the l"... < "the q"... < query
    // Prefix match dominates suffix match.
    {26, 5, "over quick brown fox"},
    // No match.
    {-1, 0, ","},
    {-1, 0, "1234"},
    {-1, 0, "THE QUICK BROWN FOX"},
    {-1, 0, "(the"},
  };

  for (size_t idx = 0; idx < arraysize(test_cases); ++idx) {
    const auto& test_case = test_cases[idx];
    int new_size = static_cast<int>(::strlen(test_case.query_str));
    const unsigned char* new_buf =
        reinterpret_cast<const unsigned char*>(test_case.query_str);

    // Perform the search.
    int pos = 0;
    int match_len = courgette::qsuf::search(
        &I[0], old_buf, old_size, new_buf, new_size, &pos);

    // Check basic properties and match with expected values.
    EXPECT_GE(match_len, 0) << "test_case[" << idx << "]";
    EXPECT_LE(match_len, new_size) << "test_case[" << idx << "]";
    if (match_len > 0) {
      EXPECT_GE(pos, 0) << "test_case[" << idx << "]";
      EXPECT_LE(pos, old_size - match_len) << "test_case[" << idx << "]";
      EXPECT_EQ(0, ::memcmp(old_buf + pos, new_buf, match_len))
          << "test_case[" << idx << "]";
    }
    if (test_case.exp_pos >= 0) {
      EXPECT_EQ(test_case.exp_pos, pos) << "test_case[" << idx << "]";
    }
    EXPECT_EQ(test_case.exp_match_len, match_len) << "test_case[" << idx << "]";
  }
}
