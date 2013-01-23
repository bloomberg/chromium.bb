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

QuicFramer::QuicFramer(QuicDecrypter* decrypter, QuicEncrypter* encrypter)
    : visitor_(NULL),
      fec_builder_(NULL),
      error_(QUIC_NO_ERROR),
      last_sequence_number_(0),
      decrypter_(decrypter),
      encrypter_(encrypter) {
}

QuicFramer::~QuicFramer() {}

bool CanTruncate(const QuicFrame& frame) {
  if (frame.type == ACK_FRAME ||
      frame.type == CONNECTION_CLOSE_FRAME) {
    return true;
  }
  return false;
}

QuicPacket* QuicFramer::ConstructFrameDataPacket(
    const QuicPacketHeader& header,
    const QuicFrames& frames) {
  size_t num_consumed = 0;
  QuicPacket* packet =
      ConstructMaxFrameDataPacket(header, frames, &num_consumed);
  DCHECK_EQ(frames.size(), num_consumed);
  return packet;
}

QuicPacket* QuicFramer::ConstructMaxFrameDataPacket(
    const QuicPacketHeader& header,
    const QuicFrames& frames,
    size_t* num_consumed) {
  DCHECK(!frames.empty());
  // Compute the length of the packet.  We use "magic numbers" here because
  // sizeof(member_) is not necessarily the same as sizeof(member_wire_format).
  const size_t max_plaintext_size = GetMaxPlaintextSize(kMaxPacketSize);
  size_t len = kPacketHeaderSize;
  bool truncating = false;
  for (size_t i = 0; i < frames.size(); ++i) {
    if (frames[i].type == PADDING_FRAME) {
      // PADDING implies end of packet so make sure we don't have
      // more frames on the list.
      DCHECK_EQ(i, frames.size() - 1);
      len = max_plaintext_size;
      *num_consumed = i + 1;
      break;
    }
    size_t frame_len = 1;  // Space for the 8 bit type.
    frame_len += ComputeFramePayloadLength(frames[i]);
    if (len + frame_len > max_plaintext_size) {
      // Only truncate the first frame in a packet, so if subsequent ones go
      // over, stop including more frames.
      if (i > 0) {
        break;
      }
      if (CanTruncate(frames[0])) {
        // Truncate the frame so the packet will not exceed kMaxPacketSize.
        // Note that we may not use every byte of the writer in this case.
        len = max_plaintext_size;
        *num_consumed = 1;
        truncating = true;
        DLOG(INFO) << "Truncating large frame";
        break;
      } else {
        return NULL;
      }
    }
    len += frame_len;
    *num_consumed = i + 1;
  }
  if (truncating) {
    DCHECK_EQ(1u, *num_consumed);
  }

  QuicDataWriter writer(len);

  if (!WritePacketHeader(header, &writer)) {
    return NULL;
  }

  // frame count
  if (*num_consumed > 256u) {
    return NULL;
  }

  for (size_t i = 0; i < *num_consumed; ++i) {
    const QuicFrame& frame = frames[i];
    if (!writer.WriteUInt8(frame.type)) {
          return NULL;
    }

    switch (frame.type) {
      case PADDING_FRAME:
        writer.WritePadding();
        break;
      case STREAM_FRAME:
        if (!AppendStreamFramePayload(*frame.stream_frame, &writer)) {
          return NULL;
        }
        break;
      case ACK_FRAME:
        if (!AppendAckFramePayload(*frame.ack_frame, &writer)) {
          return NULL;
        }
        break;
      case CONGESTION_FEEDBACK_FRAME:
        if (!AppendQuicCongestionFeedbackFramePayload(
                *frame.congestion_feedback_frame, &writer)) {
          return NULL;
        }
        break;
      case RST_STREAM_FRAME:
        if (!AppendRstStreamFramePayload(*frame.rst_stream_frame, &writer)) {
          return NULL;
        }
        break;
      case CONNECTION_CLOSE_FRAME:
        if (!AppendConnectionCloseFramePayload(
            *frame.connection_close_frame, &writer)) {
          return NULL;
        }
        break;
      default:
        RaiseError(QUIC_INVALID_FRAME_DATA);
        return NULL;
    }
  }

  DCHECK(truncating || len == writer.length());
  // Save the length before writing, because take clears it.
  len = writer.length();
  QuicPacket* packet = QuicPacket::NewDataPacket(writer.take(), len, true);

  if (fec_builder_) {
    fec_builder_->OnBuiltFecProtectedPayload(header,
                                             packet->FecProtectedData());
  }

  return packet;
}

QuicPacket* QuicFramer::ConstructFecPacket(const QuicPacketHeader& header,
                                           const QuicFecData& fec) {
  size_t len = kPacketHeaderSize;
  len += fec.redundancy.length();

  QuicDataWriter writer(len);

  if (!WritePacketHeader(header, &writer)) {
    return NULL;
  }

  if (!writer.WriteBytes(fec.redundancy.data(), fec.redundancy.length())) {
    return NULL;
  }

  return QuicPacket::NewFecPacket(writer.take(), len, true);
}

