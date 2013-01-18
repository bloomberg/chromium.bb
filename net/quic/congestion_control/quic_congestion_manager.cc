// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/congestion_control/quic_congestion_manager.h"

#include "net/quic/congestion_control/quic_receipt_metrics_collector.h"
#include "net/quic/congestion_control/quic_send_scheduler.h"

namespace net {

QuicCongestionManager::QuicCongestionManager(
    const QuicClock* clock,
    CongestionFeedbackType type)
    : collector_(new QuicReceiptMetricsCollector(clock, type)),
      scheduler_(new QuicSendScheduler(clock, type)) {
}

QuicCongestionManager::~QuicCongestionManager() {
}

void QuicCongestionManager::SentPacket(QuicPacketSequenceNumber sequence_number,
                                       size_t bytes,
                                       bool is_retransmission) {
  scheduler_->SentPacket(sequence_number, bytes, is_retransmission);
}

void QuicCongestionManager::OnIncomingQuicCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame) {
  scheduler_->OnIncomingQuicCongestionFeedbackFrame(frame);
}

void QuicCongestionManager::OnIncomingAckFrame(const QuicAckFrame& frame) {
  scheduler_->OnIncomingAckFrame(frame);
}

QuicTime::Delta QuicCongestionManager::TimeUntilSend(bool is_retransmission) {
  return scheduler_->TimeUntilSend(is_retransmission);
}

bool QuicCongestionManager::GenerateCongestionFeedback(
    QuicCongestionFeedbackFrame* feedback) {
  return collector_->GenerateCongestionFeedback(feedback);
}

void QuicCongestionManager::RecordIncomingPacket(
    size_t bytes,
    QuicPacketSequenceNumber sequence_number,
    QuicTime timestamp,
    bool revived) {
  collector_->RecordIncomingPacket(bytes, sequence_number, timestamp, revived);
}

}  // namespace net
