// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_framer.h"

#include "base/hash_tables.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_data_writer.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using std::make_pair;
using std::map;
using std::numeric_limits;
using std::string;

namespace net {

namespace {

// Mask to select the lowest 48 bits of a sequence number.
const QuicPacketSequenceNumber kSequenceNumberMask =
    GG_UINT64_C(0x0000FFFFFFFFFFFF);

// Returns the absolute value of the difference between |a| and |b|.
QuicPacketSequenceNumber Delta(QuicPacketSequenceNumber a,
                               QuicPacketSequenceNumber b) {
  // Since these are unsigned numbers, we can't just return abs(a - b)
  if (a < b) {
    return b - a;
  }
  return a - b;
}

QuicPacketSequenceNumber ClosestTo(QuicPacketSequenceNumber target,
                                   QuicPacketSequenceNumber a,
                                   QuicPacketSequenceNumber b) {
  return (Delta(target, a) < Delta(target, b)) ? a : b;
}

}  // namespace

QuicFramer::QuicFramer(QuicVersionTag version,
                       QuicDecrypter* decrypter,
                       QuicEncrypter* encrypter)
    : visitor_(NULL),
      fec_builder_(NULL),
      error_(QUIC_NO_ERROR),
      last_sequence_number_(0),
      quic_version_(version),
      decrypter_(decrypter),
      encrypter_(encrypter) {
  DCHECK_EQ(kQuicVersion1, version);
}

QuicFramer::~QuicFramer() {}

bool CanTruncate(const QuicFrame& frame) {
  if (frame.type == ACK_FRAME ||
      frame.type == CONNECTION_CLOSE_FRAME) {
    return true;
  }
  return false;
}

// static
size_t QuicFramer::GetMinStreamFrameSize() {
  return kQuicFrameTypeSize + kQuicStreamIdSize +
      kQuicStreamFinSize + kQuicStreamOffsetSize + kQuicStreamPayloadLengthSize;
}

// static
size_t QuicFramer::GetMinAckFrameSize() {
  return kQuicFrameTypeSize + kQuicEntropyHashSize + kSequenceNumberSize +
      kQuicEntropyHashSize + kSequenceNumberSize + kNumberOfMissingPacketsSize;
}

// static
size_t QuicFramer::GetMinRstStreamFrameSize() {
  return kQuicFrameTypeSize + kQuicStreamIdSize + kQuicErrorCodeSize +
      kQuicErrorDetailsLengthSize;
}

// static
size_t QuicFramer::GetMinConnectionCloseFrameSize() {
  return kQuicFrameTypeSize + kQuicErrorCodeSize + kQuicErrorDetailsLengthSize +
      GetMinAckFrameSize();
}

// static
size_t QuicFramer::GetMinGoAwayFrameSize() {
  return kQuicFrameTypeSize + kQuicErrorCodeSize + kQuicErrorDetailsLengthSize +
      kQuicStreamIdSize;
}

size_t QuicFramer::GetSerializedFrameLength(
    const QuicFrame& frame, size_t free_bytes, bool first_frame) {
  if (frame.type == PADDING_FRAME) {
    // PADDING implies end of packet.
    return free_bytes;
  }
  size_t frame_len = ComputeFrameLength(frame);
  if (frame_len > free_bytes) {
    // Only truncate the first frame in a packet, so if subsequent ones go
    // over, stop including more frames.
    if (!first_frame) {
      return 0;
    }
    if (CanTruncate(frame)) {
      // Truncate the frame so the packet will not exceed kMaxPacketSize.
      // Note that we may not use every byte of the writer in this case.
      DLOG(INFO) << "Truncating large frame";
      return free_bytes;
    }
  }
  return frame_len;
}

QuicPacketEntropyHash QuicFramer::GetPacketEntropyHash(
    const QuicPacketHeader& header) const {
  if (!header.entropy_flag) {
    // TODO(satyamshekhar): Return some more better value here (something that
    // is not a constant).
    return 0;
  }
  return 1 << (header.packet_sequence_number % 8);
}

SerializedPacket QuicFramer::ConstructFrameDataPacket(
    const QuicPacketHeader& header,
    const QuicFrames& frames) {
  const size_t max_plaintext_size = GetMaxPlaintextSize(kMaxPacketSize);
  size_t packet_size = GetPacketHeaderSize(header.public_header.version_flag);
  for (size_t i = 0; i < frames.size(); ++i) {
    DCHECK_LE(packet_size, max_plaintext_size);
    const size_t frame_size = GetSerializedFrameLength(
        frames[i], max_plaintext_size - packet_size, i == 0);
    DCHECK(frame_size);
    packet_size += frame_size;
  }
  return ConstructFrameDataPacket(header, frames, packet_size);
}

SerializedPacket QuicFramer::ConstructFrameDataPacket(
    const QuicPacketHeader& header,
    const QuicFrames& frames,
    size_t packet_size) {
  QuicDataWriter writer(packet_size);
  SerializedPacket kNoPacket = SerializedPacket(0, NULL, 0, NULL);
  if (!WritePacketHeader(header, &writer)) {
    return kNoPacket;
  }

  for (size_t i = 0; i < frames.size(); ++i) {
    const QuicFrame& frame = frames[i];
    if (!writer.WriteUInt8(frame.type)) {
      return kNoPacket;
    }

    switch (frame.type) {
      case PADDING_FRAME:
        writer.WritePadding();
        break;
      case STREAM_FRAME:
        if (!AppendStreamFramePayload(*frame.stream_frame, &writer)) {
          return kNoPacket;
        }
        break;
      case ACK_FRAME:
        if (!AppendAckFramePayload(*frame.ack_frame, &writer)) {
          return kNoPacket;
        }
        break;
      case CONGESTION_FEEDBACK_FRAME:
        if (!AppendQuicCongestionFeedbackFramePayload(
                *frame.congestion_feedback_frame, &writer)) {
          return kNoPacket;
        }
        break;
      case RST_STREAM_FRAME:
        if (!AppendRstStreamFramePayload(*frame.rst_stream_frame, &writer)) {
          return kNoPacket;
        }
        break;
      case CONNECTION_CLOSE_FRAME:
        if (!AppendConnectionCloseFramePayload(
                *frame.connection_close_frame, &writer)) {
          return kNoPacket;
        }
        break;
      case GOAWAY_FRAME:
        if (!AppendGoAwayFramePayload(*frame.goaway_frame, &writer)) {
          return kNoPacket;
        }
        break;
      default:
        RaiseError(QUIC_INVALID_FRAME_DATA);
        return kNoPacket;
    }
  }

  // Save the length before writing, because take clears it.
  const size_t len = writer.length();
  // Less than or equal because truncated acks end up with max_plaintex_size
  // length, even though they're typically slightly shorter.
  DCHECK_LE(len, packet_size);
  QuicPacket* packet = QuicPacket::NewDataPacket(
      writer.take(), len, true, header.public_header.version_flag);

  if (fec_builder_) {
    fec_builder_->OnBuiltFecProtectedPayload(header,
                                             packet->FecProtectedData());
  }

  return SerializedPacket(header.packet_sequence_number, packet,
                          GetPacketEntropyHash(header), NULL);
}

SerializedPacket QuicFramer::ConstructFecPacket(const QuicPacketHeader& header,
                                                const QuicFecData& fec) {
  size_t len = GetPacketHeaderSize(header.public_header.version_flag);
  len += fec.redundancy.length();

  QuicDataWriter writer(len);
  SerializedPacket kNoPacket = SerializedPacket(0, NULL, 0, NULL);
  if (!WritePacketHeader(header, &writer)) {
    return kNoPacket;
  }

  if (!writer.WriteBytes(fec.redundancy.data(), fec.redundancy.length())) {
    return kNoPacket;
  }

  return SerializedPacket(header.packet_sequence_number,
                          QuicPacket::NewFecPacket(
                              writer.take(), len, true,
                              header.public_header.version_flag),
                          GetPacketEntropyHash(header), NULL);
}

// static
QuicEncryptedPacket* QuicFramer::ConstructPublicResetPacket(
    const QuicPublicResetPacket& packet) {
  DCHECK(packet.public_header.reset_flag);
  DCHECK(!packet.public_header.version_flag);
  size_t len = GetPublicResetPacketSize();
  QuicDataWriter writer(len);

  if (!writer.WriteUInt64(packet.public_header.guid)) {
    return NULL;
  }

  uint8 flags = static_cast<uint8>(PACKET_PUBLIC_FLAGS_RST);
  if (!writer.WriteUInt8(flags)) {
    return NULL;
  }

  if (!writer.WriteUInt64(packet.nonce_proof)) {
    return NULL;
  }

  if (!AppendPacketSequenceNumber(packet.rejected_sequence_number,
                                  &writer)) {
    return NULL;
  }

  return new QuicEncryptedPacket(writer.take(), len, true);
}

bool QuicFramer::ProcessPacket(const QuicEncryptedPacket& packet) {
  DCHECK(!reader_.get());
  reader_.reset(new QuicDataReader(packet.data(), packet.length()));

  // First parse the public header.
  QuicPacketPublicHeader public_header;
  if (!ProcessPublicHeader(&public_header)) {
    DLOG(WARNING) << "Unable to process public header.";
    reader_.reset(NULL);
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }
  // TODO(satyamshekhar): Handle version negotiation.
  if (public_header.version_flag && public_header.version != quic_version_) {
      return false;
  }

  bool rv;
  if (public_header.reset_flag) {
    rv = ProcessPublicResetPacket(public_header);
  } else {
    rv = ProcessDataPacket(public_header, packet);
  }

  reader_.reset(NULL);
  return rv;
}

bool QuicFramer::ProcessDataPacket(
    const QuicPacketPublicHeader& public_header,
    const QuicEncryptedPacket& packet) {
  visitor_->OnPacket();

  QuicPacketHeader header(public_header);
  if (!ProcessPacketHeader(&header, packet)) {
    DLOG(WARNING) << "Unable to process data packet header.";
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }

  if (!visitor_->OnPacketHeader(header)) {
    return true;
  }

  if (packet.length() > kMaxPacketSize) {
    DLOG(WARNING) << "Packet too large: " << packet.length();
    return RaiseError(QUIC_PACKET_TOO_LARGE);
  }

  // Handle the payload.
  if (!header.fec_flag) {
    if (header.fec_group != 0) {
      StringPiece payload = reader_->PeekRemainingPayload();
      visitor_->OnFecProtectedPayload(payload);
    }
    if (!ProcessFrameData()) {
      DCHECK_NE(QUIC_NO_ERROR, error_);  // ProcessFrameData sets the error.
      DLOG(WARNING) << "Unable to process frame data.";
      return false;
    }
  } else {
    QuicFecData fec_data;
    fec_data.fec_group = header.fec_group;
    fec_data.redundancy = reader_->ReadRemainingPayload();
    visitor_->OnFecData(fec_data);
  }

  visitor_->OnPacketComplete();
  return true;
}

bool QuicFramer::ProcessPublicResetPacket(
    const QuicPacketPublicHeader& public_header) {
  QuicPublicResetPacket packet(public_header);
  if (!reader_->ReadUInt64(&packet.nonce_proof)) {
    set_detailed_error("Unable to read nonce proof.");
    return false;
  }
  // TODO(satyamshekhar): validate nonce to protect against DoS.

  if (!reader_->ReadUInt48(&packet.rejected_sequence_number)) {
    set_detailed_error("Unable to read rejected sequence number.");
    return false;
  }
  visitor_->OnPublicResetPacket(packet);
  return true;
}

bool QuicFramer::ProcessRevivedPacket(QuicPacketHeader* header,
                                      StringPiece payload) {
  DCHECK(!reader_.get());

  visitor_->OnRevivedPacket();

  header->entropy_hash = GetPacketEntropyHash(*header);

  // TODO(satyamshekhar): Don't process if the visitor refuses the header.
  visitor_->OnPacketHeader(*header);

  if (payload.length() > kMaxPacketSize) {
    set_detailed_error("Revived packet too large.");
    return RaiseError(QUIC_PACKET_TOO_LARGE);
  }

  reader_.reset(new QuicDataReader(payload.data(), payload.length()));
  if (!ProcessFrameData()) {
    DCHECK_NE(QUIC_NO_ERROR, error_);  // ProcessFrameData sets the error.
    DLOG(WARNING) << "Unable to process frame data.";
    return false;
  }

  visitor_->OnPacketComplete();
  reader_.reset(NULL);
  return true;
}

bool QuicFramer::WritePacketHeader(const QuicPacketHeader& header,
                                   QuicDataWriter* writer) {
  if (!writer->WriteUInt64(header.public_header.guid)) {
    return false;
  }

  uint8 flags = 0;
  if (header.public_header.reset_flag) {
    flags |= PACKET_PUBLIC_FLAGS_RST;
  }
  if (header.public_header.version_flag) {
    flags |= PACKET_PUBLIC_FLAGS_VERSION;
  }
  if (!writer->WriteUInt8(flags)) {
    return false;
  }

  if (header.public_header.version_flag) {
    writer->WriteUInt32(quic_version_);
  }

  if (!AppendPacketSequenceNumber(header.packet_sequence_number, writer)) {
    return false;
  }

  flags = 0;
  if (header.fec_flag) {
    flags |= PACKET_PRIVATE_FLAGS_FEC;
  }
  if (header.entropy_flag) {
    flags |= PACKET_PRIVATE_FLAGS_ENTROPY;
  }
  if (header.fec_entropy_flag) {
    flags |= PACKET_PRIVATE_FLAGS_FEC_ENTROPY;
  }
  if (!writer->WriteUInt8(flags)) {
    return false;
  }

  // Offset from the current packet sequence number to the first fec
  // protected packet, or kNoFecOffset to signal no FEC protection.
  uint8 first_fec_protected_packet_offset = kNoFecOffset;

  // The FEC group number is the sequence number of the first fec
  // protected packet, or 0 if this packet is not protected.
  if (header.fec_group != 0) {
    DCHECK_GE(header.packet_sequence_number, header.fec_group);
    DCHECK_GT(255u, header.packet_sequence_number - header.fec_group);
    first_fec_protected_packet_offset =
        header.packet_sequence_number - header.fec_group;
  }
  if (!writer->WriteBytes(&first_fec_protected_packet_offset, 1)) {
    return false;
  }

  return true;
}

QuicPacketSequenceNumber QuicFramer::CalculatePacketSequenceNumberFromWire(
    QuicPacketSequenceNumber packet_sequence_number) const {
  // The new sequence number might have wrapped to the next epoch, or
  // it might have reverse wrapped to the previous epoch, or it might
  // remain in the same epoch.  Select the sequence number closest to the
  // previous sequence number.
  QuicPacketSequenceNumber epoch = last_sequence_number_ & ~kSequenceNumberMask;
  QuicPacketSequenceNumber prev_epoch = epoch - (GG_UINT64_C(1) << 48);
  QuicPacketSequenceNumber next_epoch = epoch + (GG_UINT64_C(1) << 48);

  return ClosestTo(last_sequence_number_,
                   epoch + packet_sequence_number,
                   ClosestTo(last_sequence_number_,
                             prev_epoch + packet_sequence_number,
                             next_epoch + packet_sequence_number));
}

bool QuicFramer::ProcessPublicHeader(QuicPacketPublicHeader* public_header) {
  if (!reader_->ReadUInt64(&public_header->guid)) {
    set_detailed_error("Unable to read GUID.");
    return false;
  }

  uint8 public_flags;
  if (!reader_->ReadBytes(&public_flags, 1)) {
    set_detailed_error("Unable to read public flags.");
    return false;
  }

  if (public_flags > PACKET_PUBLIC_FLAGS_MAX) {
    set_detailed_error("Illegal public flags value.");
    return false;
  }

  public_header->reset_flag = (public_flags & PACKET_PUBLIC_FLAGS_RST) != 0;
  public_header->version_flag =
      (public_flags & PACKET_PUBLIC_FLAGS_VERSION) != 0;

  if (public_header->version_flag &&
      !reader_->ReadUInt32(&public_header->version)) {
    set_detailed_error("Unable to read protocol version.");
    return false;
  }

  return true;
}

// static
bool QuicFramer::ReadGuidFromPacket(const QuicEncryptedPacket& packet,
                                    QuicGuid* guid) {
  QuicDataReader reader(packet.data(), packet.length());
  return reader.ReadUInt64(guid);
}

bool QuicFramer::ProcessPacketHeader(
    QuicPacketHeader* header,
    const QuicEncryptedPacket& packet) {
  if (!ProcessPacketSequenceNumber(&header->packet_sequence_number)) {
    set_detailed_error("Unable to read sequence number.");
    return false;
  }

  if (header->packet_sequence_number == 0u) {
    set_detailed_error("Packet sequence numbers cannot be 0.");
    return false;
  }

  if (!DecryptPayload(header->packet_sequence_number,
                      header->public_header.version_flag,
                      packet)) {
    DLOG(WARNING) << "Unable to decrypt payload.";
    return RaiseError(QUIC_DECRYPTION_FAILURE);
  }

  uint8 private_flags;
  if (!reader_->ReadBytes(&private_flags, 1)) {
    set_detailed_error("Unable to read private flags.");
    return false;
  }

  if (private_flags > PACKET_PRIVATE_FLAGS_MAX) {
    set_detailed_error("Illegal private flags value.");
    return false;
  }

  header->fec_flag = (private_flags & PACKET_PRIVATE_FLAGS_FEC) != 0;
  header->entropy_flag = (private_flags & PACKET_PRIVATE_FLAGS_ENTROPY) != 0;
  header->fec_entropy_flag =
      (private_flags & PACKET_PRIVATE_FLAGS_FEC_ENTROPY) != 0;

  uint8 first_fec_protected_packet_offset;
  if (!reader_->ReadBytes(&first_fec_protected_packet_offset, 1)) {
    set_detailed_error("Unable to read first fec protected packet offset.");
    return false;
  }
  header->fec_group = first_fec_protected_packet_offset == kNoFecOffset ? 0 :
      header->packet_sequence_number - first_fec_protected_packet_offset;

  header->entropy_hash = GetPacketEntropyHash(*header);
  // Set the last sequence number after we have decrypted the packet
  // so we are confident is not attacker controlled.
  last_sequence_number_ = header->packet_sequence_number;
  return true;
}

bool QuicFramer::ProcessPacketSequenceNumber(
    QuicPacketSequenceNumber* sequence_number) {
  QuicPacketSequenceNumber wire_sequence_number;
  if (!reader_->ReadUInt48(&wire_sequence_number)) {
    return false;
  }

  *sequence_number =
      CalculatePacketSequenceNumberFromWire(wire_sequence_number);
  return true;
}

bool QuicFramer::ProcessFrameData() {
  if (reader_->IsDoneReading()) {
    set_detailed_error("Unable to read frame type.");
    return RaiseError(QUIC_INVALID_FRAME_DATA);
  }
  while (!reader_->IsDoneReading()) {
    uint8 frame_type;
    if (!reader_->ReadBytes(&frame_type, 1)) {
      set_detailed_error("Unable to read frame type.");
      return RaiseError(QUIC_INVALID_FRAME_DATA);
    }
    switch (frame_type) {
      case PADDING_FRAME:
        // We're done with the packet
        return true;
      case STREAM_FRAME:
        if (!ProcessStreamFrame()) {
          return RaiseError(QUIC_INVALID_FRAME_DATA);
        }
        break;
      case ACK_FRAME: {
        QuicAckFrame frame;
        if (!ProcessAckFrame(&frame)) {
          return RaiseError(QUIC_INVALID_FRAME_DATA);
        }
        break;
      }
      case CONGESTION_FEEDBACK_FRAME: {
        QuicCongestionFeedbackFrame frame;
        if (!ProcessQuicCongestionFeedbackFrame(&frame)) {
          return RaiseError(QUIC_INVALID_FRAME_DATA);
        }
        break;
      }
      case RST_STREAM_FRAME:
        if (!ProcessRstStreamFrame()) {
          return RaiseError(QUIC_INVALID_RST_STREAM_DATA);
        }
        break;
      case CONNECTION_CLOSE_FRAME:
        if (!ProcessConnectionCloseFrame()) {
          return RaiseError(QUIC_INVALID_CONNECTION_CLOSE_DATA);
        }
        break;
      case GOAWAY_FRAME:
        if (!ProcessGoAwayFrame()) {
          return RaiseError(QUIC_INVALID_GOAWAY_DATA);
        }
        break;
      default:
        set_detailed_error("Illegal frame type.");
        DLOG(WARNING) << "Illegal frame type: "
                      << static_cast<int>(frame_type);
        return RaiseError(QUIC_INVALID_FRAME_DATA);
    }
  }

  return true;
}

bool QuicFramer::ProcessStreamFrame() {
  QuicStreamFrame frame;
  if (!reader_->ReadUInt32(&frame.stream_id)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }

  uint8 fin;
  if (!reader_->ReadBytes(&fin, 1)) {
    set_detailed_error("Unable to read fin.");
    return false;
  }
  if (fin > 1) {
    set_detailed_error("Invalid fin value.");
    return false;
  }
  frame.fin = (fin == 1);

  if (!reader_->ReadUInt64(&frame.offset)) {
    set_detailed_error("Unable to read offset.");
    return false;
  }

  if (!reader_->ReadStringPiece16(&frame.data)) {
    set_detailed_error("Unable to read frame data.");
    return false;
  }

  visitor_->OnStreamFrame(frame);
  return true;
}

bool QuicFramer::ProcessAckFrame(QuicAckFrame* frame) {
  if (!ProcessSentInfo(&frame->sent_info)) {
    return false;
  }
  if (!ProcessReceivedInfo(&frame->received_info)) {
    return false;
  }
  visitor_->OnAckFrame(*frame);
  return true;
}

bool QuicFramer::ProcessReceivedInfo(ReceivedPacketInfo* received_info) {
  if (!reader_->ReadBytes(&received_info->entropy_hash, 1)) {
    set_detailed_error("Unable to read entropy hash for received packets.");
    return false;
  }

  if (!ProcessPacketSequenceNumber(&received_info->largest_observed)) {
     set_detailed_error("Unable to read largest observed.");
     return false;
  }

  uint8 num_missing_packets;
  if (!reader_->ReadBytes(&num_missing_packets, 1)) {
    set_detailed_error("Unable to read num missing packets.");
    return false;
  }

  for (int i = 0; i < num_missing_packets; ++i) {
    QuicPacketSequenceNumber sequence_number;
    if (!ProcessPacketSequenceNumber(&sequence_number)) {
      set_detailed_error("Unable to read sequence number in missing packets.");
      return false;
    }
    received_info->missing_packets.insert(sequence_number);
  }

  return true;
}

bool QuicFramer::ProcessSentInfo(SentPacketInfo* sent_info) {
  if (!reader_->ReadBytes(&sent_info->entropy_hash, 1)) {
    set_detailed_error("Unable to read entropy hash for sent packets.");
    return false;
  }

  if (!ProcessPacketSequenceNumber(&sent_info->least_unacked)) {
    set_detailed_error("Unable to read least unacked.");
    return false;
  }

  return true;
}

bool QuicFramer::ProcessQuicCongestionFeedbackFrame(
    QuicCongestionFeedbackFrame* frame) {
  uint8 feedback_type;
  if (!reader_->ReadBytes(&feedback_type, 1)) {
    set_detailed_error("Unable to read congestion feedback type.");
    return false;
  }
  frame->type =
      static_cast<CongestionFeedbackType>(feedback_type);

  switch (frame->type) {
    case kInterArrival: {
      CongestionFeedbackMessageInterArrival* inter_arrival =
          &frame->inter_arrival;
      if (!reader_->ReadUInt16(
              &inter_arrival->accumulated_number_of_lost_packets)) {
        set_detailed_error(
            "Unable to read accumulated number of lost packets.");
        return false;
      }
      uint8 num_received_packets;
      if (!reader_->ReadBytes(&num_received_packets, 1)) {
        set_detailed_error("Unable to read num received packets.");
        return false;
      }

      if (num_received_packets > 0u) {
        uint64 smallest_received;
        if (!ProcessPacketSequenceNumber(&smallest_received)) {
          set_detailed_error("Unable to read smallest received.");
          return false;
        }

        uint64 time_received_us;
        if (!reader_->ReadUInt64(&time_received_us)) {
          set_detailed_error("Unable to read time received.");
          return false;
        }

        inter_arrival->received_packet_times.insert(
            make_pair(smallest_received,
                      QuicTime::FromMicroseconds(time_received_us)));

        for (int i = 0; i < num_received_packets - 1; ++i) {
          uint16 sequence_delta;
          if (!reader_->ReadUInt16(&sequence_delta)) {
            set_detailed_error(
                "Unable to read sequence delta in received packets.");
            return false;
          }

          int32 time_delta_us;
          if (!reader_->ReadBytes(&time_delta_us, sizeof(time_delta_us))) {
            set_detailed_error(
                "Unable to read time delta in received packets.");
            return false;
          }
          QuicPacketSequenceNumber packet = smallest_received + sequence_delta;
          inter_arrival->received_packet_times.insert(
              make_pair(packet, QuicTime::FromMicroseconds(time_received_us +
                                                           time_delta_us)));
        }
      }
      break;
    }
    case kFixRate: {
      uint32 bitrate = 0;
      if (!reader_->ReadUInt32(&bitrate)) {
        set_detailed_error("Unable to read bitrate.");
        return false;
      }
      frame->fix_rate.bitrate = QuicBandwidth::FromBytesPerSecond(bitrate);
      break;
    }
    case kTCP: {
      CongestionFeedbackMessageTCP* tcp = &frame->tcp;
      if (!reader_->ReadUInt16(&tcp->accumulated_number_of_lost_packets)) {
        set_detailed_error(
            "Unable to read accumulated number of lost packets.");
        return false;
      }
      uint16 receive_window = 0;
      if (!reader_->ReadUInt16(&receive_window)) {
        set_detailed_error("Unable to read receive window.");
        return false;
      }
      // Simple bit packing, don't send the 4 least significant bits.
      tcp->receive_window = static_cast<QuicByteCount>(receive_window) << 4;
      break;
    }
    default:
      set_detailed_error("Illegal congestion feedback type.");
      DLOG(WARNING) << "Illegal congestion feedback type: "
                    << frame->type;
      return RaiseError(QUIC_INVALID_FRAME_DATA);
  }

  visitor_->OnCongestionFeedbackFrame(*frame);
  return true;
}

bool QuicFramer::ProcessRstStreamFrame() {
  QuicRstStreamFrame frame;
  if (!reader_->ReadUInt32(&frame.stream_id)) {
    set_detailed_error("Unable to read stream_id.");
    return false;
  }

  uint32 error_code;
  if (!reader_->ReadUInt32(&error_code)) {
    set_detailed_error("Unable to read rst stream error code.");
    return false;
  }
  frame.error_code = static_cast<QuicErrorCode>(error_code);

  StringPiece error_details;
  if (!reader_->ReadStringPiece16(&error_details)) {
    set_detailed_error("Unable to read rst stream error details.");
    return false;
  }
  frame.error_details = error_details.as_string();

  visitor_->OnRstStreamFrame(frame);
  return true;
}

bool QuicFramer::ProcessConnectionCloseFrame() {
  QuicConnectionCloseFrame frame;

  uint32 error_code;
  if (!reader_->ReadUInt32(&error_code)) {
    set_detailed_error("Unable to read connection close error code.");
    return false;
  }
  frame.error_code = static_cast<QuicErrorCode>(error_code);

  StringPiece error_details;
  if (!reader_->ReadStringPiece16(&error_details)) {
    set_detailed_error("Unable to read connection close error details.");
    return false;
  }
  frame.error_details = error_details.as_string();

  if (!ProcessAckFrame(&frame.ack_frame)) {
    DLOG(WARNING) << "Unable to process ack frame.";
    return false;
  }

  visitor_->OnConnectionCloseFrame(frame);
  return true;
}

bool QuicFramer::ProcessGoAwayFrame() {
  QuicGoAwayFrame frame;

  uint32 error_code;
  if (!reader_->ReadUInt32(&error_code)) {
    set_detailed_error("Unable to read go away error code.");
    return false;
  }
  frame.error_code = static_cast<QuicErrorCode>(error_code);

  uint32 stream_id;
  if (!reader_->ReadUInt32(&stream_id)) {
    set_detailed_error("Unable to read last good stream id.");
    return false;
  }
  frame.last_good_stream_id = static_cast<QuicErrorCode>(stream_id);

  StringPiece reason_phrase;
  if (!reader_->ReadStringPiece16(&reason_phrase)) {
    set_detailed_error("Unable to read goaway reason.");
    return false;
  }
  frame.reason_phrase = reason_phrase.as_string();

  visitor_->OnGoAwayFrame(frame);
  return true;
}

StringPiece QuicFramer::GetAssociatedDataFromEncryptedPacket(
    const QuicEncryptedPacket& encrypted, bool includes_version) {
  return StringPiece(encrypted.data() + kStartOfHashData,
                     GetStartOfEncryptedData(includes_version)
                     - kStartOfHashData);
}

QuicEncryptedPacket* QuicFramer::EncryptPacket(
    QuicPacketSequenceNumber packet_sequence_number,
    const QuicPacket& packet) {
  scoped_ptr<QuicData> out(encrypter_->Encrypt(packet_sequence_number,
                                               packet.AssociatedData(),
                                               packet.Plaintext()));
  if (out.get() == NULL) {
    RaiseError(QUIC_ENCRYPTION_FAILURE);
    return NULL;
  }
  StringPiece header_data = packet.BeforePlaintext();
  size_t len =  header_data.length() + out->length();
  char* buffer = new char[len];
  // TODO(rch): eliminate this buffer copy by passing in a buffer to Encrypt().
  memcpy(buffer, header_data.data(), header_data.length());
  memcpy(buffer + header_data.length(), out->data(), out->length());
  return new QuicEncryptedPacket(buffer, len, true);
}

size_t QuicFramer::GetMaxPlaintextSize(size_t ciphertext_size) {
  return encrypter_->GetMaxPlaintextSize(ciphertext_size);
}

bool QuicFramer::DecryptPayload(QuicPacketSequenceNumber sequence_number,
                                bool version_flag,
                                const QuicEncryptedPacket& packet) {
  StringPiece encrypted;
  if (!reader_->ReadStringPiece(&encrypted, reader_->BytesRemaining())) {
    return false;
  }
  DCHECK(decrypter_.get() != NULL);
  decrypted_.reset(decrypter_->Decrypt(
      sequence_number,
      GetAssociatedDataFromEncryptedPacket(packet, version_flag),
      encrypted));
  if  (decrypted_.get() == NULL) {
    return false;
  }

  reader_.reset(new QuicDataReader(decrypted_->data(), decrypted_->length()));
  return true;
}

size_t QuicFramer::ComputeFrameLength(const QuicFrame& frame) {
  switch (frame.type) {
    case STREAM_FRAME:
      return GetMinStreamFrameSize() + frame.stream_frame->data.size();
    case ACK_FRAME: {
      const QuicAckFrame& ack = *frame.ack_frame;
      return GetMinAckFrameSize() +
          kSequenceNumberSize * ack.received_info.missing_packets.size();
    }
    case CONGESTION_FEEDBACK_FRAME: {
      size_t len = kQuicFrameTypeSize;
      const QuicCongestionFeedbackFrame& congestion_feedback =
          *frame.congestion_feedback_frame;
      len += 1;  // Congestion feedback type.

      switch (congestion_feedback.type) {
        case kInterArrival: {
          const CongestionFeedbackMessageInterArrival& inter_arrival =
              congestion_feedback.inter_arrival;
          len += 2;
          len += 1;  // Number received packets.
          if (inter_arrival.received_packet_times.size() > 0) {
            len += kSequenceNumberSize;  // Smallest received.
            len += 8;  // Time.
            // 2 bytes per sequence number delta plus 4 bytes per delta time.
            len += kSequenceNumberSize *
                (inter_arrival.received_packet_times.size() - 1);
          }
          break;
        }
        case kFixRate:
          len += 4;
          break;
        case kTCP:
          len += 4;
          break;
        default:
          set_detailed_error("Illegal feedback type.");
          DLOG(INFO) << "Illegal feedback type: " << congestion_feedback.type;
          break;
      }
      return len;
    }
    case RST_STREAM_FRAME:
      return GetMinRstStreamFrameSize() +
          frame.rst_stream_frame->error_details.size();
    case CONNECTION_CLOSE_FRAME: {
      const QuicAckFrame& ack = frame.connection_close_frame->ack_frame;
      return GetMinConnectionCloseFrameSize() +
          frame.connection_close_frame->error_details.size() +
          kSequenceNumberSize * ack.received_info.missing_packets.size();
    }
    case GOAWAY_FRAME:
      return GetMinGoAwayFrameSize() + frame.goaway_frame->reason_phrase.size();
    case PADDING_FRAME:
      DCHECK(false);
      return 0;
    case NUM_FRAME_TYPES:
      DCHECK(false);
      return 0;
  }
  // Not reachable, but some Chrome compilers can't figure that out.
  DCHECK(false);
  return 0;
}

// static
bool QuicFramer::AppendPacketSequenceNumber(
    QuicPacketSequenceNumber packet_sequence_number,
    QuicDataWriter* writer) {
  // Ensure the entire sequence number can be written.
  if (writer->capacity() - writer->length() < kSequenceNumberSize) {
    return false;
  }
  return writer->WriteUInt48(packet_sequence_number & kSequenceNumberMask);
}

bool QuicFramer::AppendStreamFramePayload(
    const QuicStreamFrame& frame,
    QuicDataWriter* writer) {
  if (!writer->WriteUInt32(frame.stream_id)) {
    return false;
  }
  if (!writer->WriteUInt8(frame.fin)) {
    return false;
  }
  if (!writer->WriteUInt64(frame.offset)) {
    return false;
  }
  if (!writer->WriteUInt16(frame.data.size())) {
    return false;
  }
  if (!writer->WriteBytes(frame.data.data(),
                           frame.data.size())) {
    return false;
  }
  return true;
}

QuicPacketSequenceNumber QuicFramer::CalculateLargestObserved(
    const SequenceNumberSet& missing_packets,
    SequenceNumberSet::const_iterator largest_written) {
  SequenceNumberSet::const_iterator it = largest_written;
  QuicPacketSequenceNumber previous_missing = *it;
  ++it;

  // See if the next thing is a gap in the missing packets: if it's a
  // non-missing packet we can return it.
  if (it != missing_packets.end() && previous_missing + 1 != *it) {
    return *it - 1;
  }

  // Otherwise return the largest missing packet, as indirectly observed.
  return *largest_written;
}

// TODO(ianswett): Use varints or another more compact approach for all deltas.
bool QuicFramer::AppendAckFramePayload(
    const QuicAckFrame& frame,
    QuicDataWriter* writer) {
  // TODO(satyamshekhar): Decide how often we really should send this
  // entropy_hash update.
  if (!writer->WriteUInt8(frame.sent_info.entropy_hash)) {
    return false;
  }

  if (!AppendPacketSequenceNumber(frame.sent_info.least_unacked, writer)) {
    return false;
  }

  size_t received_entropy_offset = writer->length();
  if (!writer->WriteUInt8(frame.received_info.entropy_hash)) {
    return false;
  }

  size_t largest_observed_offset = writer->length();
  if (!AppendPacketSequenceNumber(frame.received_info.largest_observed,
                                  writer)) {
    return false;
  }

  // We don't check for overflowing uint8 here, because we only can fit 192 acks
  // per packet, so if we overflow we will be truncated.
  uint8 num_missing_packets = frame.received_info.missing_packets.size();
  size_t num_missing_packets_offset = writer->length();
  if (!writer->WriteBytes(&num_missing_packets, 1)) {
    return false;
  }

  SequenceNumberSet::const_iterator it =
      frame.received_info.missing_packets.begin();
  int num_missing_packets_written = 0;
  for (; it != frame.received_info.missing_packets.end(); ++it) {
    if (!AppendPacketSequenceNumber(*it, writer)) {
      // We are truncating.
      QuicPacketSequenceNumber largest_observed =
          CalculateLargestObserved(frame.received_info.missing_packets, --it);
      // Overwrite entropy hash for received packets.
      writer->WriteUInt8ToOffset(
          entropy_calculator_->ReceivedEntropyHash(largest_observed),
          received_entropy_offset);
      // Overwrite largest_observed.
      writer->WriteUInt48ToOffset(largest_observed & kSequenceNumberMask,
                                  largest_observed_offset);
      writer->WriteUInt8ToOffset(num_missing_packets_written,
                                 num_missing_packets_offset);
      return true;
    }
    ++num_missing_packets_written;
    DCHECK_GE(numeric_limits<uint8>::max(), num_missing_packets_written);
  }

  return true;
}

bool QuicFramer::AppendQuicCongestionFeedbackFramePayload(
    const QuicCongestionFeedbackFrame& frame,
    QuicDataWriter* writer) {
  if (!writer->WriteBytes(&frame.type, 1)) {
    return false;
  }

  switch (frame.type) {
    case kInterArrival: {
      const CongestionFeedbackMessageInterArrival& inter_arrival =
          frame.inter_arrival;
      if (!writer->WriteUInt16(
              inter_arrival.accumulated_number_of_lost_packets)) {
        return false;
      }
      DCHECK_GE(numeric_limits<uint8>::max(),
                inter_arrival.received_packet_times.size());
      if (inter_arrival.received_packet_times.size() >
          numeric_limits<uint8>::max()) {
        return false;
      }
      // TODO(ianswett): Make num_received_packets a varint.
      uint8 num_received_packets =
          inter_arrival.received_packet_times.size();
      if (!writer->WriteBytes(&num_received_packets, 1)) {
        return false;
      }
      if (num_received_packets > 0) {
        TimeMap::const_iterator it =
            inter_arrival.received_packet_times.begin();

        QuicPacketSequenceNumber lowest_sequence = it->first;
        if (!AppendPacketSequenceNumber(lowest_sequence, writer)) {
          return false;
        }

        QuicTime lowest_time = it->second;
        // TODO(ianswett): Use time deltas from the connection's first received
        // packet.
        if (!writer->WriteUInt64(lowest_time.ToMicroseconds())) {
          return false;
        }

        for (++it; it != inter_arrival.received_packet_times.end(); ++it) {
          QuicPacketSequenceNumber sequence_delta = it->first - lowest_sequence;
          DCHECK_GE(numeric_limits<uint16>::max(), sequence_delta);
          if (sequence_delta > numeric_limits<uint16>::max()) {
            return false;
          }
          if (!writer->WriteUInt16(static_cast<uint16>(sequence_delta))) {
            return false;
          }

          int32 time_delta_us =
              it->second.Subtract(lowest_time).ToMicroseconds();
          if (!writer->WriteBytes(&time_delta_us, sizeof(time_delta_us))) {
            return false;
          }
        }
      }
      break;
    }
    case kFixRate: {
      const CongestionFeedbackMessageFixRate& fix_rate =
          frame.fix_rate;
      if (!writer->WriteUInt32(fix_rate.bitrate.ToBytesPerSecond())) {
        return false;
      }
      break;
    }
    case kTCP: {
      const CongestionFeedbackMessageTCP& tcp = frame.tcp;
      DCHECK_LE(tcp.receive_window, 1u << 20);
      // Simple bit packing, don't send the 4 least significant bits.
      uint16 receive_window = static_cast<uint16>(tcp.receive_window >> 4);
      if (!writer->WriteUInt16(tcp.accumulated_number_of_lost_packets)) {
        return false;
      }
      if (!writer->WriteUInt16(receive_window)) {
        return false;
      }
      break;
    }
    default:
      return false;
  }

  return true;
}

bool QuicFramer::AppendRstStreamFramePayload(
        const QuicRstStreamFrame& frame,
        QuicDataWriter* writer) {
  if (!writer->WriteUInt32(frame.stream_id)) {
    return false;
  }

  uint32 error_code = static_cast<uint32>(frame.error_code);
  if (!writer->WriteUInt32(error_code)) {
    return false;
  }

  if (!writer->WriteStringPiece16(frame.error_details)) {
    return false;
  }
  return true;
}

bool QuicFramer::AppendConnectionCloseFramePayload(
    const QuicConnectionCloseFrame& frame,
    QuicDataWriter* writer) {
  uint32 error_code = static_cast<uint32>(frame.error_code);
  if (!writer->WriteUInt32(error_code)) {
    return false;
  }
  if (!writer->WriteStringPiece16(frame.error_details)) {
    return false;
  }
  AppendAckFramePayload(frame.ack_frame, writer);
  return true;
}

bool QuicFramer::AppendGoAwayFramePayload(const QuicGoAwayFrame& frame,
                                          QuicDataWriter* writer) {
  uint32 error_code = static_cast<uint32>(frame.error_code);
  if (!writer->WriteUInt32(error_code)) {
    return false;
  }
  uint32 stream_id = static_cast<uint32>(frame.last_good_stream_id);
  if (!writer->WriteUInt32(stream_id)) {
    return false;
  }
  if (!writer->WriteStringPiece16(frame.reason_phrase)) {
    return false;
  }
  return true;
}

bool QuicFramer::RaiseError(QuicErrorCode error) {
  DLOG(INFO) << detailed_error_;
  set_error(error);
  visitor_->OnError(this);
  reader_.reset(NULL);
  return false;
}

}  // namespace net
