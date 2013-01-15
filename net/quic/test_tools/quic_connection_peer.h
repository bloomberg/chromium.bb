// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_CONNECTION_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_CONNECTION_PEER_H_

#include "base/basictypes.h"

namespace net {

struct QuicAckFrame;
class QuicConnection;
class QuicConnectionVisitorInterface;
class QuicReceiptMetricsCollector;
class QuicSendScheduler;

namespace test {

// Peer to make public a number of otherwise private QuicConnection methods.
class QuicConnectionPeer {
 public:
  static void SendAck(QuicConnection* connection);

  static void SetCollector(QuicConnection* connection,
                           QuicReceiptMetricsCollector* collector);

  static void SetScheduler(QuicConnection* connection,
                           QuicSendScheduler* scheduler);

  static QuicAckFrame* GetOutgoingAck(QuicConnection* connection);

  static QuicConnectionVisitorInterface* GetVisitor(
      QuicConnection* connection);

  static bool GetReceivedTruncatedAck(QuicConnection* connection);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicConnectionPeer);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_TEST_PEER_H_
