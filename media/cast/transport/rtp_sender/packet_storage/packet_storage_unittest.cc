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

static const int kMaxDeltaStoredMs = 500;
static const base::TimeDelta kDeltaBetweenFrames =
    base::TimeDelta::FromMilliseconds(33);

static const int64 kStartMillisecond = INT64_C(12345678900000);

class PacketStorageTest : public ::testing::Test {
 protected:
  PacketStorageTest() : packet_storage_(&testing_clock_, kMaxDeltaStoredMs) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  base::SimpleTestTickClock testing_clock_;
  PacketStorage packet_storage_;

  DISALLOW_COPY_AND_ASSIGN(PacketStorageTest);
};

TEST_F(PacketStorageTest, TimeOut) {
  Packet test_123(100, 123);  // 100 insertions of the value 123.
  PacketList packets;
  for (uint32 frame_id = 0; frame_id < 30; ++frame_id) {
    for (uint16 packet_id = 0; packet_id < 10; ++packet_id) {
      packet_storage_.StorePacket(frame_id,
                                  packet_id,
                                  new base::RefCountedData<Packet>(test_123));
    }
    testing_clock_.Advance(kDeltaBetweenFrames);
  }

  // All packets belonging to the first 14 frames is expected to be expired.
  for (uint32 frame_id = 0; frame_id < 14; ++frame_id) {
    for (uint16 packet_id = 0; packet_id < 10; ++packet_id) {
      Packet packet;
      EXPECT_FALSE(packet_storage_.GetPacket(frame_id, packet_id, &packets));
    }
  }
  // All packets belonging to the next 15 frames is expected to be valid.
  for (uint32 frame_id = 14; frame_id < 30; ++frame_id) {
    for (uint16 packet_id = 0; packet_id < 10; ++packet_id) {
      EXPECT_TRUE(packet_storage_.GetPacket(frame_id, packet_id, &packets));
      EXPECT_TRUE(packets.front()->data == test_123);
    }
  }
}

TEST_F(PacketStorageTest, MaxNumberOfPackets) {
  Packet test_123(100, 123);  // 100 insertions of the value 123.
  PacketList packets;

  uint32 frame_id = 0;
  for (uint16 packet_id = 0; packet_id <= PacketStorage::kMaxStoredPackets;
       ++packet_id) {
    packet_storage_.StorePacket(frame_id,
                                packet_id,
                                new base::RefCountedData<Packet>(test_123));
  }
  Packet packet;
  uint16 packet_id = 0;
  EXPECT_FALSE(packet_storage_.GetPacket(frame_id, packet_id, &packets));

  ++packet_id;
  for (; packet_id <= PacketStorage::kMaxStoredPackets; ++packet_id) {
    EXPECT_TRUE(packet_storage_.GetPacket(frame_id, packet_id, &packets));
    EXPECT_TRUE(packets.back()->data == test_123);
  }
}

TEST_F(PacketStorageTest, PacketContent) {
  Packet test_123(100, 123);  // 100 insertions of the value 123.
  Packet test_234(200, 234);  // 200 insertions of the value 234.
  PacketList packets;

  for (uint32 frame_id = 0; frame_id < 10; ++frame_id) {
    for (uint16 packet_id = 0; packet_id < 10; ++packet_id) {
      // Every other packet.
      if (packet_id % 2 == 0) {
        packet_storage_.StorePacket(frame_id,
                                    packet_id,
                                    new base::RefCountedData<Packet>(test_123));
      } else {
        packet_storage_.StorePacket(frame_id,
                                    packet_id,
                                    new base::RefCountedData<Packet>(test_234));
      }
    }
    testing_clock_.Advance(kDeltaBetweenFrames);
  }
  for (uint32 frame_id = 0; frame_id < 10; ++frame_id) {
    for (uint16 packet_id = 0; packet_id < 10; ++packet_id) {
      EXPECT_TRUE(packet_storage_.GetPacket(frame_id, packet_id, &packets));
      // Every other packet.
      if (packet_id % 2 == 0) {
        EXPECT_TRUE(packets.back()->data == test_123);
      } else {
        EXPECT_TRUE(packets.back()->data == test_234);
      }
    }
  }
}

}  // namespace transport
}  // namespace cast
}  // namespace media
