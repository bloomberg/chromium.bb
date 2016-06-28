// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/third_party/bsdiff/qsufsort.h"

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "base/macros.h"
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
    for (int i = 0; i < len + 1; ++i)
      EXPECT_EQ(i, I_sorted[i]);

    // First string must be empty string.
    EXPECT_EQ(len, I[0]);

    // Expect that the |len + 1| suffixes are strictly ordered.
    const unsigned char* end = s + len;
    for (int i = 0; i < len; ++i) {
      const unsigned char* suf1 = s + I[i];
      const unsigned char* suf2 = s + I[i + 1];
      bool is_less = std::lexicographical_compare(suf1, end, suf2, end);
      EXPECT_TRUE(is_less);
    }
  }
}
