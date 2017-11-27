// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_control_frame_manager.h"

#include "net/quic/platform/api/quic_test.h"

namespace net {
namespace test {
namespace {

const QuicStreamId kTestStreamId = 5;

class QuicControlFrameManagerTest : public QuicTest {
 public:
  QuicControlFrameManagerTest()
      : rst_stream_(1, kTestStreamId, QUIC_STREAM_CANCELLED, 0),
        goaway_(2, QUIC_PEER_GOING_AWAY, kTestStreamId, "Going away."),
        window_update_(3, kTestStreamId, 100),
        blocked_(4, kTestStreamId) {}

  QuicControlFrameManager manager_;
  QuicRstStreamFrame rst_stream_;
  QuicGoAwayFrame goaway_;
  QuicWindowUpdateFrame window_update_;
  QuicBlockedFrame blocked_;
};

TEST_F(QuicControlFrameManagerTest, OnControlFrameAcked) {
  EXPECT_EQ(0u, manager_.size());
  manager_.OnControlFrameSent(QuicFrame(&rst_stream_));
  manager_.OnControlFrameSent(QuicFrame(&goaway_));
  manager_.OnControlFrameSent(QuicFrame(&window_update_));
  manager_.OnControlFrameSent(QuicFrame(&blocked_));
  manager_.OnControlFrameSent(QuicFrame(QuicPingFrame(5)));
  EXPECT_EQ(5u, manager_.size());

  EXPECT_TRUE(manager_.IsControlFrameOutstanding(QuicFrame(&window_update_)));
  manager_.OnControlFrameAcked(QuicFrame(&window_update_));
  EXPECT_FALSE(manager_.IsControlFrameOutstanding(QuicFrame(&window_update_)));
  EXPECT_EQ(5u, manager_.size());

  manager_.OnControlFrameAcked(QuicFrame(&goaway_));
  EXPECT_EQ(5u, manager_.size());
  manager_.OnControlFrameAcked(QuicFrame(&rst_stream_));
  EXPECT_EQ(2u, manager_.size());
}

TEST_F(QuicControlFrameManagerTest, OnControlFrameLost) {
  EXPECT_FALSE(manager_.HasPendingRetransmission());
  EXPECT_EQ(0u, manager_.size());
  manager_.OnControlFrameSent(QuicFrame(&rst_stream_));
  manager_.OnControlFrameSent(QuicFrame(&goaway_));
  manager_.OnControlFrameSent(QuicFrame(&window_update_));
  manager_.OnControlFrameSent(QuicFrame(&blocked_));
  manager_.OnControlFrameSent(QuicFrame(QuicPingFrame(5)));
  EXPECT_EQ(5u, manager_.size());

  manager_.OnControlFrameLost(QuicFrame(&window_update_));
  manager_.OnControlFrameLost(QuicFrame(&rst_stream_));
  EXPECT_TRUE(manager_.HasPendingRetransmission());
  EXPECT_EQ(window_update_.control_frame_id,
            manager_.NextPendingRetransmission()
                .window_update_frame->control_frame_id);
  manager_.OnControlFrameSent(QuicFrame(&window_update_));
  EXPECT_TRUE(manager_.IsControlFrameOutstanding(QuicFrame(&window_update_)));

  EXPECT_TRUE(manager_.HasPendingRetransmission());
  EXPECT_EQ(
      rst_stream_.control_frame_id,
      manager_.NextPendingRetransmission().rst_stream_frame->control_frame_id);
  manager_.OnControlFrameAcked(QuicFrame(&rst_stream_));
  EXPECT_FALSE(manager_.IsControlFrameOutstanding(QuicFrame(&rst_stream_)));

  EXPECT_FALSE(manager_.HasPendingRetransmission());
  EXPECT_EQ(4u, manager_.size());
}

}  // namespace
}  // namespace test
}  // namespace net
