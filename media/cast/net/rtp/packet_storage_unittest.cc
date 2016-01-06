// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/packet_storage.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "media/cast/constants.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {
namespace cast {

static const size_t kStoredFrames = 10;

// Generate |number_of_frames| and store into |*storage|.
// First frame has 1 packet, second frame has 2 packets, etc.
static void StoreFrames(size_t number_of_frames,
                        uint32_t first_frame_id,
                        PacketStorage* storage) {
  const base::TimeTicks zero;
  const int kSsrc = 1;
  for (size_t i = 0; i < number_of_frames; ++i) {
    SendPacketVector packets;
    // First frame has 1 packet, second frame has 2 packets, etc.
    const size_t kNumberOfPackets = i + 1;
    for (size_t j = 0; j < kNumberOfPackets; ++j) {
      Packet test_packet(1, 0);
      packets.push_back(
          std::make_pair(PacedPacketSender::MakePacketKey(
                             zero, kSsrc, i, base::checked_cast<uint16_t>(j)),
                         new base::RefCountedData<Packet>(test_packet)));
    }
    storage->StoreFrame(first_frame_id, packets);
    ++first_frame_id;
  }
}

TEST(PacketStorageTest, NumberOfStoredFrames) {
  PacketStorage storage;

  uint32_t frame_id = 0;
  frame_id = ~frame_id;  // The maximum value of uint32_t.
  StoreFrames(kMaxUnackedFrames / 2, frame_id, &storage);
  EXPECT_EQ(static_cast<size_t>(kMaxUnackedFrames / 2),
            storage.GetNumberOfStoredFrames());
}

TEST(PacketStorageTest, GetFrameWrapAround8bits) {
  PacketStorage storage;

  const uint32_t kFirstFrameId = 250;
  StoreFrames(kStoredFrames, kFirstFrameId, &storage);
  EXPECT_EQ(std::min<size_t>(kMaxUnackedFrames, kStoredFrames),
            storage.GetNumberOfStoredFrames());

  // Expect we get the correct frames by looking at the number of
  // packets.
  uint32_t frame_id = kFirstFrameId;
  for (size_t i = 0; i < kStoredFrames; ++i) {
    ASSERT_TRUE(storage.GetFrame8(frame_id));
    EXPECT_EQ(i + 1, storage.GetFrame8(frame_id)->size());
    ++frame_id;
  }
}

TEST(PacketStorageTest, GetFrameWrapAround32bits) {
  PacketStorage storage;

  // First frame ID is close to the maximum value of uint32_t.
  uint32_t first_frame_id = 0xffffffff - 5;
  StoreFrames(kStoredFrames, first_frame_id, &storage);
  EXPECT_EQ(std::min<size_t>(kMaxUnackedFrames, kStoredFrames),
            storage.GetNumberOfStoredFrames());

  // Expect we get the correct frames by looking at the number of
  // packets.
  uint32_t frame_id = first_frame_id;
  for (size_t i = 0; i < kStoredFrames; ++i) {
    ASSERT_TRUE(storage.GetFrame8(frame_id));
    EXPECT_EQ(i + 1, storage.GetFrame8(frame_id)->size());
    ++frame_id;
  }
}

TEST(PacketStorageTest, FramesReleased) {
  PacketStorage storage;

  const uint32_t kFirstFrameId = 0;
  StoreFrames(5, kFirstFrameId, &storage);
  EXPECT_EQ(std::min<size_t>(kMaxUnackedFrames, 5),
            storage.GetNumberOfStoredFrames());

  for (uint32_t frame_id = kFirstFrameId; frame_id < kFirstFrameId + 5;
       ++frame_id) {
    EXPECT_TRUE(storage.GetFrame8(frame_id));
  }

  storage.ReleaseFrame(kFirstFrameId + 2);
  EXPECT_EQ(4u, storage.GetNumberOfStoredFrames());
  EXPECT_FALSE(storage.GetFrame8(kFirstFrameId + 2));

  storage.ReleaseFrame(kFirstFrameId + 0);
  EXPECT_EQ(3u, storage.GetNumberOfStoredFrames());
  EXPECT_FALSE(storage.GetFrame8(kFirstFrameId + 0));

  storage.ReleaseFrame(kFirstFrameId + 3);
  EXPECT_EQ(2u, storage.GetNumberOfStoredFrames());
  EXPECT_FALSE(storage.GetFrame8(kFirstFrameId + 3));

  storage.ReleaseFrame(kFirstFrameId + 4);
  EXPECT_EQ(1u, storage.GetNumberOfStoredFrames());
  EXPECT_FALSE(storage.GetFrame8(kFirstFrameId + 4));

  storage.ReleaseFrame(kFirstFrameId + 1);
  EXPECT_EQ(0u, storage.GetNumberOfStoredFrames());
  EXPECT_FALSE(storage.GetFrame8(kFirstFrameId + 1));
}

}  // namespace cast
}  // namespace media
