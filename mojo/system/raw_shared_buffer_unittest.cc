// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/raw_shared_buffer.h"

#include "base/macros.h"
#include "base/memory/ref_counted.h"
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
  scoped_refptr<RawSharedBuffer> buffer(RawSharedBuffer::Create(kNumBytes));
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
  scoped_refptr<RawSharedBuffer> buffer(RawSharedBuffer::Create(100));
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

// Tests that separate mappings get distinct addresses.
// Note: It's not inconceivable that the OS could ref-count identical mappings
// and reuse the same address, in which case we'd have to be more careful about
// using the address as the key for unmapping.
TEST(RawSharedBufferTest, MappingsDistinct) {
  scoped_refptr<RawSharedBuffer> buffer(RawSharedBuffer::Create(100));
  scoped_ptr<RawSharedBuffer::Mapping> mapping1(buffer->Map(0, 100));
  scoped_ptr<RawSharedBuffer::Mapping> mapping2(buffer->Map(0, 100));
  EXPECT_NE(mapping1->base(), mapping2->base());
}

TEST(RawSharedBufferTest, BufferZeroInitialized) {
  static const size_t kSizes[] = { 10, 100, 1000, 10000, 100000 };
  for (size_t i = 0; i < arraysize(kSizes); i++) {
    scoped_refptr<RawSharedBuffer> buffer(RawSharedBuffer::Create(kSizes[i]));
    scoped_ptr<RawSharedBuffer::Mapping> mapping(buffer->Map(0, kSizes[i]));
    for (size_t j = 0; j < kSizes[i]; j++) {
      // "Assert" instead of "expect" so we don't spam the output with thousands
      // of failures if we fail.
      ASSERT_EQ('\0', static_cast<char*>(mapping->base())[j])
          << "size " << kSizes[i] << ", offset " << j;
    }
  }
}

TEST(RawSharedBufferTest, MappingsOutliveBuffer) {
  scoped_ptr<RawSharedBuffer::Mapping> mapping1;
  scoped_ptr<RawSharedBuffer::Mapping> mapping2;

  {
    scoped_refptr<RawSharedBuffer> buffer(RawSharedBuffer::Create(100));
    mapping1 = buffer->Map(0, 100).Pass();
    mapping2 = buffer->Map(50, 50).Pass();
    static_cast<char*>(mapping1->base())[50] = 'x';
  }

  EXPECT_EQ('x', static_cast<char*>(mapping2->base())[0]);

  static_cast<char*>(mapping2->base())[1] = 'y';
  EXPECT_EQ('y', static_cast<char*>(mapping1->base())[51]);
}

}  // namespace
}  // namespace system
}  // namespace mojo
