// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/third_party/paged_array.h"

#include "testing/gtest/include/gtest/gtest.h"

class PagedArrayTest : public testing::Test {
 public:
  // Total allocation of 4GB will fail in 32 bit programs if allocations are
  // leaked.
  static const int kIterations = 20;
  static const int kSize = 200 * 1024 * 1024 / sizeof(int);  // 200MB
};

TEST_F(PagedArrayTest, TestManyAllocationsDestructorFree) {
  for (int i = 0; i < kIterations; ++i) {
    courgette::PagedArray<int> a;
    EXPECT_TRUE(a.Allocate(kSize));
  }
}

TEST_F(PagedArrayTest, TestManyAllocationsManualFree) {
  courgette::PagedArray<int> a;
  for (int i = 0; i < kIterations; ++i) {
    EXPECT_TRUE(a.Allocate(kSize));
    a.clear();
  }
}

TEST_F(PagedArrayTest, TestAccess) {
  const int kAccessSize = 3 * 1024 * 1024;
  courgette::PagedArray<int> a;
  a.Allocate(kAccessSize);
  for (int i = 0; i < kAccessSize; ++i) {
    a[i] = i;
  }
  for (int i = 0; i < kAccessSize; ++i) {
    EXPECT_EQ(i, a[i]);
  }
}
