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

using ::testing::_;

TEST(QuicFlowControllerTest, SendingBytes) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control_2, true);

  const QuicStreamId kStreamId = 1234;
  const uint64 kSendWindow = 100;
  const uint64 kReceiveWindow = 200;
  const uint64 kMaxReceiveWindow = 200;

  QuicFlowController fc(QuicVersionMax(), kStreamId, false, kSendWindow,
                        kReceiveWindow, kMaxReceiveWindow);

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
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control_2, true);

  const QuicStreamId kStreamId = 1234;
  const uint64 kSendWindow = 100;
  const uint64 kReceiveWindow = 200;
  const uint64 kMaxReceiveWindow = 200;

  QuicFlowController fc(QuicVersionMax(), kStreamId, false, kSendWindow,
                        kReceiveWindow, kMaxReceiveWindow);

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
  EXPECT_CALL(connection, SendWindowUpdate(kStreamId, _)).Times(1);
  fc.MaybeSendWindowUpdate(&connection);
}

TEST(QuicFlowControllerTest, DisabledWhenQuicVersionDoesNotSupportFlowControl) {
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control_2, true);

  const QuicStreamId kStreamId = 1234;
  const uint64 kSendWindow = 100;
  const uint64 kReceiveWindow = 200;
  const uint64 kMaxReceiveWindow = 200;

  QuicFlowController fc(QUIC_VERSION_16, kStreamId, false, kSendWindow,
                        kReceiveWindow, kMaxReceiveWindow);

  // Should not be enabled, and should not report as blocked.
  EXPECT_FALSE(fc.IsEnabled());
  EXPECT_FALSE(fc.IsBlocked());
  EXPECT_FALSE(fc.FlowControlViolation());

  // Any attempts to add/remove bytes should have no effect.
  EXPECT_EQ(kSendWindow, fc.SendWindowSize());
  EXPECT_EQ(kSendWindow, QuicFlowControllerPeer::SendWindowOffset(&fc));
  EXPECT_EQ(kReceiveWindow, QuicFlowControllerPeer::ReceiveWindowOffset(&fc));
  fc.AddBytesSent(123);
  fc.AddBytesConsumed(456);
  fc.AddBytesBuffered(789);
  fc.RemoveBytesBuffered(321);
  EXPECT_EQ(kSendWindow, fc.SendWindowSize());
  EXPECT_EQ(kSendWindow, QuicFlowControllerPeer::SendWindowOffset(&fc));
  EXPECT_EQ(kReceiveWindow, QuicFlowControllerPeer::ReceiveWindowOffset(&fc));

  // Any attempt to change offset should have no effect.
  EXPECT_EQ(kSendWindow, fc.SendWindowSize());
  EXPECT_EQ(kSendWindow, QuicFlowControllerPeer::SendWindowOffset(&fc));
  fc.UpdateSendWindowOffset(kSendWindow + 12345);
  EXPECT_EQ(kSendWindow, fc.SendWindowSize());
  EXPECT_EQ(kSendWindow, QuicFlowControllerPeer::SendWindowOffset(&fc));

  // Should never send WINDOW_UPDATE or BLOCKED frames, even if the internal
  // state implies that it should.
  MockConnection connection(false);

  // If the flow controller was enabled, then a send window size of 0 would
  // trigger a BLOCKED frame to be sent.
  EXPECT_EQ(kSendWindow, fc.SendWindowSize());
  EXPECT_CALL(connection, SendBlocked(_)).Times(0);
  fc.MaybeSendBlocked(&connection);

  // If the flow controller was enabled, then a WINDOW_UPDATE would be sent if
  // (receive window) < (max receive window / 2)
  QuicFlowControllerPeer::SetReceiveWindowOffset(&fc, kMaxReceiveWindow / 10);
  EXPECT_TRUE(QuicFlowControllerPeer::ReceiveWindowSize(&fc) <
              (kMaxReceiveWindow / 2));
  EXPECT_CALL(connection, SendWindowUpdate(_, _)).Times(0);
  fc.MaybeSendWindowUpdate(&connection);

  // Should not be enabled, and should not report as blocked.
  EXPECT_FALSE(fc.IsEnabled());
  EXPECT_FALSE(fc.IsBlocked());
  EXPECT_FALSE(fc.FlowControlViolation());
}

}  // namespace test
}  // namespace net