// static
QuicEncryptedPacket* QuicFramer::ConstructPublicResetPacket(
    const QuicPublicResetPacket& packet) {
  DCHECK_EQ(PACKET_PUBLIC_FLAGS_RST,
            PACKET_PUBLIC_FLAGS_RST & packet.public_header.flags);
  size_t len = kPublicResetPacketSize;
  QuicDataWriter writer(len);

  if (!writer.WriteUInt64(packet.public_header.guid)) {
    return NULL;
  }

  uint8 flags = static_cast<uint8>(packet.public_header.flags);
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

// TODO(satyamshekhar): Framer doesn't need addresses. Get rid of them.
bool QuicFramer::ProcessPacket(const IPEndPoint& self_address,
                               const IPEndPoint& peer_address,
                               const QuicEncryptedPacket& packet) {
  DCHECK(!reader_.get());
  reader_.reset(new QuicDataReader(packet.data(), packet.length()));

  // First parse the public header.
  QuicPacketPublicHeader public_header;
  if (!ProcessPublicHeader(&public_header)) {
    DLOG(WARNING) << "Unable to process public header.";
    reader_.reset(NULL);
    return RaiseError(QUIC_INVALID_PACKET_HEADER);
  }

  bool rv;
  if (public_header.flags & PACKET_PUBLIC_FLAGS_RST) {
    rv = ProcessPublicResetPacket(public_header);
  } else {
    rv = ProcessDataPacket(public_header, self_address, peer_address, packet);
  }

  reader_.reset(NULL);
  return rv;
}

bool QuicFramer::ProcessDataPacket(
    const QuicPacketPublicHeader& public_header,
    const IPEndPoint& self_address,
    const IPEndPoint& peer_address,
    const QuicEncryptedPacket& packet) {
  visitor_->OnPacket(self_address, peer_address);

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
  if ((header.private_flags & PACKET_PRIVATE_FLAGS_FEC) == 0) {
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

bool QuicFramer::ProcessRevivedPacket(const QuicPacketHeader& header,
                                      StringPiece payload) {
  DCHECK(!reader_.get());

  visitor_->OnRevivedPacket();

  visitor_->OnPacketHeader(header);

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

  uint8 flags =static_cast<uint8>(header.public_header.flags);
  if (!writer->WriteUInt8(flags)) {
    return false;
  }

  if (!AppendPacketSequenceNumber(header.packet_sequence_number, writer)) {
    return false;
  }

  flags = static_cast<uint8>(header.private_flags);
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
  // The new sequence number might have wrapped to the next epoc, or
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

/* static */
bool QuicFramer::ReadGuidFromPacket(const QuicEncryptedPacket& packet,
                                    QuicGuid* guid) {
  QuicDataReader reader(packet.data(), packet.length());
  return reader.ReadUInt64(guid);
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

  public_header->flags = static_cast<QuicPacketPublicFlags>(public_flags);
  return true;
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

  if (!DecryptPayload(packet)) {
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

  header->private_flags = static_cast<QuicPacketPrivateFlags>(private_flags);

  uint8 first_fec_protected_packet_offset;
  if (!reader_->ReadBytes(&first_fec_protected_packet_offset, 1)) {
    set_detailed_error("Unable to read first fec protected packet offset.");
    return false;
  }
  header->fec_group = first_fec_protected_packet_offset == kNoFecOffset ? 0 :
      header->packet_sequence_number - first_fec_protected_packet_offset;

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
      CongestionFeedbackMessageFixRate* fix_rate = &frame->fix_rate;
      if (!reader_->ReadUInt32(&fix_rate->bitrate_in_bytes_per_second)) {
        set_detailed_error("Unable to read bitrate.");
        return false;
      }
      break;
    }
    case kTCP: {
      CongestionFeedbackMessageTCP* tcp = &frame->tcp;
      if (!reader_->ReadUInt16(&tcp->accumulated_number_of_lost_packets)) {
        set_detailed_error(
            "Unable to read accumulated number of lost packets.");
        return false;
      }
      if (!reader_->ReadUInt16(&tcp->receive_window)) {
        set_detailed_error("Unable to read receive window.");
        return false;
      }
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

  if (!reader_->ReadUInt64(&frame.offset)) {
    set_detailed_error("Unable to read offset in rst frame.");
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

QuicEncryptedPacket* QuicFramer::EncryptPacket(const QuicPacket& packet) {
  scoped_ptr<QuicData> out(encrypter_->Encrypt(packet.AssociatedData(),
                                               packet.Plaintext()));
  if (out.get() == NULL) {
    RaiseError(QUIC_ENCRYPTION_FAILURE);
    return NULL;
  }
  size_t len = kStartOfEncryptedData + out->length();
  char* buffer = new char[len];
  // TODO(rch): eliminate this buffer copy by passing in a buffer to Encrypt().
  memcpy(buffer, packet.data(), kStartOfEncryptedData);
  memcpy(buffer + kStartOfEncryptedData, out->data(), out->length());
  return new QuicEncryptedPacket(buffer, len, true);
}

size_t QuicFramer::GetMaxPlaintextSize(size_t ciphertext_size) {
  return encrypter_->GetMaxPlaintextSize(ciphertext_size);
}

bool QuicFramer::DecryptPayload(const QuicEncryptedPacket& packet) {
  StringPiece encrypted;
  if (!reader_->ReadStringPiece(&encrypted, reader_->BytesRemaining())) {
    return false;
  }
  DCHECK(decrypter_.get() != NULL);
  decrypted_.reset(decrypter_->Decrypt(packet.AssociatedData(), encrypted));
  if  (decrypted_.get() == NULL) {
    return false;
  }

  reader_.reset(new QuicDataReader(decrypted_->data(), decrypted_->length()));
  return true;
}

size_t QuicFramer::ComputeFramePayloadLength(const QuicFrame& frame) {
  size_t len = 0;
  // We use "magic numbers" here because sizeof(member_) is not necessarily the
  // same as sizeof(member_wire_format).
  switch (frame.type) {
    case STREAM_FRAME:
      len += 4;  // stream id
      len += 1;  // fin
      len += 8;  // offset
      len += 2;  // space for the 16 bit length
      len += frame.stream_frame->data.size();
      break;
    case ACK_FRAME: {
      const QuicAckFrame& ack = *frame.ack_frame;
      len += 6;  // largest observed packet sequence number
      len += 1;  // num missing packets
      len += 6 * ack.received_info.missing_packets.size();
      len += 6;  // least packet sequence number awaiting an ack
      break;
    }
    case CONGESTION_FEEDBACK_FRAME: {
      const QuicCongestionFeedbackFrame& congestion_feedback =
          *frame.congestion_feedback_frame;
      len += 1;  // congestion feedback type

      switch (congestion_feedback.type) {
        case kInterArrival: {
          const CongestionFeedbackMessageInterArrival& inter_arrival =
              congestion_feedback.inter_arrival;
          len += 2;
          len += 1;  // num received packets
          if (inter_arrival.received_packet_times.size() > 0) {
            len += 6;  // smallest received
            len += 8;  // time
            // 2 bytes per sequence number delta plus 4 bytes per delta time.
            len += 6 * (inter_arrival.received_packet_times.size() - 1);
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
      break;
    }
    case RST_STREAM_FRAME:
      len += 4;  // stream id
      len += 8;  // offset
      len += 4;  // error code
      len += 2;  // error details size
      len += frame.rst_stream_frame->error_details.size();
      break;
    case CONNECTION_CLOSE_FRAME:
      len += 4;  // error code
      len += 2;  // error details size
      len += frame.connection_close_frame->error_details.size();
      len += ComputeFramePayloadLength(
          QuicFrame(&frame.connection_close_frame->ack_frame));
      break;
    default:
      set_detailed_error("Illegal frame type.");
      DLOG(INFO) << "Illegal frame type: " << frame.type;
      break;
  }
  return len;
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
    const SequenceSet& missing_packets,
    SequenceSet::const_iterator largest_written) {
  SequenceSet::const_iterator it = largest_written;
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
  if (!AppendPacketSequenceNumber(frame.sent_info.least_unacked, writer)) {
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

  SequenceSet::const_iterator it = frame.received_info.missing_packets.begin();
  int num_missing_packets_written = 0;
  for (; it != frame.received_info.missing_packets.end(); ++it) {
    if (!AppendPacketSequenceNumber(*it, writer)) {
      // We are truncating.  Overwrite largest_observed.
      QuicPacketSequenceNumber largest_observed =
          CalculateLargestObserved(frame.received_info.missing_packets, --it);
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
      if (!writer->WriteUInt32(fix_rate.bitrate_in_bytes_per_second)) {
        return false;
      }
      break;
    }
    case kTCP: {
      const CongestionFeedbackMessageTCP& tcp = frame.tcp;
      if (!writer->WriteUInt16(tcp.accumulated_number_of_lost_packets)) {
        return false;
      }
      if (!writer->WriteUInt16(tcp.receive_window)) {
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
  if (!writer->WriteUInt64(frame.offset)) {
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

bool QuicFramer::RaiseError(QuicErrorCode error) {
  DLOG(INFO) << detailed_error_;
  set_error(error);
  visitor_->OnError(this);
  reader_.reset(NULL);
  return false;
}

}  // namespace net
