// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_connection_peer.h"

#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/congestion_control/quic_send_scheduler.h"
#include "net/quic/quic_connection.h"

namespace net {
namespace test {

// static
void QuicConnectionPeer::SendAck(QuicConnection* connection) {
  connection->SendAck();
}

// static
void QuicConnectionPeer::SetCollector(QuicConnection* connection,
                                      QuicReceiptMetricsCollector* collector) {
  connection->collector_.reset(collector);
}

// static
void QuicConnectionPeer::SetScheduler(QuicConnection* connection,
                                      QuicSendScheduler* scheduler) {
  connection->scheduler_.reset(scheduler);
}

// static
QuicAckFrame* QuicConnectionPeer::GetOutgoingAck(QuicConnection* connection) {
  return &connection->outgoing_ack_;
}

// static
QuicConnectionVisitorInterface* QuicConnectionPeer::GetVisitor(
    QuicConnection* connection) {
  return connection->visitor_;
}

bool QuicConnectionPeer::GetReceivedTruncatedAck(QuicConnection* connection) {
    return connection->received_truncated_ack_;
}

}  // namespace test
}  // namespace net
