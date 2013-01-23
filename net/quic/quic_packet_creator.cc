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
      fec_group_number_(0) {
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
  DCHECK(options_.max_packets_per_fec_group == 0 ||
         (fec_group_.get() != NULL && fec_group_->GroupSize() > 0));
  return options_.max_packets_per_fec_group > 0 &&
      ((force_close && fec_group_->GroupSize() > 0) ||
       fec_group_->GroupSize() >= options_.max_packets_per_fec_group);
}

void QuicPacketCreator::MaybeStartFEC() {
  if (options_.max_packets_per_fec_group > 0) {
    DCHECK(fec_group_.get() == NULL);
    // Set the fec group number to the sequence number of the next packet.
    fec_group_number_ = sequence_number() + 1;
    fec_group_.reset(new QuicFecGroup());
  }
}

size_t QuicPacketCreator::CreateStreamFrame(QuicStreamId id,
                                            StringPiece data,
                                            QuicStreamOffset offset,
                                            bool fin,
                                            QuicFrames* frames) {
  DCHECK_GT(options_.max_packet_length,
            QuicUtils::StreamFramePacketOverhead(1));

  size_t unconsumed_bytes = data.size();
  if (data.size() != 0) {
    size_t max_frame_len = framer_->GetMaxPlaintextSize(
        options_.max_packet_length -
        QuicUtils::StreamFramePacketOverhead(1));
    DCHECK_GT(max_frame_len, 0u);
    size_t frame_len = min<size_t>(max_frame_len, unconsumed_bytes);

    if (unconsumed_bytes > 0) {
      bool set_fin = false;
      if (unconsumed_bytes <= frame_len) {  // last frame.
        frame_len = min(unconsumed_bytes, frame_len);
        set_fin = fin;
      }
      StringPiece data_frame(data.data() + data.size() - unconsumed_bytes,
                             frame_len);
      frames->push_back(QuicFrame(new QuicStreamFrame(
          id, set_fin, offset, data_frame)));

      unconsumed_bytes -= frame_len;
    }
    // If we haven't finished serializing all the data, don't set any final fin.
    if (unconsumed_bytes > 0) {
      fin = false;
    }
  }

  // Create a new packet for the fin, if necessary.
  if (fin && data.size() == 0) {
    QuicStreamFrame* frame = new QuicStreamFrame(id, true, offset, "");
    frames->push_back(QuicFrame(frame));
  }

  return data.size() - unconsumed_bytes;
}

PacketPair QuicPacketCreator::SerializeAllFrames(const QuicFrames& frames) {
  size_t num_serialized;
  PacketPair pair = SerializeFrames(frames, &num_serialized);
  DCHECK_EQ(frames.size(), num_serialized);
  return pair;
}

PacketPair QuicPacketCreator::SerializeFrames(const QuicFrames& frames,
                                              size_t* num_serialized) {
  QuicPacketHeader header;
  FillPacketHeader(fec_group_number_, PACKET_PRIVATE_FLAGS_NONE, &header);

  QuicPacket* packet = framer_->ConstructMaxFrameDataPacket(
      header, frames, num_serialized);
  return make_pair(header.packet_sequence_number, packet);
}

QuicPacketCreator::PacketPair QuicPacketCreator::SerializeFec() {
  DCHECK_LT(0u, fec_group_->GroupSize());
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

  QuicPacketHeader header;
  FillPacketHeader(0, PACKET_PRIVATE_FLAGS_NONE, &header);

  QuicFrames frames;
  frames.push_back(QuicFrame(close_frame));
  QuicPacket* packet = framer_->ConstructFrameDataPacket(header, frames);
  DCHECK(packet);
  return make_pair(header.packet_sequence_number, packet);
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

}  // namespace net
