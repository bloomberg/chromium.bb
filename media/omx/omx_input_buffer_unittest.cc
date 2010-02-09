// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/omx/omx_input_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kSize = 256;

void GenerateData(uint8** data, int size) {
  *data = new uint8[size];
  for (int i = 0; i < size; ++i) {
    (*data)[i] = static_cast<uint8>(i);
  }
}

bool VerifyData(uint8* data, int size, int offset) {
  bool data_mismatch = false;
  for (int i = 0; i < size; ++i) {
    if (data[i] != static_cast<uint8>(offset + i)) {
      data_mismatch = true;
      break;
    }
  }
  return !data_mismatch;
}

}  // namespace

namespace media {

TEST(OmxInputBufferTest, Read) {
  scoped_array<uint8> read_buffer(new uint8[kSize]);
  uint8* data;
  GenerateData(&data, kSize);
  scoped_refptr<OmxInputBuffer> buffer(new OmxInputBuffer(data, kSize));
  EXPECT_FALSE(buffer->Used());

  // Perform the first read.
  EXPECT_EQ(10, buffer->Read(read_buffer.get(), 10));
  EXPECT_FALSE(buffer->Used());
  EXPECT_TRUE(VerifyData(read_buffer.get(), 10, 0));

  // Perform the second read.
  EXPECT_EQ(10, buffer->Read(read_buffer.get(), 10));
  EXPECT_FALSE(buffer->Used());
  EXPECT_TRUE(VerifyData(read_buffer.get(), 10, 10));

  // Perform the last read.
  EXPECT_EQ(kSize - 20, buffer->Read(read_buffer.get(), kSize));
  EXPECT_TRUE(buffer->Used());
  EXPECT_TRUE(VerifyData(read_buffer.get(), kSize - 20, 20));

  // This read will return nothing.
  EXPECT_EQ(0, buffer->Read(read_buffer.get(), kSize));
  EXPECT_TRUE(buffer->Used());
}

TEST(OmxInputBufferTest, EmptyBuffer) {
  scoped_refptr<OmxInputBuffer> buffer(new OmxInputBuffer(NULL, 0));
  EXPECT_TRUE(buffer->IsEndOfStream());
  EXPECT_TRUE(buffer->Used());
}

}  // namespace media
