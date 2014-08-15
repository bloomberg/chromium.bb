// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>
#include "media/cast/cast_defines.h"
#include "media/cast/net/cast_transport_defines.h"

namespace media {
namespace cast {

class FrameIdWrapHelperTest : public ::testing::Test {
 protected:
  FrameIdWrapHelperTest() {}
  virtual ~FrameIdWrapHelperTest() {}

  void RunOneTest(uint32 starting_point, int iterations) {
    const int window_size = 127;
    uint32 window_base = starting_point;
    frame_id_wrap_helper_.largest_frame_id_seen_ = starting_point;
    for (int i = 0; i < iterations; i++) {
      uint32 largest_frame_id_seen =
          frame_id_wrap_helper_.largest_frame_id_seen_;
      int offset = rand() % window_size;
      uint32 frame_id = window_base + offset;
      uint32 mapped_frame_id =
          frame_id_wrap_helper_.MapTo32bitsFrameId(frame_id & 0xff);
      EXPECT_EQ(frame_id, mapped_frame_id)
          << " Largest ID seen: " << largest_frame_id_seen
          << " Window base: " << window_base
          << " Offset: " << offset;
      window_base = frame_id;
    }
  }

  FrameIdWrapHelper frame_id_wrap_helper_;

  DISALLOW_COPY_AND_ASSIGN(FrameIdWrapHelperTest);
};

TEST_F(FrameIdWrapHelperTest, FirstFrame) {
  EXPECT_EQ(kStartFrameId, frame_id_wrap_helper_.MapTo32bitsFrameId(255u));
}

TEST_F(FrameIdWrapHelperTest, Rollover) {
  uint32 new_frame_id = 0u;
  for (int i = 0; i <= 256; ++i) {
    new_frame_id =
        frame_id_wrap_helper_.MapTo32bitsFrameId(static_cast<uint8>(i));
  }
  EXPECT_EQ(256u, new_frame_id);
}

TEST_F(FrameIdWrapHelperTest, OutOfOrder) {
  uint32 new_frame_id = 0u;
  for (int i = 0; i < 255; ++i) {
    new_frame_id =
        frame_id_wrap_helper_.MapTo32bitsFrameId(static_cast<uint8>(i));
  }
  EXPECT_EQ(254u, new_frame_id);
  new_frame_id = frame_id_wrap_helper_.MapTo32bitsFrameId(0u);
  EXPECT_EQ(256u, new_frame_id);
  new_frame_id = frame_id_wrap_helper_.MapTo32bitsFrameId(255u);
  EXPECT_EQ(255u, new_frame_id);
  new_frame_id = frame_id_wrap_helper_.MapTo32bitsFrameId(1u);
  EXPECT_EQ(257u, new_frame_id);
}

TEST_F(FrameIdWrapHelperTest, Windowed) {
  srand(0);
  for (int i = 0; i < 50000 && !HasFailure(); i++) {
    RunOneTest(i * 4711, 20);
    // Test wrap-around scenarios.
    RunOneTest(0x7fffff00ul, 20);
    RunOneTest(0xffffff00ul, 20);
  }
}

}  // namespace cast
}  // namespace media
