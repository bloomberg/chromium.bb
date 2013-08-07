// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_creator.h"

#include "base/logging.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_fec_group.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using std::make_pair;
using std::min;
using std::pair;
using std::vector;

namespace net {

QuicPacketCreator::QuicPacketCreator(QuicGuid guid,
                                     QuicFramer* framer,
                                     QuicRandom* random_generator,
                                     bool is_server)
    : guid_(guid),
      framer_(framer),
      random_generator_(random_generator),
      sequence_number_(0),
      fec_group_number_(0),
      is_server_(is_server),
      send_version_in_packet_(!is_server),
      packet_size_(GetPacketHeaderSize(options_.send_guid_length,
                                       send_version_in_packet_,
                                       options_.send_sequence_number_length,
                                       NOT_IN_FEC_GROUP)) {
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
  return fec_group_.get() != NULL && fec_group_->NumReceivedPackets() > 0 &&
      (force_close ||
       fec_group_->NumReceivedPackets() >= options_.max_packets_per_fec_group);
}

void QuicPacketCreator::MaybeStartFEC() {
  if (options_.max_packets_per_fec_group > 0 && fec_group_.get() == NULL) {
    DCHECK(queued_frames_.empty());
    // Set the fec group number to the sequence number of the next packet.
    fec_group_number_ = sequence_number() + 1;
    fec_group_.reset(new QuicFecGroup());
    packet_size_ = GetPacketHeaderSize(options_.send_guid_length,
                                       send_version_in_packet_,
                                       options_.send_sequence_number_length,
                                       IN_FEC_GROUP);
    DCHECK_LE(packet_size_, options_.max_packet_length);
  }
}

// Stops serializing version of the protocol in packets sent after this call.
// A packet that is already open might send kQuicVersionSize bytes less than the
// maximum packet size if we stop sending version before it is serialized.
void QuicPacketCreator::StopSendingVersion() {
  DCHECK(send_version_in_packet_);
  send_version_in_packet_ = false;
  if (packet_size_ > 0) {
    DCHECK_LT(kQuicVersionSize, packet_size_);
    packet_size_ -= kQuicVersionSize;
  }
}

bool QuicPacketCreator::HasRoomForStreamFrame(QuicStreamId id,
                                              QuicStreamOffset offset) const {
  return BytesFree() >
      QuicFramer::GetMinStreamFrameSize(framer_->version(), id, offset, true);
}

// static
size_t QuicPacketCreator::StreamFramePacketOverhead(
    QuicVersion version,
    QuicGuidLength guid_length,
    bool include_version,
    QuicSequenceNumberLength sequence_number_length,
    InFecGroup is_in_fec_group) {
  return GetPacketHeaderSize(guid_length, include_version,
                             sequence_number_length, is_in_fec_group) +
      // Assumes this is a stream with a single lone packet.
      QuicFramer::GetMinStreamFrameSize(version, 1u, 0u, true);
}

size_t QuicPacketCreator::CreateStreamFrame(QuicStreamId id,
                                            StringPiece data,
                                            QuicStreamOffset offset,
                                            bool fin,
                                            QuicFrame* frame) {
  DCHECK_GT(options_.max_packet_length,
            StreamFramePacketOverhead(
                framer_->version(), PACKET_8BYTE_GUID, kIncludeVersion,
                PACKET_6BYTE_SEQUENCE_NUMBER, IN_FEC_GROUP));
  DCHECK(HasRoomForStreamFrame(id, offset));

  const size_t free_bytes = BytesFree();
  size_t bytes_consumed = 0;

  if (data.size() != 0) {
    // When a STREAM frame is the last frame in a packet, it consumes two fewer
    // bytes of framing overhead.
    // Anytime more data is available than fits in with the extra two bytes,
    // the frame will be the last, and up to two extra bytes are consumed.
    // TODO(ianswett): If QUIC pads, the 1 byte PADDING frame does not fit when
    // 1 byte is available, because then the STREAM frame isn't the last.

    // The minimum frame size(0 bytes of data) if it's not the last frame.
    size_t min_frame_size = QuicFramer::GetMinStreamFrameSize(
        framer_->version(), id, offset, false);
    // Check if it's the last frame in the packet.
    if (data.size() + min_frame_size > free_bytes) {
      // The minimum frame size(0 bytes of data) if it is the last frame.
      size_t min_last_frame_size = QuicFramer::GetMinStreamFrameSize(
          framer_->version(), id, offset, true);
      bytes_consumed =
          min<size_t>(free_bytes - min_last_frame_size, data.size());
    } else {
      DCHECK_LT(data.size(), BytesFree());
      bytes_consumed = data.size();
    }

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

SerializedPacket QuicPacketCreator::SerializeAllFrames(
    const QuicFrames& frames) {
  // TODO(satyamshekhar): Verify that this DCHECK won't fail. What about queued
  // frames from SendStreamData()[send_stream_should_flush_ == false &&
  // data.empty() == true] and retransmit due to RTO.
  DCHECK_EQ(0u, queued_frames_.size());
  for (size_t i = 0; i < frames.size(); ++i) {
    bool success = AddFrame(frames[i], false);
    DCHECK(success);
  }
  SerializedPacket packet = SerializePacket();
  DCHECK(packet.retransmittable_frames == NULL);
  return packet;
}

bool QuicPacketCreator::HasPendingFrames() {
  return !queued_frames_.empty();
}

size_t QuicPacketCreator::BytesFree() const {
  const size_t max_plaintext_size =
      framer_->GetMaxPlaintextSize(options_.max_packet_length);
  if (packet_size_ > max_plaintext_size) {
    return 0;
  }
  return max_plaintext_size - packet_size_;
}

bool QuicPacketCreator::AddSavedFrame(const QuicFrame& frame) {
  return AddFrame(frame, true);
}

SerializedPacket QuicPacketCreator::SerializePacket() {
  DCHECK_EQ(false, queued_frames_.empty());
  QuicPacketHeader header;
  FillPacketHeader(fec_group_number_, false, false, &header);

  SerializedPacket serialized = framer_->BuildDataPacket(
      header, queued_frames_, packet_size_);
  queued_frames_.clear();
  packet_size_ = GetPacketHeaderSize(options_.send_guid_length,
                                     send_version_in_packet_,
                                     options_.send_sequence_number_length,
                                     fec_group_.get() != NULL ?
                                         IN_FEC_GROUP : NOT_IN_FEC_GROUP);
  serialized.retransmittable_frames = queued_retransmittable_frames_.release();
  return serialized;
}

SerializedPacket QuicPacketCreator::SerializeFec() {
  DCHECK_LT(0u, fec_group_->NumReceivedPackets());
  DCHECK_EQ(0u, queued_frames_.size());
  QuicPacketHeader header;
  FillPacketHeader(fec_group_number_, true,
                   fec_group_->entropy_parity(), &header);
  QuicFecData fec_data;
  fec_data.fec_group = fec_group_->min_protected_packet();
  fec_data.redundancy = fec_group_->payload_parity();
  SerializedPacket serialized = framer_->BuildFecPacket(header, fec_data);
  fec_group_.reset(NULL);
  fec_group_number_ = 0;
  // Reset packet_size_, since the next packet may not have an FEC group.
  packet_size_ = GetPacketHeaderSize(options_.send_guid_length,
                                     send_version_in_packet_,
                                     options_.send_sequence_number_length,
                                     NOT_IN_FEC_GROUP);
  DCHECK(serialized.packet);
  DCHECK_GE(options_.max_packet_length, serialized.packet->length());
  return serialized;
}

SerializedPacket QuicPacketCreator::SerializeConnectionClose(
    QuicConnectionCloseFrame* close_frame) {
  QuicFrames frames;
  frames.push_back(QuicFrame(close_frame));
  return SerializeAllFrames(frames);
}

QuicEncryptedPacket* QuicPacketCreator::SerializeVersionNegotiationPacket(
    const QuicVersionVector& supported_versions) {
  DCHECK(is_server_);
  QuicPacketPublicHeader header;
  header.guid = guid_;
  header.reset_flag = false;
  header.version_flag = true;
  header.versions = supported_versions;
  QuicEncryptedPacket* encrypted =
      framer_->BuildVersionNegotiationPacket(header, supported_versions);
  DCHECK(encrypted);
  DCHECK_GE(options_.max_packet_length, encrypted->length());
  return encrypted;
}

void QuicPacketCreator::FillPacketHeader(QuicFecGroupNumber fec_group,
                                         bool fec_flag,
                                         bool fec_entropy_flag,
                                         QuicPacketHeader* header) {
  header->public_header.guid = guid_;
  header->public_header.reset_flag = false;
  header->public_header.version_flag = send_version_in_packet_;
  header->fec_flag = fec_flag;
  header->packet_sequence_number = ++sequence_number_;

  bool entropy_flag;
  if (header->packet_sequence_number == 1) {
    DCHECK(!fec_flag);
    // TODO(satyamshekhar): No entropy in the first message.
    // For crypto tests to pass. Fix this by using deterministic QuicRandom.
    entropy_flag = 0;
  } else if (fec_flag) {
    // FEC packets don't have an entropy of their own. Entropy flag for FEC
    // packets is the XOR of entropy of previous packets.
    entropy_flag = fec_entropy_flag;
  } else {
    entropy_flag = random_generator_->RandBool();
  }
  header->entropy_flag = entropy_flag;
  header->is_in_fec_group = fec_group == 0 ? NOT_IN_FEC_GROUP : IN_FEC_GROUP;
  header->fec_group = fec_group;
}

bool QuicPacketCreator::ShouldRetransmit(const QuicFrame& frame) {
  return frame.type != ACK_FRAME && frame.type != CONGESTION_FEEDBACK_FRAME &&
      frame.type != PADDING_FRAME;
}

bool QuicPacketCreator::AddFrame(const QuicFrame& frame,
                                 bool save_retransmittable_frames) {
  size_t frame_len = framer_->GetSerializedFrameLength(
      frame, BytesFree(), queued_frames_.empty());
  if (frame_len == 0) {
    return false;
  }
  packet_size_ += frame_len;

  if (save_retransmittable_frames && ShouldRetransmit(frame)) {
    if (queued_retransmittable_frames_.get() == NULL) {
      queued_retransmittable_frames_.reset(new RetransmittableFrames());
    }
    if (frame.type == STREAM_FRAME) {
      queued_frames_.push_back(
          queued_retransmittable_frames_->AddStreamFrame(frame.stream_frame));
    } else {
      queued_frames_.push_back(
          queued_retransmittable_frames_->AddNonStreamFrame(frame));
    }
  } else {
    queued_frames_.push_back(frame);
  }
  return true;
}

}  // namespace net
