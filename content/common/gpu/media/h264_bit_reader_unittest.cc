// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/h264_bit_reader.h"

#include "testing/gtest/include/gtest/gtest.h"

using content::H264BitReader;

TEST(BitReaderTest, H264StreamTest) {
  // This stream contains an escape sequence. Its last byte only has 4 bits.
  // 0001 0010 0011 0100 0000 0000 0000 0000 0000 0011 0101 0110 0111 0000
  uint8 buffer[] = {0x12, 0x34, 0x00, 0x00, 0x03, 0x56, 0x70};
  H264BitReader reader;
  uint8 value8;
  uint32 value32;

  reader.Initialize(buffer, sizeof(buffer));
  EXPECT_EQ(reader.Tell(), 0);
  EXPECT_TRUE(reader.ReadBits(4, &value8));
  EXPECT_EQ(value8, 1u);
  EXPECT_EQ(reader.Tell(), 4);
  EXPECT_TRUE(reader.HasMoreData());

  EXPECT_TRUE(reader.ReadBits(8, &value8));
  EXPECT_EQ(value8, 0x23u);
  EXPECT_EQ(reader.Tell(), 12);
  EXPECT_TRUE(reader.HasMoreData());

  EXPECT_TRUE(reader.ReadBits(24, &value32));
  EXPECT_EQ(value32, 0x400005u);
  EXPECT_EQ(reader.Tell(), 44);  // Include the skipped escape byte
  EXPECT_TRUE(reader.HasMoreData());

  EXPECT_TRUE(reader.ReadBits(8, &value8));
  EXPECT_EQ(value8, 0x67u);
  EXPECT_EQ(reader.Tell(), 52);  // Include the skipped escape byte
  EXPECT_FALSE(reader.HasMoreData());

  EXPECT_TRUE(reader.ReadBits(0, &value8));
  EXPECT_EQ(reader.Tell(), 52);  // Include the skipped escape byte
  EXPECT_FALSE(reader.ReadBits(1, &value8));
  EXPECT_FALSE(reader.HasMoreData());

  // Do it again using SkipBits
  reader.Initialize(buffer, sizeof(buffer));
  EXPECT_EQ(reader.Tell(), 0);
  EXPECT_TRUE(reader.SkipBits(4));
  EXPECT_EQ(reader.Tell(), 4);
  EXPECT_TRUE(reader.HasMoreData());

  EXPECT_TRUE(reader.SkipBits(8));
  EXPECT_EQ(reader.Tell(), 12);
  EXPECT_TRUE(reader.HasMoreData());

  EXPECT_TRUE(reader.SkipBits(24));
  EXPECT_EQ(reader.Tell(), 44);  // Include the skipped escape byte
  EXPECT_TRUE(reader.HasMoreData());

  EXPECT_TRUE(reader.SkipBits(8));
  EXPECT_EQ(reader.Tell(), 52);  // Include the skipped escape byte
  EXPECT_FALSE(reader.HasMoreData());

  EXPECT_TRUE(reader.SkipBits(0));
  EXPECT_FALSE(reader.SkipBits(1));
  EXPECT_FALSE(reader.HasMoreData());
}
