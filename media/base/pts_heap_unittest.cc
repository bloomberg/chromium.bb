// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time.h"
#include "media/base/pts_heap.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(PtsHeapTest, IsEmpty) {
  const base::TimeDelta kTestPts1 = base::TimeDelta::FromMicroseconds(123);

  PtsHeap heap;
  ASSERT_TRUE(heap.IsEmpty());
  heap.Push(kTestPts1);
  ASSERT_FALSE(heap.IsEmpty());
  heap.Pop();
  ASSERT_TRUE(heap.IsEmpty());
}

TEST(PtsHeapTest, Ordering) {
  const base::TimeDelta kTestPts1 = base::TimeDelta::FromMicroseconds(123);
  const base::TimeDelta kTestPts2 = base::TimeDelta::FromMicroseconds(456);

  PtsHeap heap;
  heap.Push(kTestPts1);
  heap.Push(kTestPts2);
  heap.Push(kTestPts1);

  EXPECT_TRUE(kTestPts1 == heap.Top());
  heap.Pop();
  EXPECT_TRUE(kTestPts1 == heap.Top());
  heap.Pop();
  EXPECT_TRUE(kTestPts2 == heap.Top());
  heap.Pop();
}

}  // namespace media
