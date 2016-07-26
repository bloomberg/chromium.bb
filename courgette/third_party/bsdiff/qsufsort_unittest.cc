// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/third_party/bsdiff/qsufsort.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool IsPermutation(const std::vector<int> v) {
  std::vector<int> v_sorted(v);
  std::sort(v_sorted.begin(), v_sorted.end());
  for (int i = 0; i < static_cast<int>(v.size()); ++i)
    if (i != v_sorted[i])
      return false;
  return true;
}

}  // namespace

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
    int size = static_cast<int>(::strlen(test_cases[idx]));
    const unsigned char* buf =
        reinterpret_cast<const unsigned char*>(test_cases[idx]);

    // Generate the suffix array as I.
    std::vector<int> I(size + 1);
    std::vector<int> V(size + 1);
    qsuf::qsufsort<int*>(&I[0], &V[0], buf, size);

    // Expect I[] and V[] to be a permutation of [0, size].
    EXPECT_TRUE(IsPermutation(I));
    EXPECT_TRUE(IsPermutation(V));

    // Expect V[] to be inverse of I[].
    for (int i = 0; i < size + 1; ++i)
      EXPECT_EQ(i, V[I[i]]);

    // First string must be empty string.
    EXPECT_EQ(size, I[0]);

    // Expect that the |size + 1| suffixes are strictly ordered.
    const unsigned char* end = buf + size;
    for (int i = 0; i < size; ++i) {
      const unsigned char* suf1 = buf + I[i];
      const unsigned char* suf2 = buf + I[i + 1];
      bool is_less = std::lexicographical_compare(suf1, end, suf2, end);
      EXPECT_TRUE(is_less);
    }
  }
}
