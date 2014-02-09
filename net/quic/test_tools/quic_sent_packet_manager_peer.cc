// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_sent_packet_manager_peer.h"

#include "base/stl_util.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_sent_packet_manager.h"

namespace net {
namespace test {

// static
void QuicSentPacketManagerPeer::SetMaxTailLossProbes(
    QuicSentPacketManager* sent_packet_manager, size_t max_tail_loss_probes) {
  sent_packet_manager->max_tail_loss_probes_ = max_tail_loss_probes;
}

// static
void QuicSentPacketManagerPeer::SetSendAlgorithm(
    QuicSentPacketManager* sent_packet_manager,
    SendAlgorithmInterface* send_algorithm) {
  sent_packet_manager->send_algorithm_.reset(send_algorithm);
}

// static
size_t QuicSentPacketManagerPeer::GetNackCount(
    const QuicSentPacketManager* sent_packet_manager,
    QuicPacketSequenceNumber sequence_number) {
  return sent_packet_manager->packet_history_map_.find(
      sequence_number)->second->nack_count();
}

// static
size_t QuicSentPacketManagerPeer::GetPendingRetransmissionCount(
    const QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->pending_retransmissions_.size();
}

// static
bool QuicSentPacketManagerPeer::HasPendingPackets(
    const QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->HasPendingPackets();
}

// static
QuicTime QuicSentPacketManagerPeer::GetSentTime(
    const QuicSentPacketManager* sent_packet_manager,
    QuicPacketSequenceNumber sequence_number) {
  DCHECK(ContainsKey(sent_packet_manager->unacked_packets_, sequence_number));

  return sent_packet_manager->unacked_packets_
      .find(sequence_number)->second.sent_time;
}

// static
QuicTime::Delta QuicSentPacketManagerPeer::rtt(
    QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->rtt_sample_;
}

// static
bool QuicSentPacketManagerPeer::IsRetransmission(
    QuicSentPacketManager* sent_packet_manager,
    QuicPacketSequenceNumber sequence_number) {
  DCHECK(sent_packet_manager->HasRetransmittableFrames(sequence_number));
  return sent_packet_manager->HasRetransmittableFrames(sequence_number) &&
      sent_packet_manager->
          unacked_packets_[sequence_number].all_transmissions->size() > 1;
}

// static
void QuicSentPacketManagerPeer::MarkForRetransmission(
    QuicSentPacketManager* sent_packet_manager,
    QuicPacketSequenceNumber sequence_number,
    TransmissionType transmission_type) {
  sent_packet_manager->MarkForRetransmission(sequence_number,
                                             transmission_type);
}

// static
QuicTime::Delta QuicSentPacketManagerPeer::GetRetransmissionDelay(
    const QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->GetRetransmissionDelay();
}

// static
bool QuicSentPacketManagerPeer::HasUnackedCryptoPackets(
    const QuicSentPacketManager* sent_packet_manager) {
  return sent_packet_manager->pending_crypto_packet_count_ > 0;
}

}  // namespace test
}  // namespace net
