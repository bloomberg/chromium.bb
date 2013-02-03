// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_creator.h"

#include "base/logging.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using std::make_pair;
using std::min;
using std::pair;
using std::vector;

namespace net {

QuicPacketCreator::QuicPacketCreator(QuicGuid guid, QuicFramer* framer)
    : guid_(guid),
      framer_(framer),
      sequence_number_(0),
      fec_group_number_(0),
      packet_size_(kPacketHeaderSize) {
  framer_->set_fec_builder(this);
}

QuicPacketCreator::~QuicPacketCreator() {
}

void QuicPacketCreator::OnBuiltFecProtectedPayload(
    const QuicPacketHeader& header, StringPiece payload) {
  if (fec_group_.get()) {
    fec_group_->Update(header, payload);
  }
}

bool QuicPacketCreator::ShouldSendFec(bool force_close) const {
  return fec_group_.get() != NULL &&
      (force_close ||
       fec_group_->NumReceivedPackets() >= options_.max_packets_per_fec_group);
}

void QuicPacketCreator::MaybeStartFEC() {
  if (options_.max_packets_per_fec_group > 0 && fec_group_.get() == NULL) {
    // Set the fec group number to the sequence number of the next packet.
    fec_group_number_ = sequence_number() + 1;
    fec_group_.reset(new QuicFecGroup());
  }
}

size_t QuicPacketCreator::CreateStreamFrame(QuicStreamId id,
                                            StringPiece data,
                                            QuicStreamOffset offset,
                                            bool fin,
                                            QuicFrame* frame) {
  DCHECK_GT(options_.max_packet_length,
            QuicUtils::StreamFramePacketOverhead(1));
  const size_t free_bytes = BytesFree();
  DCHECK_GE(free_bytes, kFrameTypeSize + kMinStreamFrameLength);

  size_t bytes_consumed = 0;
  if (data.size() != 0) {
    size_t max_data_len = free_bytes - kFrameTypeSize - kMinStreamFrameLength;
    bytes_consumed = min<size_t>(max_data_len, data.size());

    bool set_fin = fin && bytes_consumed == data.size();  // Last frame.
    StringPiece data_frame(data.data(), bytes_consumed);
    *frame = QuicFrame(new QuicStreamFrame(id, set_fin, offset, data_frame));
  } else {
    DCHECK(fin);
    // Create a new packet for the fin, if necessary.
    *frame = QuicFrame(new QuicStreamFrame(id, true, offset, ""));
  }

  return bytes_consumed;
}

PacketPair QuicPacketCreator::SerializeAllFrames(const QuicFrames& frames) {
  DCHECK_EQ(0u, queued_frames_.size());
  for (size_t i = 0; i < frames.size(); ++i) {
    bool success = AddFrame(frames[i]);
    DCHECK(success);
  }
  return SerializePacket(NULL);
}

bool QuicPacketCreator::HasPendingFrames() {
  return !queued_frames_.empty();
}

size_t QuicPacketCreator::BytesFree() {
  const size_t max_plaintext_size =
      framer_->GetMaxPlaintextSize(options_.max_packet_length);
  if (packet_size_ > max_plaintext_size) {
    return 0;
  } else {
    return max_plaintext_size - packet_size_;
  }
}

bool QuicPacketCreator::AddFrame(const QuicFrame& frame) {
  size_t frame_len = framer_->GetSerializedFrameLength(
      frame, BytesFree(), queued_frames_.empty());
  if (frame_len == 0) {
    return false;
  }
  packet_size_ += frame_len;
  queued_frames_.push_back(frame);
  return true;
}

PacketPair QuicPacketCreator::SerializePacket(
    QuicFrames* retransmittable_frames) {
  DCHECK_EQ(false, queued_frames_.empty());
  QuicPacketHeader header;
  FillPacketHeader(fec_group_number_, PACKET_PRIVATE_FLAGS_NONE, &header);

  QuicPacket* packet = framer_->ConstructFrameDataPacket(
      header, queued_frames_, packet_size_);
  for (size_t i = 0; i < queued_frames_.size(); ++i) {
    if (retransmittable_frames != NULL && ShouldRetransmit(queued_frames_[i])) {
      retransmittable_frames->push_back(queued_frames_[i]);
    }
  }
  queued_frames_.clear();
  packet_size_ = kPacketHeaderSize;
  return make_pair(header.packet_sequence_number, packet);
}

QuicPacketCreator::PacketPair QuicPacketCreator::SerializeFec() {
  DCHECK_LT(0u, fec_group_->NumReceivedPackets());
  DCHECK_EQ(0u, queued_frames_.size());
  QuicPacketHeader header;
  FillPacketHeader(fec_group_number_, PACKET_PRIVATE_FLAGS_FEC, &header);

  QuicFecData fec_data;
  fec_data.fec_group = fec_group_->min_protected_packet();
  fec_data.redundancy = fec_group_->parity();
  QuicPacket* packet = framer_->ConstructFecPacket(header, fec_data);
  fec_group_.reset(NULL);
  fec_group_number_ = 0;
  DCHECK(packet);
  DCHECK_GE(options_.max_packet_length, packet->length());
  return make_pair(header.packet_sequence_number, packet);
}

QuicPacketCreator::PacketPair QuicPacketCreator::CloseConnection(
    QuicConnectionCloseFrame* close_frame) {
  QuicFrames frames;
  frames.push_back(QuicFrame(close_frame));
  return SerializeAllFrames(frames);
}

void QuicPacketCreator::FillPacketHeader(QuicFecGroupNumber fec_group,
                                         QuicPacketPrivateFlags flags,
                                         QuicPacketHeader* header) {
  header->public_header.guid = guid_;
  header->public_header.flags = PACKET_PUBLIC_FLAGS_NONE;
  header->private_flags = flags;
  header->packet_sequence_number = ++sequence_number_;
  header->fec_group = fec_group;
}

bool QuicPacketCreator::ShouldRetransmit(const QuicFrame& frame) {
  return frame.type != ACK_FRAME && frame.type != CONGESTION_FEEDBACK_FRAME &&
      frame.type != PADDING_FRAME;
}

}  // namespace net
