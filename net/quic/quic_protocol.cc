// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_protocol.h"

#include "base/stl_util.h"
#include "net/quic/quic_utils.h"

using base::StringPiece;
using std::map;
using std::numeric_limits;
using std::ostream;
using std::string;

namespace net {

size_t GetPacketHeaderSize(QuicPacketHeader header) {
  return GetPacketHeaderSize(header.public_header.guid_length,
                             header.public_header.version_flag,
                             header.public_header.sequence_number_length,
                             header.is_in_fec_group);
}

size_t GetPacketHeaderSize(QuicGuidLength guid_length,
                           bool include_version,
                           QuicSequenceNumberLength sequence_number_length,
                           InFecGroup is_in_fec_group) {
  return kPublicFlagsSize + guid_length +
      (include_version ? kQuicVersionSize : 0) + sequence_number_length +
      kPrivateFlagsSize + (is_in_fec_group == IN_FEC_GROUP ? kFecGroupSize : 0);
}

size_t GetPublicResetPacketSize() {
  return kPublicFlagsSize + PACKET_8BYTE_GUID + kPublicResetNonceSize +
      PACKET_6BYTE_SEQUENCE_NUMBER;
}

size_t GetStartOfFecProtectedData(
    QuicGuidLength guid_length,
    bool include_version,
    QuicSequenceNumberLength sequence_number_length) {
  return GetPacketHeaderSize(
      guid_length, include_version, sequence_number_length, IN_FEC_GROUP);
}

size_t GetStartOfEncryptedData(
    QuicGuidLength guid_length,
    bool include_version,
    QuicSequenceNumberLength sequence_number_length) {
  // Don't include the fec size, since encryption starts before private flags.
  return GetPacketHeaderSize(
      guid_length, include_version, sequence_number_length, NOT_IN_FEC_GROUP) -
      kPrivateFlagsSize;
}

QuicPacketPublicHeader::QuicPacketPublicHeader()
    : guid(0),
      guid_length(PACKET_8BYTE_GUID),
      reset_flag(false),
      version_flag(false),
      sequence_number_length(PACKET_6BYTE_SEQUENCE_NUMBER) {
}

QuicPacketPublicHeader::QuicPacketPublicHeader(
    const QuicPacketPublicHeader& other)
    : guid(other.guid),
      guid_length(other.guid_length),
      reset_flag(other.reset_flag),
      version_flag(other.version_flag),
      sequence_number_length(other.sequence_number_length),
      versions(other.versions) {
}

QuicPacketPublicHeader::~QuicPacketPublicHeader() {}

QuicPacketHeader::QuicPacketHeader()
    : fec_flag(false),
      entropy_flag(false),
      entropy_hash(0),
      packet_sequence_number(0),
      is_in_fec_group(NOT_IN_FEC_GROUP),
      fec_group(0) {
}

QuicPacketHeader::QuicPacketHeader(const QuicPacketPublicHeader& header)
    : public_header(header),
      fec_flag(false),
      entropy_flag(false),
      entropy_hash(0),
      packet_sequence_number(0),
      is_in_fec_group(NOT_IN_FEC_GROUP),
      fec_group(0) {
}

QuicStreamFrame::QuicStreamFrame() {}

QuicStreamFrame::QuicStreamFrame(const QuicStreamFrame& frame)
    : stream_id(frame.stream_id),
      fin(frame.fin),
      offset(frame.offset),
      data(frame.data),
      notifier(frame.notifier) {
}

QuicStreamFrame::QuicStreamFrame(QuicStreamId stream_id,
                                 bool fin,
                                 QuicStreamOffset offset,
                                 IOVector data)
    : stream_id(stream_id),
      fin(fin),
      offset(offset),
      data(data),
      notifier(NULL) {
}

string* QuicStreamFrame::GetDataAsString() const {
  string* data_string = new string();
  data_string->reserve(data.TotalBufferSize());
  for (size_t i = 0; i < data.Size(); ++i) {
    data_string->append(static_cast<char*>(data.iovec()[i].iov_base),
                        data.iovec()[i].iov_len);
  }
  DCHECK_EQ(data_string->size(), data.TotalBufferSize());
  return data_string;
}

uint32 MakeQuicTag(char a, char b, char c, char d) {
  return static_cast<uint32>(a) |
         static_cast<uint32>(b) << 8 |
         static_cast<uint32>(c) << 16 |
         static_cast<uint32>(d) << 24;
}

QuicVersionVector QuicSupportedVersions() {
  QuicVersionVector supported_versions;
  for (size_t i = 0; i < arraysize(kSupportedQuicVersions); ++i) {
    supported_versions.push_back(kSupportedQuicVersions[i]);
  }
  return supported_versions;
}

QuicTag QuicVersionToQuicTag(const QuicVersion version) {
  switch (version) {
    case QUIC_VERSION_12:
      return MakeQuicTag('Q', '0', '1', '2');
    default:
      // This shold be an ERROR because we should never attempt to convert an
      // invalid QuicVersion to be written to the wire.
      LOG(ERROR) << "Unsupported QuicVersion: " << version;
      return 0;
  }
}

QuicVersion QuicTagToQuicVersion(const QuicTag version_tag) {
  for (size_t i = 0; i < arraysize(kSupportedQuicVersions); ++i) {
    if (version_tag == QuicVersionToQuicTag(kSupportedQuicVersions[i])) {
      return kSupportedQuicVersions[i];
    }
  }
  // Reading from the client so this should not be considered an ERROR.
  DVLOG(1) << "Unsupported QuicTag version: "
             << QuicUtils::TagToString(version_tag);
  return QUIC_VERSION_UNSUPPORTED;
}

#define RETURN_STRING_LITERAL(x) \
case x: \
return #x

string QuicVersionToString(const QuicVersion version) {
  switch (version) {
    RETURN_STRING_LITERAL(QUIC_VERSION_12);
    default:
      return "QUIC_VERSION_UNSUPPORTED";
  }
}

string QuicVersionVectorToString(const QuicVersionVector& versions) {
  string result = "";
  for (size_t i = 0; i < versions.size(); ++i) {
    if (i != 0) {
      result.append(",");
    }
    result.append(QuicVersionToString(versions[i]));
  }
  return result;
}

ostream& operator<<(ostream& os, const QuicPacketHeader& header) {
  os << "{ guid: " << header.public_header.guid
     << ", guid_length:" << header.public_header.guid_length
     << ", sequence_number_length:"
     << header.public_header.sequence_number_length
     << ", reset_flag: " << header.public_header.reset_flag
     << ", version_flag: " << header.public_header.version_flag;
  if (header.public_header.version_flag) {
    os << " version: ";
    for (size_t i = 0; i < header.public_header.versions.size(); ++i) {
      os << header.public_header.versions[0] << " ";
    }
  }
  os << ", fec_flag: " << header.fec_flag
     << ", entropy_flag: " << header.entropy_flag
     << ", entropy hash: " << static_cast<int>(header.entropy_hash)
     << ", sequence_number: " << header.packet_sequence_number
     << ", is_in_fec_group:" << header.is_in_fec_group
     << ", fec_group: " << header.fec_group<< "}\n";
  return os;
}

ReceivedPacketInfo::ReceivedPacketInfo()
    : largest_observed(0),
      delta_time_largest_observed(QuicTime::Delta::Infinite()),
      is_truncated(false) {
}

ReceivedPacketInfo::~ReceivedPacketInfo() {}

bool IsAwaitingPacket(const ReceivedPacketInfo& received_info,
                      QuicPacketSequenceNumber sequence_number) {
  return sequence_number > received_info.largest_observed ||
      ContainsKey(received_info.missing_packets, sequence_number);
}

void InsertMissingPacketsBetween(ReceivedPacketInfo* received_info,
                                 QuicPacketSequenceNumber lower,
                                 QuicPacketSequenceNumber higher) {
  for (QuicPacketSequenceNumber i = lower; i < higher; ++i) {
    received_info->missing_packets.insert(i);
  }
}

SentPacketInfo::SentPacketInfo() {}

SentPacketInfo::~SentPacketInfo() {}

// Testing convenience method.
QuicAckFrame::QuicAckFrame(QuicPacketSequenceNumber largest_observed,
                           QuicTime largest_observed_receive_time,
                           QuicPacketSequenceNumber least_unacked) {
  received_info.largest_observed = largest_observed;
  received_info.entropy_hash = 0;
  sent_info.least_unacked = least_unacked;
  sent_info.entropy_hash = 0;
}

ostream& operator<<(ostream& os, const SentPacketInfo& sent_info) {
  os << "entropy_hash: " << static_cast<int>(sent_info.entropy_hash)
     << " least_unacked: " << sent_info.least_unacked;
  return os;
}

ostream& operator<<(ostream& os, const ReceivedPacketInfo& received_info) {
  os << "entropy_hash: " << static_cast<int>(received_info.entropy_hash)
     << " largest_observed: " << received_info.largest_observed
     << " missing_packets: [ ";
  for (SequenceNumberSet::const_iterator it =
           received_info.missing_packets.begin();
       it != received_info.missing_packets.end(); ++it) {
    os << *it << " ";
  }
  os << " ] ";
  return os;
}

QuicCongestionFeedbackFrame::QuicCongestionFeedbackFrame() {
}

QuicCongestionFeedbackFrame::~QuicCongestionFeedbackFrame() {
}

ostream& operator<<(ostream& os,
                    const QuicCongestionFeedbackFrame& congestion_frame) {
  os << "type: " << congestion_frame.type;
  switch (congestion_frame.type) {
    case kInterArrival: {
      const CongestionFeedbackMessageInterArrival& inter_arrival =
          congestion_frame.inter_arrival;
      os << " accumulated_number_of_lost_packets: "
         << inter_arrival.accumulated_number_of_lost_packets;
      os << " received packets: [ ";
      for (TimeMap::const_iterator it =
               inter_arrival.received_packet_times.begin();
           it != inter_arrival.received_packet_times.end(); ++it) {
        os << it->first << "@" << it->second.ToDebuggingValue() << " ";
      }
      os << "]";
      break;
    }
    case kFixRate: {
      os << " bitrate_in_bytes_per_second: "
         << congestion_frame.fix_rate.bitrate.ToBytesPerSecond();
      break;
    }
    case kTCP: {
      const CongestionFeedbackMessageTCP& tcp = congestion_frame.tcp;
      os << " accumulated_number_of_lost_packets: "
         << congestion_frame.tcp.accumulated_number_of_lost_packets;
      os << " receive_window: " << tcp.receive_window;
      break;
    }
  }
  return os;
}

ostream& operator<<(ostream& os, const QuicAckFrame& ack_frame) {
  os << "sent info { " << ack_frame.sent_info << " } "
     << "received info { " << ack_frame.received_info << " }\n";
  return os;
}

CongestionFeedbackMessageFixRate::CongestionFeedbackMessageFixRate()
    : bitrate(QuicBandwidth::Zero()) {
}

CongestionFeedbackMessageInterArrival::
CongestionFeedbackMessageInterArrival() {}

CongestionFeedbackMessageInterArrival::
~CongestionFeedbackMessageInterArrival() {}

QuicGoAwayFrame::QuicGoAwayFrame(QuicErrorCode error_code,
                                 QuicStreamId last_good_stream_id,
                                 const string& reason)
    : error_code(error_code),
      last_good_stream_id(last_good_stream_id),
      reason_phrase(reason) {
  DCHECK_LE(error_code, numeric_limits<uint8>::max());
}

QuicFecData::QuicFecData() {}

QuicData::~QuicData() {
  if (owns_buffer_) {
    delete [] const_cast<char*>(buffer_);
  }
}

StringPiece QuicPacket::FecProtectedData() const {
  const size_t start_of_fec = GetStartOfFecProtectedData(
      guid_length_, includes_version_, sequence_number_length_);
  return StringPiece(data() + start_of_fec, length() - start_of_fec);
}

StringPiece QuicPacket::AssociatedData() const {
  return StringPiece(
      data() + kStartOfHashData,
      GetStartOfEncryptedData(
          guid_length_, includes_version_, sequence_number_length_) -
      kStartOfHashData);
}

StringPiece QuicPacket::BeforePlaintext() const {
  return StringPiece(data(), GetStartOfEncryptedData(guid_length_,
                                                     includes_version_,
                                                     sequence_number_length_));
}

StringPiece QuicPacket::Plaintext() const {
  const size_t start_of_encrypted_data =
      GetStartOfEncryptedData(
          guid_length_, includes_version_, sequence_number_length_);
  return StringPiece(data() + start_of_encrypted_data,
                     length() - start_of_encrypted_data);
}

RetransmittableFrames::RetransmittableFrames()
    : encryption_level_(NUM_ENCRYPTION_LEVELS) {
}

RetransmittableFrames::~RetransmittableFrames() {
  for (QuicFrames::iterator it = frames_.begin(); it != frames_.end(); ++it) {
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
  STLDeleteElements(&stream_data_);
}

const QuicFrame& RetransmittableFrames::AddStreamFrame(
    QuicStreamFrame* stream_frame) {
  // Make an owned copy of the stream frame's data.
  stream_data_.push_back(stream_frame->GetDataAsString());
  // Ensure the stream frame's IOVector points to the owned copy of the data.
  stream_frame->data.Clear();
  stream_frame->data.Append(const_cast<char*>(stream_data_.back()->data()),
                            stream_data_.back()->size());
  frames_.push_back(QuicFrame(stream_frame));
  return frames_.back();
}

const QuicFrame& RetransmittableFrames::AddNonStreamFrame(
    const QuicFrame& frame) {
  DCHECK_NE(frame.type, STREAM_FRAME);
  frames_.push_back(frame);
  return frames_.back();
}

void RetransmittableFrames::set_encryption_level(EncryptionLevel level) {
  encryption_level_ = level;
}

SerializedPacket::SerializedPacket(
    QuicPacketSequenceNumber sequence_number,
    QuicSequenceNumberLength sequence_number_length,
    QuicPacket* packet,
    QuicPacketEntropyHash entropy_hash,
    RetransmittableFrames* retransmittable_frames)
    : sequence_number(sequence_number),
      sequence_number_length(sequence_number_length),
      packet(packet),
      entropy_hash(entropy_hash),
      retransmittable_frames(retransmittable_frames) {
}

SerializedPacket::~SerializedPacket() {}

QuicEncryptedPacket* QuicEncryptedPacket::Clone() const {
  char* buffer = new char[this->length()];
  memcpy(buffer, this->data(), this->length());
  return new QuicEncryptedPacket(buffer, this->length(), true);
}

ostream& operator<<(ostream& os, const QuicEncryptedPacket& s) {
  os << s.length() << "-byte data";
  return os;
}

ostream& operator<<(ostream& os, const QuicConsumedData& s) {
  os << "bytes_consumed: " << s.bytes_consumed
     << " fin_consumed: " << s.fin_consumed;
  return os;
}

}  // namespace net
