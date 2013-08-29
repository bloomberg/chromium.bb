// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtp_sender/packet_storage/packet_storage.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"

namespace media {
namespace cast {

static const int kMaxDeltaStoredMs = 500;
static const base::TimeDelta kDeltaBetweenFrames =
    base::TimeDelta::FromMilliseconds(33);

static const int64 kStartMillisecond = 123456789;

class PacketStorageTest : public ::testing::Test {
 protected:
  PacketStorageTest() : packet_storage_(kMaxDeltaStoredMs) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
    packet_storage_.set_clock(&testing_clock_);
  }

  PacketStorage packet_storage_;
  base::SimpleTestTickClock testing_clock_;
};

TEST_F(PacketStorageTest, TimeOut) {
  std::vector<uint8> test_123(100, 123);  // 100 insertions of the value 123.

  for (uint8 frame_id = 0; frame_id < 30; ++frame_id) {
    for (uint16 packet_id = 0; packet_id < 10; ++packet_id) {
      packet_storage_.StorePacket(frame_id, packet_id, test_123);
    }
    testing_clock_.Advance(kDeltaBetweenFrames);
  }

  // All packets belonging to the first 14 frames is expected to be expired.
  for (uint8 frame_id = 0; frame_id < 14; ++frame_id) {
    for (uint16 packet_id = 0; packet_id < 10; ++packet_id) {
      std::vector<uint8> packet;
      EXPECT_FALSE(packet_storage_.GetPacket(frame_id, packet_id, &packet));
    }
  }
  // All packets belonging to the next 15 frames is expected to be valid.
  for (uint8 frame_id = 14; frame_id < 30; ++frame_id) {
    for (uint16 packet_id = 0; packet_id < 10; ++packet_id) {
      std::vector<uint8> packet;
      EXPECT_TRUE(packet_storage_.GetPacket(frame_id, packet_id, &packet));
      EXPECT_TRUE(packet == test_123);
    }
  }
}

TEST_F(PacketStorageTest, MaxNumberOfPackets) {
  std::vector<uint8> test_123(100, 123);  // 100 insertions of the value 123.

  uint8 frame_id = 0;
  for (uint16 packet_id = 0; packet_id <= PacketStorage::kMaxStoredPackets;
      ++packet_id) {
    packet_storage_.StorePacket(frame_id, packet_id, test_123);
  }
  std::vector<uint8> packet;
  uint16 packet_id = 0;
  EXPECT_FALSE(packet_storage_.GetPacket(frame_id, packet_id, &packet));

  ++packet_id;
  for (; packet_id <= PacketStorage::kMaxStoredPackets; ++packet_id) {
    std::vector<uint8> packet;
    EXPECT_TRUE(packet_storage_.GetPacket(frame_id, packet_id, &packet));
    EXPECT_TRUE(packet == test_123);
  }
}

TEST_F(PacketStorageTest, PacketContent) {
  std::vector<uint8> test_123(100, 123);  // 100 insertions of the value 123.
  std::vector<uint8> test_234(200, 234);  // 200 insertions of the value 234.

  for (uint8 frame_id = 0; frame_id < 10; ++frame_id) {
    for (uint16 packet_id = 0; packet_id < 10; ++packet_id) {
      // Every other packet.
      if (packet_id % 2 == 0) {
        packet_storage_.StorePacket(frame_id, packet_id, test_123);
      } else {
        packet_storage_.StorePacket(frame_id, packet_id, test_234);
      }
    }
    testing_clock_.Advance(kDeltaBetweenFrames);
  }
  for (uint8 frame_id = 0; frame_id < 10; ++frame_id) {
    for (uint16 packet_id = 0; packet_id < 10; ++packet_id) {
      std::vector<uint8> packet;
      EXPECT_TRUE(packet_storage_.GetPacket(frame_id, packet_id, &packet));
      // Every other packet.
      if (packet_id % 2 == 0) {
        EXPECT_TRUE(packet == test_123);
      } else {
        EXPECT_TRUE(packet == test_234);
      }
    }
  }
}

}  // namespace cast
}  // namespace media

