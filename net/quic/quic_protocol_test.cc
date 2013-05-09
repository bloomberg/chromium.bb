// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_protocol.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

TEST(QuicProtocolTest, MakeQuicTag) {
  QuicTag tag = MakeQuicTag('A', 'B', 'C', 'D');
  char bytes[4];
  memcpy(bytes, &tag, 4);
  EXPECT_EQ('A', bytes[0]);
  EXPECT_EQ('B', bytes[1]);
  EXPECT_EQ('C', bytes[2]);
  EXPECT_EQ('D', bytes[3]);
}

TEST(QuicProtocolTest, IsAawaitingPacket) {
  ReceivedPacketInfo received_info;
  received_info.largest_observed = 10u;
  EXPECT_TRUE(IsAwaitingPacket(received_info, 11u));
  EXPECT_FALSE(IsAwaitingPacket(received_info, 1u));

  received_info.missing_packets.insert(10);
  EXPECT_TRUE(IsAwaitingPacket(received_info, 10u));
}

TEST(QuicProtocolTest, InsertMissingPacketsBetween) {
  ReceivedPacketInfo received_info;
  InsertMissingPacketsBetween(&received_info, 4u, 10u);
  EXPECT_EQ(6u, received_info.missing_packets.size());

  QuicPacketSequenceNumber i = 4;
  for (SequenceNumberSet::iterator it = received_info.missing_packets.begin();
       it != received_info.missing_packets.end(); ++it, ++i) {
    EXPECT_EQ(i, *it);
  }
}

}  // namespace
}  // namespace test
}  // namespace net
