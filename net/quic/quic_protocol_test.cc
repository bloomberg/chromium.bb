// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_protocol.h"

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

TEST(QuicProtocolTest, ReceivedInfo_RecordReceived) {
  ReceivedPacketInfo received_info;
  received_info.RecordReceived(1u);

  EXPECT_EQ(1u, received_info.largest_observed);
  EXPECT_EQ(0u, received_info.missing_packets.size());

  received_info.RecordReceived(3u);

  EXPECT_EQ(3u, received_info.largest_observed);
  EXPECT_EQ(1u, received_info.missing_packets.size());

  received_info.RecordReceived(2u);

  EXPECT_EQ(3u, received_info.largest_observed);
  EXPECT_EQ(0u, received_info.missing_packets.size());
}

TEST(QuicProtocolTest, ReceivedInfo_ClearMissingBefore) {
  ReceivedPacketInfo received_info;
  received_info.RecordReceived(1u);

  // Clear nothing.
  received_info.ClearMissingBefore(1);
  EXPECT_EQ(0u, received_info.missing_packets.size());

  received_info.RecordReceived(3u);

  // Clear the first packet.
  received_info.ClearMissingBefore(2);
  EXPECT_EQ(1u, received_info.missing_packets.size());

  // Clear the second packet, which has not been received.
  received_info.ClearMissingBefore(3);
  EXPECT_EQ(0u, received_info.missing_packets.size());
}

}  // namespace
}  // namespace test
}  // namespace net
