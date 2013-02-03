// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_connection_peer.h"

#include "base/stl_util.h"
#include "net/quic/congestion_control/quic_congestion_manager.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_connection.h"

namespace net {
namespace test {

// static
void QuicConnectionPeer::SendAck(QuicConnection* connection) {
  connection->SendAck();
}

// static
void QuicConnectionPeer::SetReceiveAlgorithm(
    QuicConnection* connection,
    ReceiveAlgorithmInterface* receive_algorithm) {
  connection->congestion_manager_.receive_algorithm_.reset(receive_algorithm);
}

// static
void QuicConnectionPeer::SetSendAlgorithm(
    QuicConnection* connection,
    SendAlgorithmInterface* send_algorithm) {
  connection->congestion_manager_.send_algorithm_.reset(send_algorithm);
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

// static
QuicPacketCreator* QuicConnectionPeer::GetPacketCreator(
    QuicConnection* connection) {
  return &connection->packet_creator_;
}

bool QuicConnectionPeer::GetReceivedTruncatedAck(QuicConnection* connection) {
    return connection->received_truncated_ack_;
}

// static
size_t QuicConnectionPeer::GetNumRetransmissionTimeouts(
    QuicConnection* connection) {
  return connection->retransmission_timeouts_.size();
}

// static
bool QuicConnectionPeer::IsSavedForRetransmission(
    QuicConnection* connection,
    QuicPacketSequenceNumber sequence_number) {
  return ContainsKey(connection->retransmission_map_, sequence_number);
}

// static
size_t QuicConnectionPeer::GetRetransmissionCount(
    QuicConnection* connection,
    QuicPacketSequenceNumber sequence_number) {
  QuicConnection::RetransmissionMap::iterator it =
      connection->retransmission_map_.find(sequence_number);
  return it->second.number_retransmissions;
}

}  // namespace test
}  // namespace net
