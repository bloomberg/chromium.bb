// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/rtp_sender/packet_storage/packet_storage.h"

#include <stdint.h>

#include <vector>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {
namespace transport {

static int kStoredFrames = 10;

class PacketStorageTest : public ::testing::Test {
 protected:
  PacketStorageTest() : packet_storage_(kStoredFrames) {
  }

  PacketStorage packet_storage_;

  DISALLOW_COPY_AND_ASSIGN(PacketStorageTest);
};

TEST_F(PacketStorageTest, PacketContent) {
  base::TimeTicks frame_tick;
  for (uint32 frame_id = 0; frame_id < 200; ++frame_id) {
    for (uint16 packet_id = 0; packet_id < 5; ++packet_id) {
      Packet test_packet(frame_id + 1, packet_id);
      packet_storage_.StorePacket(
          frame_id,
          packet_id,
          PacedPacketSender::MakePacketKey(frame_tick,
                                           1, // ssrc
                                           packet_id),
          new base::RefCountedData<Packet>(test_packet));
    }

    for (uint32 f = 0; f <= frame_id; f++) {
      for (uint16 packet_id = 0; packet_id < 5; ++packet_id) {
        SendPacketVector packets;
        if (packet_storage_.GetPacket32(f, packet_id, &packets)) {
          EXPECT_GT(f + kStoredFrames, frame_id);
          EXPECT_EQ(f + 1, packets.back().second->data.size());
          EXPECT_EQ(packet_id, packets.back().second->data[0]);
          EXPECT_TRUE(packet_storage_.GetPacket(f & 0xff, packet_id, &packets));
          EXPECT_TRUE(packets.back().second->data ==
                      packets.front().second->data);
        } else {
          EXPECT_LE(f + kStoredFrames, frame_id);
        }
      }
    }
  }
}

}  // namespace transport
}  // namespace cast
}  // namespace media
