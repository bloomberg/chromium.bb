// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_connection_peer.h"

#include "base/stl_util.h"
#include "net/quic/congestion_control/quic_congestion_manager.h"
#include "net/quic/congestion_control/receive_algorithm_interface.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/test_tools/quic_framer_peer.h"
#include "net/quic/test_tools/quic_test_writer.h"

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
QuicAckFrame* QuicConnectionPeer::CreateAckFrame(QuicConnection* connection) {
  return connection->CreateAckFrame();
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

// static
QuicCongestionManager* QuicConnectionPeer::GetCongestionManager(
    QuicConnection* connection) {
  return &connection->congestion_manager_;
}

bool QuicConnectionPeer::GetReceivedTruncatedAck(QuicConnection* connection) {
    return connection->received_truncated_ack_;
}

// static
QuicTime::Delta QuicConnectionPeer::GetNetworkTimeout(
    QuicConnection* connection) {
  return connection->idle_network_timeout_;
}

// static
bool QuicConnectionPeer::IsSavedForRetransmission(
    QuicConnection* connection,
    QuicPacketSequenceNumber sequence_number) {
  return connection->sent_packet_manager_.IsUnacked(sequence_number) &&
      connection->sent_packet_manager_.HasRetransmittableFrames(
          sequence_number);
}

// static
size_t QuicConnectionPeer::GetRetransmissionCount(
    QuicConnection* connection,
    QuicPacketSequenceNumber sequence_number) {
  return connection->sent_packet_manager_.GetRetransmissionCount(
      sequence_number);
}

// static
QuicPacketEntropyHash QuicConnectionPeer::GetSentEntropyHash(
    QuicConnection* connection,
    QuicPacketSequenceNumber sequence_number) {
  return connection->sent_entropy_manager_.EntropyHash(sequence_number);
}

// static
bool QuicConnectionPeer::IsValidEntropy(
    QuicConnection* connection,
    QuicPacketSequenceNumber largest_observed,
    const SequenceNumberSet& missing_packets,
    QuicPacketEntropyHash entropy_hash) {
  return connection->sent_entropy_manager_.IsValidEntropy(
      largest_observed, missing_packets, entropy_hash);
}

// static
QuicPacketEntropyHash QuicConnectionPeer::ReceivedEntropyHash(
    QuicConnection* connection,
    QuicPacketSequenceNumber sequence_number) {
  return connection->received_packet_manager_.EntropyHash(
      sequence_number);
}

// static
bool QuicConnectionPeer::IsWriteBlocked(QuicConnection* connection) {
  return connection->write_blocked_;
}

// static
void QuicConnectionPeer::SetIsWriteBlocked(QuicConnection* connection,
                                           bool write_blocked) {
  connection->write_blocked_ = write_blocked;
}

// static
bool QuicConnectionPeer::IsServer(QuicConnection* connection) {
  return connection->is_server_;
}

// static
void QuicConnectionPeer::SetIsServer(QuicConnection* connection,
                                     bool is_server) {
  connection->is_server_ = is_server;
  QuicFramerPeer::SetIsServer(&connection->framer_, is_server);
}

// static
void QuicConnectionPeer::SetSelfAddress(QuicConnection* connection,
                                        const IPEndPoint& self_address) {
  connection->self_address_ = self_address;
}

// static
void QuicConnectionPeer::SetPeerAddress(QuicConnection* connection,
                                        const IPEndPoint& peer_address) {
  connection->peer_address_ = peer_address;
}

// static
void QuicConnectionPeer::SwapCrypters(QuicConnection* connection,
                                      QuicFramer* framer) {
  framer->SwapCryptersForTest(&connection->framer_);
}

// static
QuicConnectionHelperInterface* QuicConnectionPeer::GetHelper(
    QuicConnection* connection) {
  return connection->helper_;
}

// static
QuicFramer* QuicConnectionPeer::GetFramer(QuicConnection* connection) {
  return &connection->framer_;
}

QuicFecGroup* QuicConnectionPeer::GetFecGroup(QuicConnection* connection,
                                              int fec_group) {
  connection->last_header_.fec_group = fec_group;
  return connection->GetFecGroup();
}

// static
QuicAlarm* QuicConnectionPeer::GetAckAlarm(QuicConnection* connection) {
  return connection->ack_alarm_.get();
}

// static
QuicAlarm* QuicConnectionPeer::GetRetransmissionAlarm(
    QuicConnection* connection) {
  return connection->retransmission_alarm_.get();
}

// static
QuicAlarm* QuicConnectionPeer::GetSendAlarm(QuicConnection* connection) {
  return connection->send_alarm_.get();
}

// static
QuicAlarm* QuicConnectionPeer::GetResumeWritesAlarm(
    QuicConnection* connection) {
  return connection->resume_writes_alarm_.get();
}

// static
QuicAlarm* QuicConnectionPeer::GetTimeoutAlarm(QuicConnection* connection) {
  return connection->timeout_alarm_.get();
}

// static
QuicPacketWriter* QuicConnectionPeer::GetWriter(QuicConnection* connection) {
  return connection->writer_;
}

// static
void QuicConnectionPeer::SetWriter(QuicConnection* connection,
                                   QuicTestWriter* writer) {
  connection->writer_ = writer;
}

}  // namespace test
}  // namespace net
