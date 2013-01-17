// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(DataBufferTest, Constructor_ZeroSize) {
  // Zero-sized buffers are valid. In practice they aren't used very much but it
  // eliminates clients from worrying about null data pointers.
  scoped_refptr<DataBuffer> buffer = new DataBuffer(0);
  EXPECT_TRUE(buffer->GetData());
  EXPECT_TRUE(buffer->GetWritableData());
  EXPECT_EQ(0, buffer->GetDataSize());
  EXPECT_FALSE(buffer->IsEndOfStream());
}

TEST(DataBufferTest, Constructor_NonZeroSize) {
  // Buffer size should be set.
  scoped_refptr<DataBuffer> buffer = new DataBuffer(10);
  EXPECT_TRUE(buffer->GetData());
  EXPECT_TRUE(buffer->GetWritableData());
  EXPECT_EQ(0, buffer->GetDataSize());
  EXPECT_FALSE(buffer->IsEndOfStream());
}

TEST(DataBufferTest, Constructor_ScopedArray) {
  // Data should be passed and both data and buffer size should be set.
  const int kSize = 8;
  scoped_array<uint8> data(new uint8[kSize]);
  const uint8* kData = data.get();

  scoped_refptr<DataBuffer> buffer = new DataBuffer(data.Pass(), kSize);
  EXPECT_TRUE(buffer->GetData());
  EXPECT_TRUE(buffer->GetWritableData());
  EXPECT_EQ(kData, buffer->GetData());
  EXPECT_EQ(kSize, buffer->GetDataSize());
  EXPECT_FALSE(buffer->IsEndOfStream());
}

TEST(DataBufferTest, CopyFrom) {
  const uint8 kTestData[] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };
  const int kTestDataSize = arraysize(kTestData);

  scoped_refptr<DataBuffer> buffer =
      DataBuffer::CopyFrom(kTestData, kTestDataSize);
  EXPECT_EQ(kTestDataSize, buffer->GetDataSize());
  EXPECT_FALSE(buffer->IsEndOfStream());

  // Ensure we are copying the data, not just pointing to the original data.
  EXPECT_EQ(0, memcmp(buffer->GetData(), kTestData, kTestDataSize));
  buffer->GetWritableData()[0] = 0xFF;
  EXPECT_NE(0, memcmp(buffer->GetData(), kTestData, kTestDataSize));
}

TEST(DataBufferTest, CreateEOSBuffer) {
  scoped_refptr<DataBuffer> buffer = DataBuffer::CreateEOSBuffer();
  EXPECT_TRUE(buffer->IsEndOfStream());
}

TEST(DataBufferTest, Timestamp) {
  const base::TimeDelta kZero;
  const base::TimeDelta kTimestampA = base::TimeDelta::FromMicroseconds(1337);
  const base::TimeDelta kTimestampB = base::TimeDelta::FromMicroseconds(1234);

  scoped_refptr<DataBuffer> buffer = new DataBuffer(0);
  EXPECT_TRUE(buffer->GetTimestamp() == kZero);

  buffer->SetTimestamp(kTimestampA);
  EXPECT_TRUE(buffer->GetTimestamp() == kTimestampA);

  buffer->SetTimestamp(kTimestampB);
  EXPECT_TRUE(buffer->GetTimestamp() == kTimestampB);
}

TEST(DataBufferTest, Duration) {
  const base::TimeDelta kZero;
  const base::TimeDelta kDurationA = base::TimeDelta::FromMicroseconds(1337);
  const base::TimeDelta kDurationB = base::TimeDelta::FromMicroseconds(1234);

  scoped_refptr<DataBuffer> buffer = new DataBuffer(0);
  EXPECT_TRUE(buffer->GetDuration() == kZero);

  buffer->SetDuration(kDurationA);
  EXPECT_TRUE(buffer->GetDuration() == kDurationA);

  buffer->SetDuration(kDurationB);
  EXPECT_TRUE(buffer->GetDuration() == kDurationB);
}

TEST(DataBufferTest, ReadingWriting) {
  const char kData[] = "hello";
  const int kDataSize = arraysize(kData);
  const char kNewData[] = "chromium";
  const int kNewDataSize = arraysize(kNewData);

  // Create a DataBuffer.
  scoped_refptr<DataBuffer> buffer(new DataBuffer(kDataSize));
  ASSERT_TRUE(buffer);

  uint8* data = buffer->GetWritableData();
  ASSERT_TRUE(data);
  memcpy(data, kData, kDataSize);
  buffer->SetDataSize(kDataSize);
  const uint8* read_only_data = buffer->GetData();
  ASSERT_EQ(data, read_only_data);
  ASSERT_EQ(0, memcmp(read_only_data, kData, kDataSize));
  EXPECT_FALSE(buffer->IsEndOfStream());

  scoped_refptr<DataBuffer> buffer2(new DataBuffer(kNewDataSize + 10));
  data = buffer2->GetWritableData();
  ASSERT_TRUE(data);
  memcpy(data, kNewData, kNewDataSize);
  buffer2->SetDataSize(kNewDataSize);
  read_only_data = buffer2->GetData();
  EXPECT_EQ(kNewDataSize, buffer2->GetDataSize());
  ASSERT_EQ(data, read_only_data);
  EXPECT_EQ(0, memcmp(read_only_data, kNewData, kNewDataSize));
}

}  // namespace media
