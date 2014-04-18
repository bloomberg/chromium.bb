// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_flow_controller.h"

#include "base/strings/stringprintf.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_flow_controller_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::StringPrintf;

namespace net {
namespace test {

TEST(QuicFlowControllerTest, SendingBytes) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control, true);

  const QuicStreamId kStreamId = 1234;
  const uint64 kSendWindow = 100;
  const uint64 kReceiveWindow = 200;
  const uint64 kMaxReceiveWindow = 200;

  QuicFlowController fc(kStreamId, false, kSendWindow, kReceiveWindow,
                        kMaxReceiveWindow);

  EXPECT_TRUE(fc.IsEnabled());
  EXPECT_FALSE(fc.IsBlocked());
  EXPECT_FALSE(fc.FlowControlViolation());
  EXPECT_EQ(kSendWindow, fc.SendWindowSize());

  // Send some bytes, but not enough to block.
  fc.AddBytesSent(kSendWindow / 2);
  EXPECT_FALSE(fc.IsBlocked());
  EXPECT_EQ(kSendWindow / 2, fc.SendWindowSize());

  // Send enough bytes to block.
  fc.AddBytesSent(kSendWindow / 2);
  EXPECT_TRUE(fc.IsBlocked());
  EXPECT_EQ(0u, fc.SendWindowSize());

  // BLOCKED frame should get sent.
  MockConnection connection(false);
  EXPECT_CALL(connection, SendBlocked(kStreamId)).Times(1);
  fc.MaybeSendBlocked(&connection);

  // Update the send window, and verify this has unblocked.
  EXPECT_TRUE(fc.UpdateSendWindowOffset(2 * kSendWindow));
  EXPECT_FALSE(fc.IsBlocked());
  EXPECT_EQ(kSendWindow, fc.SendWindowSize());

  // Updating with a smaller offset doesn't change anything.
  EXPECT_FALSE(fc.UpdateSendWindowOffset(kSendWindow / 10));
  EXPECT_EQ(kSendWindow, fc.SendWindowSize());

  // Try to send more bytes, violating flow control.
  EXPECT_DFATAL(fc.AddBytesSent(kSendWindow * 10),
                StringPrintf("Trying to send an extra %d bytes",
                             static_cast<int>(kSendWindow * 10)));
  EXPECT_TRUE(fc.IsBlocked());
  EXPECT_EQ(0u, fc.SendWindowSize());
}

TEST(QuicFlowControllerTest, ReceivingBytes) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control, true);

  const QuicStreamId kStreamId = 1234;
  const uint64 kSendWindow = 100;
  const uint64 kReceiveWindow = 200;
  const uint64 kMaxReceiveWindow = 200;

  QuicFlowController fc(kStreamId, false, kSendWindow, kReceiveWindow,
                        kMaxReceiveWindow);

  EXPECT_TRUE(fc.IsEnabled());
  EXPECT_FALSE(fc.IsBlocked());
  EXPECT_FALSE(fc.FlowControlViolation());

  // Buffer some bytes, not enough to fill window.
  fc.AddBytesBuffered(kReceiveWindow / 2);
  EXPECT_FALSE(fc.FlowControlViolation());
  EXPECT_EQ(kReceiveWindow / 2, QuicFlowControllerPeer::ReceiveWindowSize(&fc));

  // Consume enough bytes to send a WINDOW_UPDATE frame.
  fc.RemoveBytesBuffered(kReceiveWindow / 2);
  fc.AddBytesConsumed(1 + kReceiveWindow / 2);
  EXPECT_FALSE(fc.FlowControlViolation());
  EXPECT_EQ((kReceiveWindow / 2) - 1,
            QuicFlowControllerPeer::ReceiveWindowSize(&fc));

  MockConnection connection(false);
  EXPECT_CALL(connection, SendWindowUpdate(kStreamId, ::testing::_)).Times(1);
  fc.MaybeSendWindowUpdate(&connection);
}

}  // namespace test
}  // namespace net
