// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_generator.h"

#include "base/logging.h"
#include "net/quic/quic_fec_group.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;

namespace net {

QuicPacketGenerator::QuicPacketGenerator(DelegateInterface* delegate,
                                         QuicPacketCreator* creator)
    : delegate_(delegate),
      packet_creator_(creator),
      should_flush_(true),
      should_send_ack_(false),
      should_send_feedback_(false) {
}

QuicPacketGenerator::~QuicPacketGenerator() {
  for (QuicFrames::iterator it = queued_control_frames_.begin();
       it != queued_control_frames_.end(); ++it) {
    switch (it->type) {
      case PADDING_FRAME:
        delete it->padding_frame;
        break;
      case STREAM_FRAME:
        delete it->stream_frame;
        break;
      case ACK_FRAME:
        delete it->ack_frame;
        break;
      case CONGESTION_FEEDBACK_FRAME:
        delete it->congestion_feedback_frame;
        break;
      case RST_STREAM_FRAME:
        delete it->rst_stream_frame;
        break;
      case CONNECTION_CLOSE_FRAME:
        delete it->connection_close_frame;
        break;
      case GOAWAY_FRAME:
        delete it->goaway_frame;
        break;
      case NUM_FRAME_TYPES:
        DCHECK(false) << "Cannot delete type: " << it->type;
    }
  }
}

void QuicPacketGenerator::SetShouldSendAck(bool also_send_feedback) {
  should_send_ack_ = true;
  should_send_feedback_ = also_send_feedback;
  SendQueuedData();
}


void QuicPacketGenerator::AddControlFrame(const QuicFrame& frame) {
  queued_control_frames_.push_back(frame);
  SendQueuedData();
}

QuicConsumedData QuicPacketGenerator::ConsumeData(QuicStreamId id,
                                                  StringPiece data,
                                                  QuicStreamOffset offset,
                                                  bool fin) {
  SendQueuedData();

  size_t total_bytes_consumed = 0;
  bool fin_consumed = false;

  while (delegate_->CanWrite(NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA)) {
    // TODO(rch) figure out FEC.
    // packet_creator_.MaybeStartFEC();
    QuicFrame frame;
    size_t bytes_consumed = packet_creator_->CreateStreamFrame(
        id, data, offset + total_bytes_consumed, fin, &frame);
    bool success = packet_creator_->AddSavedFrame(frame);
    DCHECK(success);

    total_bytes_consumed += bytes_consumed;
    fin_consumed = fin && bytes_consumed == data.size();
    data.remove_prefix(bytes_consumed);
    DCHECK(data.empty() || packet_creator_->BytesFree() == 0u);

    // TODO(ianswett): Restore packet reordering.
    if (should_flush_ || !packet_creator_->HasRoomForStreamFrame()) {
      SerializeAndSendPacket();
    }

    if (data.empty()) {
      // We're done writing the data. Exit the loop.
      // We don't make this a precondition because we could have 0 bytes of data
      // if we're simply writing a fin.
      break;
    }
  }

  // Ensure the FEC group is closed at the end of this method unless other
  // writes are pending.
  if (should_flush_ && packet_creator_->ShouldSendFec(true)) {
    SerializedPacket serialized_fec = packet_creator_->SerializeFec();
    DCHECK(serialized_fec.packet);
    delegate_->OnSerializedPacket(serialized_fec);
  }

  DCHECK(!should_flush_ || !packet_creator_->HasPendingFrames());
  return QuicConsumedData(total_bytes_consumed, fin_consumed);
}

void QuicPacketGenerator::SendQueuedData() {
  while (HasPendingData() && delegate_->CanWrite(NOT_RETRANSMISSION,
             packet_creator_->HasPendingFrames() ?
                 HAS_RETRANSMITTABLE_DATA : NO_RETRANSMITTABLE_DATA)) {
    if (!AddNextPendingFrame()) {
      // Packet was full, so serialize and send it.
      SerializeAndSendPacket();
    }
  }

  if (should_flush_) {
    if (packet_creator_->HasPendingFrames()) {
      SerializeAndSendPacket();
    }

    // Ensure the FEC group is closed at the end of this method unless other
    // writes are pending.
    if (packet_creator_->ShouldSendFec(true)) {
      SerializedPacket serialized_fec = packet_creator_->SerializeFec();
      DCHECK(serialized_fec.packet);
      delegate_->OnSerializedPacket(serialized_fec);
    }
  }
}

void QuicPacketGenerator::StartBatchOperations() {
  should_flush_ = false;
}

void QuicPacketGenerator::FinishBatchOperations() {
  should_flush_ = true;
  SendQueuedData();
}

bool QuicPacketGenerator::HasQueuedData() const {
  return packet_creator_->HasPendingFrames() || HasPendingData();
}

bool QuicPacketGenerator::HasPendingData() const {
  return should_send_ack_ || should_send_feedback_ ||
      !queued_control_frames_.empty();
}

bool QuicPacketGenerator::AddNextPendingFrame() {
  if (should_send_ack_) {
    pending_ack_frame_.reset(delegate_->CreateAckFrame());
    if (!packet_creator_->AddSavedFrame(QuicFrame(pending_ack_frame_.get()))) {
      // packet was full
      return false;
    }
    should_send_ack_ = false;
    return true;
  }

  if (should_send_feedback_) {
    pending_feedback_frame_.reset(delegate_->CreateFeedbackFrame());
    if (!packet_creator_->AddSavedFrame(QuicFrame(
            pending_feedback_frame_.get()))) {
      // packet was full
      return false;
    }
    should_send_feedback_ = false;
    return true;
  }

  DCHECK(!queued_control_frames_.empty());
  if (!packet_creator_->AddSavedFrame(queued_control_frames_.back())) {
    // packet was full
    return false;
  }
  queued_control_frames_.pop_back();
  return true;
}

void QuicPacketGenerator::SerializeAndSendPacket() {
  packet_creator_->MaybeStartFEC();
  SerializedPacket serialized_packet = packet_creator_->SerializePacket();
  DCHECK(serialized_packet.packet);
  delegate_->OnSerializedPacket(serialized_packet);

  if (packet_creator_->ShouldSendFec(false)) {
    SerializedPacket serialized_fec = packet_creator_->SerializeFec();
    DCHECK(serialized_fec.packet);
    delegate_->OnSerializedPacket(serialized_fec);
  }
}

}  // namespace net
