// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/time.h"
#include "media/base/buffer_queue.h"
#include "media/base/data_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class BufferQueueTest : public testing::Test {
 protected:
  BufferQueueTest() {
    buffer1 = new DataBuffer(kDataSize);
    buffer1->SetDataSize(kDataSize);
    data1 = buffer1->GetWritableData();

    buffer2 = new DataBuffer(kNewDataSize);
    buffer2->SetDataSize(kNewDataSize);
    data2 = buffer2->GetWritableData();

    bufferBig = new DataBuffer(2 * (kDataSize + kNewDataSize));
    bufferBig->SetDataSize(2 * (kDataSize + kNewDataSize));
    dataBig = bufferBig->GetWritableData();

    memcpy(data1, kData, kDataSize);
    memcpy(data2, kNewData, kNewDataSize);
  }

  BufferQueue queue_;

  static const char kData[];
  static const size_t kDataSize;
  static const char kNewData[];
  static const size_t kNewDataSize;
  scoped_refptr<DataBuffer> buffer1;
  scoped_refptr<DataBuffer> buffer2;
  scoped_refptr<DataBuffer> bufferBig;
  uint8* data1;
  uint8* data2;
  uint8* dataBig;

 private:
  DISALLOW_COPY_AND_ASSIGN(BufferQueueTest);
};

const char BufferQueueTest::kData[] = "hello";
const size_t BufferQueueTest::kDataSize = arraysize(BufferQueueTest::kData);
const char BufferQueueTest::kNewData[] = "chromium";
const size_t BufferQueueTest::kNewDataSize =
    arraysize(BufferQueueTest::kNewData);

TEST_F(BufferQueueTest, ValidTestData) {
  ASSERT_TRUE(kNewDataSize > kDataSize);
  ASSERT_TRUE(buffer1.get());
  ASSERT_TRUE(data1);
  ASSERT_TRUE(buffer2.get());
  ASSERT_TRUE(data2);
  ASSERT_TRUE(bufferBig.get());
  ASSERT_TRUE(dataBig);
}

TEST_F(BufferQueueTest, Ctor) {
  EXPECT_TRUE(queue_.IsEmpty());
  EXPECT_EQ(0u, queue_.SizeInBytes());
}

TEST_F(BufferQueueTest, Enqueue) {
  queue_.Enqueue(buffer1.get());
  EXPECT_FALSE(queue_.IsEmpty());
  EXPECT_EQ(kDataSize, queue_.SizeInBytes());
}

TEST_F(BufferQueueTest, CopyWithOneBuffer) {
  queue_.Enqueue(buffer1.get());

  EXPECT_EQ(kDataSize, queue_.Copy(data2, kDataSize));
  EXPECT_EQ(0, memcmp(data2, kData, kDataSize));
}

TEST_F(BufferQueueTest, Clear) {
  queue_.Enqueue(buffer1.get());

  queue_.Clear();
  EXPECT_TRUE(queue_.IsEmpty());
  EXPECT_EQ(0u, queue_.SizeInBytes());
}

TEST_F(BufferQueueTest, MultipleEnqueues) {
  queue_.Enqueue(buffer2.get());
  queue_.Enqueue(buffer1.get());
  EXPECT_EQ(kDataSize + kNewDataSize, queue_.SizeInBytes());
}

TEST_F(BufferQueueTest, CopyWithMultipleBuffers) {
  queue_.Enqueue(buffer2.get());
  queue_.Enqueue(buffer1.get());

  EXPECT_EQ(kDataSize + kNewDataSize,
            queue_.Copy(dataBig, kDataSize + kNewDataSize));
  EXPECT_EQ(0, memcmp(dataBig, kNewData, kNewDataSize));
  dataBig += kNewDataSize;
  EXPECT_EQ(0, memcmp(dataBig, kData, kDataSize));
}

TEST_F(BufferQueueTest, Consume) {
  queue_.Enqueue(buffer2.get());
  queue_.Enqueue(buffer1.get());

  queue_.Consume(kDataSize);
  EXPECT_EQ(kNewDataSize, queue_.SizeInBytes());
}

TEST_F(BufferQueueTest, CopyFromMiddleOfBuffer) {
  queue_.Enqueue(buffer2.get());
  queue_.Enqueue(buffer1.get());
  queue_.Consume(kDataSize);

  EXPECT_EQ(kNewDataSize, queue_.Copy(dataBig, kNewDataSize));
  EXPECT_EQ(0, memcmp(dataBig,
                      kNewData + kDataSize,
                      kNewDataSize - kDataSize));
  dataBig += (kNewDataSize - kDataSize);
  EXPECT_EQ(0, memcmp(dataBig, kData, kDataSize));
}


TEST_F(BufferQueueTest, GetTime) {
  const struct {
    int64 first_time_useconds;
    int64 duration_useconds;
    size_t consume_bytes;
  } tests[] = {
    // Timestamps of 0 are treated as garbage.
    { 0, 1000000, 0 },
    { 0, 4000000, 0 },
    { 0, 8000000, 0 },
    // { 0, 1000000, 4 },
    // { 0, 4000000, 4 },
    // { 0, 8000000, 4 },
    // { 0, 1000000, kNewDataSize },
    // { 0, 4000000, kNewDataSize },
    // { 0, 8000000, kNewDataSize },
    { 5, 1000000, 0 },
    { 5, 4000000, 0 },
    { 5, 8000000, 0 },
    { 5, 1000000, 4 },
    { 5, 4000000, 4 },
    { 5, 8000000, 4 },
    { 5, 1000000, kNewDataSize },
    { 5, 4000000, kNewDataSize },
    { 5, 8000000, kNewDataSize },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    buffer2->SetTimestamp(base::TimeDelta::FromMicroseconds(
        tests[i].first_time_useconds));
    buffer2->SetDuration(base::TimeDelta::FromMicroseconds(
        tests[i].duration_useconds));
    queue_.Enqueue(buffer2.get());
    queue_.Consume(tests[i].consume_bytes);

    int64 expected = base::TimeDelta::FromMicroseconds(static_cast<int64>(
        tests[i].first_time_useconds + (tests[i].consume_bytes *
        tests[i].duration_useconds) / kNewDataSize)).ToInternalValue();

    int64 actual = queue_.GetTime().ToInternalValue();

    EXPECT_EQ(expected, actual) << "With test = { start:"
        << tests[i].first_time_useconds << ", duration:"
        << tests[i].duration_useconds << ", consumed:"
        << tests[i].consume_bytes << "}\n";

    queue_.Clear();
  }
}

}  // namespace media
