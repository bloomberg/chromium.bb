// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_protocol.h"
#include "base/stl_util.h"

using base::StringPiece;
using std::map;
using std::numeric_limits;
using std::ostream;
using std::string;

namespace net {

size_t GetPacketHeaderSize(bool include_version) {
  return kQuicGuidSize + kPublicFlagsSize +
      (include_version ? kQuicVersionSize : 0) + kSequenceNumberSize +
      kPrivateFlagsSize + kFecGroupSize;
}

size_t GetPublicResetPacketSize() {
  return kQuicGuidSize + kPublicFlagsSize + kPublicResetNonceSize +
      kSequenceNumberSize;
}

size_t GetStartOfFecProtectedData(bool include_version) {
  return GetPacketHeaderSize(include_version);
}

size_t GetStartOfEncryptedData(bool include_version) {
  return GetPacketHeaderSize(include_version) - kPrivateFlagsSize -
      kFecGroupSize;
}

QuicPacketPublicHeader::QuicPacketPublicHeader()
    : guid(0),
      reset_flag(false),
      version_flag(false) {
}

QuicPacketPublicHeader::QuicPacketPublicHeader(
    const QuicPacketPublicHeader& other)
    : guid(other.guid),
      reset_flag(other.reset_flag),
      version_flag(other.version_flag),
      versions(other.versions) {
}

QuicPacketPublicHeader::~QuicPacketPublicHeader() {}

QuicPacketPublicHeader& QuicPacketPublicHeader::operator=(
    const QuicPacketPublicHeader& other) {
  guid = other.guid;
  reset_flag = other.reset_flag;
  version_flag = other.version_flag;
  versions = other.versions;
  return *this;
}

QuicPacketHeader::QuicPacketHeader()
    : fec_flag(false),
      fec_entropy_flag(false),
      entropy_flag(false),
      entropy_hash(0),
      packet_sequence_number(0),
      fec_group(0) {
}

QuicPacketHeader::QuicPacketHeader(const QuicPacketPublicHeader& header)
    : public_header(header),
      fec_flag(false),
      fec_entropy_flag(false),
      entropy_flag(false),
      entropy_hash(0),
      packet_sequence_number(0),
      fec_group(0) {
}

QuicStreamFrame::QuicStreamFrame() {}

QuicStreamFrame::QuicStreamFrame(QuicStreamId stream_id,
                                 bool fin,
                                 QuicStreamOffset offset,
                                 StringPiece data)
    : stream_id(stream_id),
      fin(fin),
      offset(offset),
      data(data) {
}

ostream& operator<<(ostream& os, const QuicPacketHeader& header) {
  os << "{ guid: " << header.public_header.guid
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
     << ", fec_group: " << header.fec_group<< "}\n";
  return os;
}

// TODO(ianswett): Initializing largest_observed to 0 should not be necessary.
ReceivedPacketInfo::ReceivedPacketInfo()
    : largest_observed(0),
      delta_time_largest_observed(QuicTime::Delta::Infinite()) {
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

bool QuicFecData::operator==(const QuicFecData& other) const {
  if (fec_group != other.fec_group) {
    return false;
  }
  if (redundancy != other.redundancy) {
    return false;
  }
  return true;
}

QuicData::~QuicData() {
  if (owns_buffer_) {
    delete [] const_cast<char*>(buffer_);
  }
}

StringPiece QuicPacket::FecProtectedData() const {
  const size_t start_of_fec = GetStartOfFecProtectedData(includes_version_);
  return StringPiece(data() + start_of_fec, length() - start_of_fec);
}

StringPiece QuicPacket::AssociatedData() const {
  return StringPiece(data() + kStartOfHashData,
                     GetStartOfEncryptedData(includes_version_) -
                     kStartOfHashData);
}

StringPiece QuicPacket::BeforePlaintext() const {
  return StringPiece(data(), GetStartOfEncryptedData(includes_version_));
}

StringPiece QuicPacket::Plaintext() const {
  const size_t start_of_encrypted_data =
      GetStartOfEncryptedData(includes_version_);
  return StringPiece(data() + start_of_encrypted_data,
                     length() - start_of_encrypted_data);
}

RetransmittableFrames::RetransmittableFrames() {}

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
  // Make an owned copy of the StringPiece.
  string* stream_data = new string(stream_frame->data.data(),
                                   stream_frame->data.size());
  // Ensure the frame's StringPiece points to the owned copy of the data.
  stream_frame->data = StringPiece(*stream_data);
  stream_data_.push_back(stream_data);
  frames_.push_back(QuicFrame(stream_frame));
  return frames_.back();
}

const QuicFrame& RetransmittableFrames::AddNonStreamFrame(
    const QuicFrame& frame) {
  DCHECK_NE(frame.type, STREAM_FRAME);
  frames_.push_back(frame);
  return frames_.back();
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
