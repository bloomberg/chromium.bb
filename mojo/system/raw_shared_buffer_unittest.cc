// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/raw_shared_buffer.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

TEST(RawSharedBufferTest, Basic) {
  const size_t kNumInts = 100;
  const size_t kNumBytes = kNumInts * sizeof(int);
  // A fudge so that we're not just writing zero bytes 75% of the time.
  const int kFudge = 1234567890;

  // Make some memory.
  scoped_ptr<RawSharedBuffer> buffer(RawSharedBuffer::Create(kNumBytes));
  ASSERT_TRUE(buffer);

  // Map it all, scribble some stuff, and then unmap it.
  {
    scoped_ptr<RawSharedBuffer::Mapping> mapping(buffer->Map(0, kNumBytes));
    ASSERT_TRUE(mapping);
    ASSERT_TRUE(mapping->base());
    int* stuff = static_cast<int*>(mapping->base());
    for (size_t i = 0; i < kNumInts; i++)
      stuff[i] = static_cast<int>(i) + kFudge;
  }

  // Map it all again, check that our scribbling is still there, then do a
  // partial mapping and scribble on that, check that everything is coherent,
  // unmap the first mapping, scribble on some of the second mapping, and then
  // unmap it.
  {
    scoped_ptr<RawSharedBuffer::Mapping> mapping1(buffer->Map(0, kNumBytes));
    ASSERT_TRUE(mapping1);
    ASSERT_TRUE(mapping1->base());
    int* stuff1 = static_cast<int*>(mapping1->base());
    for (size_t i = 0; i < kNumInts; i++)
      EXPECT_EQ(static_cast<int>(i) + kFudge, stuff1[i]) << i;

    scoped_ptr<RawSharedBuffer::Mapping> mapping2(
        buffer->Map((kNumInts / 2) * sizeof(int), 2 * sizeof(int)));
    ASSERT_TRUE(mapping2);
    ASSERT_TRUE(mapping2->base());
    int* stuff2 = static_cast<int*>(mapping2->base());
    EXPECT_EQ(static_cast<int>(kNumInts / 2) + kFudge, stuff2[0]);
    EXPECT_EQ(static_cast<int>(kNumInts / 2) + 1 + kFudge, stuff2[1]);

    stuff2[0] = 123;
    stuff2[1] = 456;
    EXPECT_EQ(123, stuff1[kNumInts / 2]);
    EXPECT_EQ(456, stuff1[kNumInts / 2 + 1]);

    mapping1.reset();

    EXPECT_EQ(123, stuff2[0]);
    EXPECT_EQ(456, stuff2[1]);
    stuff2[1] = 789;
  }

  // Do another partial mapping and check that everything is the way we expect
  // it to be.
  {
    scoped_ptr<RawSharedBuffer::Mapping> mapping(
        buffer->Map(sizeof(int), kNumBytes - sizeof(int)));
    ASSERT_TRUE(mapping);
    ASSERT_TRUE(mapping->base());
    int* stuff = static_cast<int*>(mapping->base());

    for (size_t j = 0; j < kNumInts - 1; j++) {
      int i = static_cast<int>(j) + 1;
      if (i == kNumInts / 2) {
        EXPECT_EQ(123, stuff[j]);
      } else if (i == kNumInts / 2 + 1) {
        EXPECT_EQ(789, stuff[j]);
      } else {
        EXPECT_EQ(i + kFudge, stuff[j]) << i;
      }
    }
  }
}

// TODO(vtl): Bigger buffers.

TEST(RawSharedBufferTest, InvalidArguments) {
  // Zero length not allowed.
  EXPECT_FALSE(RawSharedBuffer::Create(0));

  // Invalid mappings:
  scoped_ptr<RawSharedBuffer> buffer(RawSharedBuffer::Create(100));
  ASSERT_TRUE(buffer);

  // Zero length not allowed.
  EXPECT_FALSE(buffer->Map(0, 0));

  // Okay:
  EXPECT_TRUE(buffer->Map(0, 100));
  // Offset + length too big.
  EXPECT_FALSE(buffer->Map(0, 101));
  EXPECT_FALSE(buffer->Map(1, 100));

  // Okay:
  EXPECT_TRUE(buffer->Map(50, 50));
  // Offset + length too big.
  EXPECT_FALSE(buffer->Map(50, 51));
  EXPECT_FALSE(buffer->Map(51, 50));
}

// TODO(vtl): Check that mappings can outlive the shared buffer.

}  // namespace
}  // namespace system
}  // namespace mojo
