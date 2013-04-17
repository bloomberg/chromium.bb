// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_buffer.h"

#include <cstddef>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/io_buffer.h"
#include "net/spdy/spdy_protocol.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const char kData[] = "hello!\0hi.";
const size_t kDataSize = arraysize(kData);

class SpdyBufferTest : public ::testing::Test {};

// Make a string from the data remaining in |buffer|.
std::string BufferToString(const SpdyBuffer& buffer) {
  return std::string(buffer.GetRemainingData(), buffer.GetRemainingSize());
}

// Construct a SpdyBuffer from a SpdyFrame and make sure its data
// points to the frame's underlying data.
TEST_F(SpdyBufferTest, FrameConstructor) {
  SpdyBuffer buffer(
      scoped_ptr<SpdyFrame>(
          new SpdyFrame(const_cast<char*>(kData), kDataSize,
                        false /* owns_buffer */)));

  EXPECT_EQ(kData, buffer.GetRemainingData());
  EXPECT_EQ(kDataSize, buffer.GetRemainingSize());
}

// Construct a SpdyBuffer from a const char*/size_t pair and make sure
// it makes a copy of the data.
TEST_F(SpdyBufferTest, DataConstructor) {
  std::string data(kData, kDataSize);
  SpdyBuffer buffer(data.data(), data.size());
  // This mutation shouldn't affect |buffer|'s data.
  data[0] = 'H';

  EXPECT_NE(kData, buffer.GetRemainingData());
  EXPECT_EQ(kDataSize, buffer.GetRemainingSize());
  EXPECT_EQ(std::string(kData, kDataSize), BufferToString(buffer));
}

// Construct a SpdyBuffer and call Consume() on it, which should
// update the remaining data pointer and size appropriately.
TEST_F(SpdyBufferTest, Consume) {
  SpdyBuffer buffer(kData, kDataSize);

  EXPECT_EQ(std::string(kData, kDataSize), BufferToString(buffer));

  buffer.Consume(5);
  EXPECT_EQ(std::string(kData + 5, kDataSize - 5), BufferToString(buffer));

  buffer.Consume(kDataSize - 5);
  EXPECT_EQ(0u, buffer.GetRemainingSize());
}

// Make sure the IOBuffer returned by GetIOBufferForRemainingData()
// points to the buffer's remaining data and isn't updated by
// Consume().
TEST_F(SpdyBufferTest, GetIOBufferForRemainingData) {
  SpdyBuffer buffer(kData, kDataSize);

  buffer.Consume(5);
  scoped_refptr<IOBuffer> io_buffer = buffer.GetIOBufferForRemainingData();
  size_t io_buffer_size = buffer.GetRemainingSize();
  const std::string expectedData(kData + 5, kDataSize - 5);
  EXPECT_EQ(expectedData, std::string(io_buffer->data(), io_buffer_size));

  buffer.Consume(kDataSize - 5);
  EXPECT_EQ(expectedData, std::string(io_buffer->data(), io_buffer_size));
}

}  // namespace

}  // namespace net
