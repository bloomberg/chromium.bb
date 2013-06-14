// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The test buffer data is 52 bytes, wrap position is set to 20 (this is
// arbitrarily chosen). The total buffer size is allocated dynamically based on
// the actual header size. This gives:
// Header of some size, non-wrapping part 20 bytes, wrapping part 32 bytes.
// As input data, a 14 byte array is used and repeatedly written. It's chosen
// not to be an integer factor smaller than the wrapping part. This ensures that
// the wrapped data isn't repeated at the same position.
// Note that desipte the number of wraps (if one or more), the reference output
// data is the same since the offset at each wrap is always the same.

#include "base/memory/scoped_ptr.h"
#include "chrome/common/partial_circular_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

const uint32 kWrapPosition = 20;
const uint8 kInputData[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
const uint8 kOutputRefDataWrap[] =
    // The 20 bytes in the non-wrapping part.
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 1, 2, 3, 4, 5, 6,
     // The 32 bytes in wrapping part.
     11, 12, 13, 14,
     1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
     1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

class PartialCircularBufferTest : public testing::Test {
 public:
  PartialCircularBufferTest() {
    PartialCircularBuffer::BufferData test_struct;
    buffer_header_size_ =
        &test_struct.data[0] - reinterpret_cast<uint8*>(&test_struct);

    buffer_.reset(new uint8[buffer_header_size_ + sizeof(kOutputRefDataWrap)]);
  }

  void InitWriteBuffer(bool append) {
    pcb_write_.reset(new PartialCircularBuffer(
        buffer_.get(),
        buffer_header_size_ + sizeof(kOutputRefDataWrap),
        kWrapPosition,
        append));
  }

  void WriteToBuffer(int num) {
    for (int i = 0; i < num; ++i)
      pcb_write_->Write(kInputData, sizeof(kInputData));
  }

  void InitReadBuffer() {
    pcb_read_.reset(new PartialCircularBuffer(
        buffer_.get(), buffer_header_size_ + sizeof(kOutputRefDataWrap)));
  }

 protected:
  scoped_ptr<PartialCircularBuffer> pcb_write_;
  scoped_ptr<PartialCircularBuffer> pcb_read_;
  scoped_ptr<uint8[]> buffer_;
  uint32 buffer_header_size_;

  DISALLOW_COPY_AND_ASSIGN(PartialCircularBufferTest);
};

TEST_F(PartialCircularBufferTest, NoWrapBeginningPartOnly) {
  InitWriteBuffer(false);
  WriteToBuffer(1);
  InitReadBuffer();

  uint8 output_data[sizeof(kInputData)] = {0};
  EXPECT_EQ(sizeof(output_data),
            pcb_read_->Read(output_data, sizeof(output_data)));

  EXPECT_EQ(0, memcmp(kInputData, output_data, sizeof(kInputData)));

  EXPECT_EQ(0u, pcb_read_->Read(output_data, sizeof(output_data)));
}

TEST_F(PartialCircularBufferTest, NoWrapBeginningAndEndParts) {
  InitWriteBuffer(false);
  WriteToBuffer(2);
  InitReadBuffer();

  uint8 output_data[2 * sizeof(kInputData)] = {0};
  EXPECT_EQ(sizeof(output_data),
            pcb_read_->Read(output_data, sizeof(output_data)));

  const uint8 output_ref_data[2 * sizeof(kInputData)] =
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
       1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
  EXPECT_EQ(0, memcmp(output_ref_data, output_data, sizeof(output_data)));

  EXPECT_EQ(0u, pcb_read_->Read(output_data, sizeof(output_data)));
}

TEST_F(PartialCircularBufferTest, WrapOnce) {
  InitWriteBuffer(false);
  WriteToBuffer(4);
  InitReadBuffer();

  uint8 output_data[sizeof(kOutputRefDataWrap)] = {0};
  EXPECT_EQ(sizeof(output_data),
            pcb_read_->Read(output_data, sizeof(output_data)));

  EXPECT_EQ(0, memcmp(kOutputRefDataWrap, output_data, sizeof(output_data)));

  EXPECT_EQ(0u, pcb_read_->Read(output_data, sizeof(output_data)));
}

TEST_F(PartialCircularBufferTest, WrapTwice) {
  InitWriteBuffer(false);
  WriteToBuffer(7);
  InitReadBuffer();

  uint8 output_data[sizeof(kOutputRefDataWrap)] = {0};
  EXPECT_EQ(sizeof(output_data),
            pcb_read_->Read(output_data, sizeof(output_data)));

  EXPECT_EQ(0, memcmp(kOutputRefDataWrap, output_data, sizeof(output_data)));

  EXPECT_EQ(0u, pcb_read_->Read(output_data, sizeof(output_data)));
}

TEST_F(PartialCircularBufferTest, WrapOnceSmallerOutputBuffer) {
  InitWriteBuffer(false);
  WriteToBuffer(4);
  InitReadBuffer();

  uint8 output_data[sizeof(kOutputRefDataWrap)] = {0};
  const uint32 size_per_read = 16;
  uint32 read = 0;
  for (; read + size_per_read <= sizeof(output_data); read += size_per_read) {
    EXPECT_EQ(size_per_read,
              pcb_read_->Read(output_data + read, size_per_read));
  }
  EXPECT_EQ(sizeof(output_data) - read,
            pcb_read_->Read(output_data + read, size_per_read));

  EXPECT_EQ(0, memcmp(kOutputRefDataWrap, output_data, sizeof(output_data)));

  EXPECT_EQ(0u, pcb_read_->Read(output_data, sizeof(output_data)));
}

TEST_F(PartialCircularBufferTest, WrapOnceWithAppend) {
  InitWriteBuffer(false);
  WriteToBuffer(2);
  InitWriteBuffer(true);
  WriteToBuffer(2);
  InitReadBuffer();

  uint8 output_data[sizeof(kOutputRefDataWrap)] = {0};
  EXPECT_EQ(sizeof(output_data),
            pcb_read_->Read(output_data, sizeof(output_data)));

  EXPECT_EQ(0, memcmp(kOutputRefDataWrap, output_data, sizeof(output_data)));

  EXPECT_EQ(0u, pcb_read_->Read(output_data, sizeof(output_data)));
}

TEST_F(PartialCircularBufferTest, WrapTwiceWithAppend) {
  InitWriteBuffer(false);
  WriteToBuffer(4);
  InitWriteBuffer(true);
  WriteToBuffer(3);
  InitReadBuffer();

  uint8 output_data[sizeof(kOutputRefDataWrap)] = {0};
  EXPECT_EQ(sizeof(output_data),
            pcb_read_->Read(output_data, sizeof(output_data)));

  EXPECT_EQ(0, memcmp(kOutputRefDataWrap, output_data, sizeof(output_data)));

  EXPECT_EQ(0u, pcb_read_->Read(output_data, sizeof(output_data)));
}

TEST_F(PartialCircularBufferTest, WrapOnceThenOverwriteWithNoWrap) {
  InitWriteBuffer(false);
  WriteToBuffer(4);
  InitWriteBuffer(false);
  WriteToBuffer(1);
  InitReadBuffer();

  uint8 output_data[sizeof(kInputData)] = {0};
  EXPECT_EQ(sizeof(output_data),
            pcb_read_->Read(output_data, sizeof(output_data)));

  EXPECT_EQ(0, memcmp(kInputData, output_data, sizeof(kInputData)));

  EXPECT_EQ(0u, pcb_read_->Read(output_data, sizeof(output_data)));
}
