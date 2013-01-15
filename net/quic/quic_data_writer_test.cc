// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_data_writer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

TEST(QuicDataWriterTest, WriteUint8ToOffset) {
  QuicDataWriter writer(4);

  EXPECT_TRUE(writer.WriteUInt8ToOffset(1, 0));
  EXPECT_TRUE(writer.WriteUInt8ToOffset(2, 1));
  EXPECT_TRUE(writer.WriteUInt8ToOffset(3, 2));
  EXPECT_TRUE(writer.WriteUInt8ToOffset(4, 3));

  char* data = writer.take();

  EXPECT_EQ(1, data[0]);
  EXPECT_EQ(2, data[1]);
  EXPECT_EQ(3, data[2]);
  EXPECT_EQ(4, data[3]);

  delete[] data;
}

TEST(QuicDataWriterDeathTest, WriteUint8ToOffset) {
  QuicDataWriter writer(4);

#if !defined(WIN32) && defined(GTEST_HAS_DEATH_TEST)
#if !defined(DCHECK_ALWAYS_ON)
  EXPECT_DEBUG_DEATH(writer.WriteUInt8ToOffset(5, 4), "Check failed");
#else
  EXPECT_DEATH(writer.WriteUInt8ToOffset(5, 4), "Check failed");
#endif
#endif
}

}  // namespace
}  // namespace test
}  // namespace net
