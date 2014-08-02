// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/rtcp_sender.h"

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/big_endian.h"
#include "base/logging.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/net/rtcp/rtcp_utility.h"

namespace media {
namespace cast {
namespace {

// Max delta is 4095 milliseconds because we need to be able to encode it in
// 12 bits.
const int64 kMaxWireFormatTimeDeltaMs = INT64_C(0xfff);

uint16 MergeEventTypeAndTimestampForWireFormat(
    const CastLoggingEvent& event,
    const base::TimeDelta& time_delta) {
  int64 time_delta_ms = time_delta.InMilliseconds();

  DCHECK_GE(time_delta_ms, 0);
  DCHECK_LE(time_delta_ms, kMaxWireFormatTimeDeltaMs);

  uint16 time_delta_12_bits =
      static_cast<uint16>(time_delta_ms & kMaxWireFormatTimeDeltaMs);

  uint16 event_type_4_bits = ConvertEventTypeToWireFormat(event);
  DCHECK(event_type_4_bits);
  DCHECK(~(event_type_4_bits & 0xfff0));
  return (event_type_4_bits << 12) | time_delta_12_bits;
}

bool EventTimestampLessThan(const RtcpReceiverEventLogMessage& lhs,
                            const RtcpReceiverEventLogMessage& rhs) {
  return lhs.event_timestamp < rhs.event_timestamp;
}

void AddReceiverLog(
    const RtcpReceiverLogMessage& redundancy_receiver_log_message,
    RtcpReceiverLogMessage* receiver_log_message,
    size_t* remaining_space,
    size_t* number_of_frames,
    size_t* total_number_of_messages_to_send) {
  RtcpReceiverLogMessage::const_iterator it =
      redundancy_receiver_log_message.begin();
  while (it != redundancy_receiver_log_message.end() &&
         *remaining_space >=
             kRtcpReceiverFrameLogSize + kRtcpReceiverEventLogSize) {
    receiver_log_message->push_front(*it);
    size_t num_event_logs = (*remaining_space - kRtcpReceiverFrameLogSize) /
                            kRtcpReceiverEventLogSize;
    RtcpReceiverEventLogMessages& event_log_messages =
        receiver_log_message->front().event_log_messages_;
    if (num_event_logs < event_log_messages.size())
      event_log_messages.resize(num_event_logs);

    *remaining_space -= kRtcpReceiverFrameLogSize +
                        event_log_messages.size() * kRtcpReceiverEventLogSize;
    ++*number_of_frames;
    *total_number_of_messages_to_send += event_log_messages.size();
    ++it;
  }
}

// A class to build a string representing the NACK list in Cast message.
//
// The string will look like "23:3-6 25:1,5-6", meaning packets 3 to 6 in frame
// 23 are being NACK'ed (i.e. they are missing from the receiver's point of
// view) and packets 1, 5 and 6 are missing in frame 25. A frame that is
// completely missing will show as "26:65535".
class NackStringBuilder {
 public:
  NackStringBuilder()
      : frame_count_(0),
        packet_count_(0),
        last_frame_id_(-1),
        last_packet_id_(-1),
        contiguous_sequence_(false) {}
  ~NackStringBuilder() {}

  bool Empty() const { return frame_count_ == 0; }

  void PushFrame(int frame_id) {
    DCHECK_GE(frame_id, 0);
    if (frame_count_ > 0) {
      if (frame_id == last_frame_id_) {
        return;
      }
      if (contiguous_sequence_) {
        stream_ << "-" << last_packet_id_;
      }
      stream_ << ", ";
    }
    stream_ << frame_id;
    last_frame_id_ = frame_id;
    packet_count_ = 0;
    contiguous_sequence_ = false;
    ++frame_count_;
  }

  void PushPacket(int packet_id) {
    DCHECK_GE(last_frame_id_, 0);
    DCHECK_GE(packet_id, 0);
    if (packet_count_ == 0) {
      stream_ << ":" << packet_id;
    } else if (packet_id == last_packet_id_ + 1) {
      contiguous_sequence_ = true;
    } else {
      if (contiguous_sequence_) {
        stream_ << "-" << last_packet_id_;
        contiguous_sequence_ = false;
      }
      stream_ << "," << packet_id;
    }
    ++packet_count_;
    last_packet_id_ = packet_id;
  }

  std::string GetString() {
    if (contiguous_sequence_) {
      stream_ << "-" << last_packet_id_;
      contiguous_sequence_ = false;
    }
    return stream_.str();
  }

 private:
  std::ostringstream stream_;
  int frame_count_;
  int packet_count_;
  int last_frame_id_;
  int last_packet_id_;
  bool contiguous_sequence_;
};
}  // namespace

RtcpSender::RtcpSender(PacedPacketSender* outgoing_transport,
                       uint32 sending_ssrc)
    : ssrc_(sending_ssrc),
      transport_(outgoing_transport) {
}

RtcpSender::~RtcpSender() {}

void RtcpSender::SendRtcpFromRtpReceiver(
    uint32 packet_type_flags,
    const RtcpReportBlock* report_block,
    const RtcpReceiverReferenceTimeReport* rrtr,
    const RtcpCastMessage* cast_message,
    const ReceiverRtcpEventSubscriber::RtcpEventMultiMap* rtcp_events,
    base::TimeDelta target_delay) {
  if (packet_type_flags & kRtcpDlrr) {
    NOTREACHED() << "Invalid argument";
  }
  PacketRef packet(new base::RefCountedData<Packet>);
  packet->data.reserve(kMaxIpPacketSize);
  if (packet_type_flags & kRtcpRr) {
    BuildRR(report_block, &packet->data);
  }
  if (packet_type_flags & kRtcpRrtr) {
    DCHECK(rrtr) << "Invalid argument";
    BuildRrtr(rrtr, &packet->data);
  }
  if (packet_type_flags & kRtcpCast) {
    DCHECK(cast_message) << "Invalid argument";
    BuildCast(cast_message, target_delay, &packet->data);
  }
  if (packet_type_flags & kRtcpReceiverLog) {
    DCHECK(rtcp_events) << "Invalid argument";
    BuildReceiverLog(*rtcp_events, &packet->data);
  }

  if (packet->data.empty()) {
    NOTREACHED() << "Empty packet.";
    return;  // Sanity don't send empty packets.
  }

  transport_->SendRtcpPacket(ssrc_, packet);
}

void RtcpSender::SendRtcpFromRtpSender(
    uint32 packet_type_flags,
    const RtcpSenderInfo& sender_info,
    const RtcpDlrrReportBlock& dlrr) {
  if (packet_type_flags & kRtcpRr ||
      packet_type_flags & kRtcpRrtr ||
      packet_type_flags & kRtcpCast ||
      packet_type_flags & kRtcpReceiverLog) {
    NOTREACHED() << "Invalid argument";
  }
  PacketRef packet(new base::RefCountedData<Packet>);
  packet->data.reserve(kMaxIpPacketSize);
  if (packet_type_flags & kRtcpSr) {
    BuildSR(sender_info, &packet->data);
  }
  if (packet_type_flags & kRtcpDlrr) {
    BuildDlrrRb(dlrr, &packet->data);
  }
  if (packet->data.empty()) {
    NOTREACHED() << "Empty packet.";
    return;  // Sanity - don't send empty packets.
  }

  transport_->SendRtcpPacket(ssrc_, packet);
}

void RtcpSender::BuildRR(const RtcpReportBlock* report_block,
                         Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 32, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 32 > kMaxIpPacketSize)
    return;

  uint16 number_of_rows = (report_block) ? 7 : 1;
  packet->resize(start_size + 8);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 8);
  big_endian_writer.WriteU8(0x80 + (report_block ? 1 : 0));
  big_endian_writer.WriteU8(kPacketTypeReceiverReport);
  big_endian_writer.WriteU16(number_of_rows);
  big_endian_writer.WriteU32(ssrc_);

  if (report_block) {
    AddReportBlocks(*report_block, packet);  // Adds 24 bytes.
  }
}

void RtcpSender::AddReportBlocks(const RtcpReportBlock& report_block,
                                 Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 24, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 24 > kMaxIpPacketSize)
    return;

  packet->resize(start_size + 24);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 24);
  big_endian_writer.WriteU32(report_block.media_ssrc);
  big_endian_writer.WriteU8(report_block.fraction_lost);
  big_endian_writer.WriteU8(report_block.cumulative_lost >> 16);
  big_endian_writer.WriteU8(report_block.cumulative_lost >> 8);
  big_endian_writer.WriteU8(report_block.cumulative_lost);

  // Extended highest seq_no, contain the highest sequence number received.
  big_endian_writer.WriteU32(report_block.extended_high_sequence_number);
  big_endian_writer.WriteU32(report_block.jitter);

  // Last SR timestamp; our NTP time when we received the last report.
  // This is the value that we read from the send report packet not when we
  // received it.
  big_endian_writer.WriteU32(report_block.last_sr);

  // Delay since last received report, time since we received the report.
  big_endian_writer.WriteU32(report_block.delay_since_last_sr);
}

void RtcpSender::BuildRrtr(const RtcpReceiverReferenceTimeReport* rrtr,
                           Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 20, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 20 > kMaxIpPacketSize)
    return;

  packet->resize(start_size + 20);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 20);

  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(kPacketTypeXr);
  big_endian_writer.WriteU16(4);      // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(4);       // Add block type.
  big_endian_writer.WriteU8(0);       // Add reserved.
  big_endian_writer.WriteU16(2);      // Block length.

  // Add the media (received RTP) SSRC.
  big_endian_writer.WriteU32(rrtr->ntp_seconds);
  big_endian_writer.WriteU32(rrtr->ntp_fraction);
}

void RtcpSender::BuildCast(const RtcpCastMessage* cast,
                           base::TimeDelta target_delay,
                           Packet* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 20, kMaxIpPacketSize) << "Not enough buffer space";
  if (start_size + 20 > kMaxIpPacketSize)
    return;

  packet->resize(start_size + 20);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 20);
  uint8 FMT = 15;  // Application layer feedback.
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(kPacketTypePayloadSpecific);
  big_endian_writer.WriteU8(0);
  size_t cast_size_pos = start_size + 3;  // Save length position.
  big_endian_writer.WriteU8(4);
  big_endian_writer.WriteU32(ssrc_);              // Add our own SSRC.
  big_endian_writer.WriteU32(cast->media_ssrc);  // Remote SSRC.
  big_endian_writer.WriteU32(kCast);
  big_endian_writer.WriteU8(static_cast<uint8>(cast->ack_frame_id));
  size_t cast_loss_field_pos = start_size + 17;  // Save loss field position.
  big_endian_writer.WriteU8(0);  // Overwritten with number_of_loss_fields.
  DCHECK_LE(target_delay.InMilliseconds(),
            std::numeric_limits<uint16_t>::max());
  big_endian_writer.WriteU16(target_delay.InMilliseconds());

  size_t number_of_loss_fields = 0;
  size_t max_number_of_loss_fields = std::min<size_t>(
      kRtcpMaxCastLossFields, (kMaxIpPacketSize - packet->size()) / 4);

  MissingFramesAndPacketsMap::const_iterator frame_it =
      cast->missing_frames_and_packets.begin();

  NackStringBuilder nack_string_builder;
  for (; frame_it != cast->missing_frames_and_packets.end() &&
             number_of_loss_fields < max_number_of_loss_fields;
       ++frame_it) {
    nack_string_builder.PushFrame(frame_it->first);
    // Iterate through all frames with missing packets.
    if (frame_it->second.empty()) {
      // Special case all packets in a frame is missing.
      start_size = packet->size();
      packet->resize(start_size + 4);
      base::BigEndianWriter big_endian_nack_writer(
          reinterpret_cast<char*>(&((*packet)[start_size])), 4);
      big_endian_nack_writer.WriteU8(static_cast<uint8>(frame_it->first));
      big_endian_nack_writer.WriteU16(kRtcpCastAllPacketsLost);
      big_endian_nack_writer.WriteU8(0);
      nack_string_builder.PushPacket(kRtcpCastAllPacketsLost);
      ++number_of_loss_fields;
    } else {
      PacketIdSet::const_iterator packet_it = frame_it->second.begin();
      while (packet_it != frame_it->second.end()) {
        uint16 packet_id = *packet_it;

        start_size = packet->size();
        packet->resize(start_size + 4);
        base::BigEndianWriter big_endian_nack_writer(
            reinterpret_cast<char*>(&((*packet)[start_size])), 4);

        // Write frame and packet id to buffer before calculating bitmask.
        big_endian_nack_writer.WriteU8(static_cast<uint8>(frame_it->first));
        big_endian_nack_writer.WriteU16(packet_id);
        nack_string_builder.PushPacket(packet_id);

        uint8 bitmask = 0;
        ++packet_it;
        while (packet_it != frame_it->second.end()) {
          int shift = static_cast<uint8>(*packet_it - packet_id) - 1;
          if (shift >= 0 && shift <= 7) {
            nack_string_builder.PushPacket(*packet_it);
            bitmask |= (1 << shift);
            ++packet_it;
          } else {
            break;
          }
        }
        big_endian_nack_writer.WriteU8(bitmask);
        ++number_of_loss_fields;
      }
    }
  }
  VLOG_IF(1, !nack_string_builder.Empty())
      << "SSRC: " << cast->media_ssrc
      << ", ACK: " << cast->ack_frame_id
      << ", NACK: " << nack_string_builder.GetString();
  DCHECK_LE(number_of_loss_fields, kRtcpMaxCastLossFields);
  (*packet)[cast_size_pos] = static_cast<uint8>(4 + number_of_loss_fields);
  (*packet)[cast_loss_field_pos] = static_cast<uint8>(number_of_loss_fields);
}

void RtcpSender::BuildSR(const RtcpSenderInfo& sender_info,
                         Packet* packet) const {
  // Sender report.
  size_t start_size = packet->size();
  if (start_size + 52 > kMaxIpPacketSize) {
    DLOG(FATAL) << "Not enough buffer space";
    return;
  }

  uint16 number_of_rows = 6;
  packet->resize(start_size + 28);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 28);
  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(kPacketTypeSenderReport);
  big_endian_writer.WriteU16(number_of_rows);
  big_endian_writer.WriteU32(ssrc_);
  big_endian_writer.WriteU32(sender_info.ntp_seconds);
  big_endian_writer.WriteU32(sender_info.ntp_fraction);
  big_endian_writer.WriteU32(sender_info.rtp_timestamp);
  big_endian_writer.WriteU32(sender_info.send_packet_count);
  big_endian_writer.WriteU32(static_cast<uint32>(sender_info.send_octet_count));
  return;
}

/*
   0                   1                   2                   3
   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |V=2|P|reserved |   PT=XR=207   |             length            |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                              SSRC                             |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     BT=5      |   reserved    |         block length          |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  |                 SSRC1 (SSRC of first receiver)               | sub-
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
  |                         last RR (LRR)                         |   1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                   delay since last RR (DLRR)                  |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/
void RtcpSender::BuildDlrrRb(const RtcpDlrrReportBlock& dlrr,
                             Packet* packet) const {
  size_t start_size = packet->size();
  if (start_size + 24 > kMaxIpPacketSize) {
    DLOG(FATAL) << "Not enough buffer space";
    return;
  }

  packet->resize(start_size + 24);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[start_size])), 24);
  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(kPacketTypeXr);
  big_endian_writer.WriteU16(5);  // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(5);  // Add block type.
  big_endian_writer.WriteU8(0);  // Add reserved.
  big_endian_writer.WriteU16(3);  // Block length.
  big_endian_writer.WriteU32(ssrc_);  // Add the media (received RTP) SSRC.
  big_endian_writer.WriteU32(dlrr.last_rr);
  big_endian_writer.WriteU32(dlrr.delay_since_last_rr);
  return;
}

void RtcpSender::BuildReceiverLog(
    const ReceiverRtcpEventSubscriber::RtcpEventMultiMap& rtcp_events,
    Packet* packet) {
  const size_t packet_start_size = packet->size();
  size_t number_of_frames = 0;
  size_t total_number_of_messages_to_send = 0;
  size_t rtcp_log_size = 0;
  RtcpReceiverLogMessage receiver_log_message;

  if (!BuildRtcpReceiverLogMessage(rtcp_events,
                                   packet_start_size,
                                   &receiver_log_message,
                                   &number_of_frames,
                                   &total_number_of_messages_to_send,
                                   &rtcp_log_size)) {
    return;
  }
  packet->resize(packet_start_size + rtcp_log_size);

  base::BigEndianWriter big_endian_writer(
      reinterpret_cast<char*>(&((*packet)[packet_start_size])), rtcp_log_size);
  big_endian_writer.WriteU8(0x80 + kReceiverLogSubtype);
  big_endian_writer.WriteU8(kPacketTypeApplicationDefined);
  big_endian_writer.WriteU16(static_cast<uint16>(
      2 + 2 * number_of_frames + total_number_of_messages_to_send));
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(kCast);

  while (!receiver_log_message.empty() &&
         total_number_of_messages_to_send > 0) {
    RtcpReceiverFrameLogMessage& frame_log_messages(
        receiver_log_message.front());

    // Add our frame header.
    big_endian_writer.WriteU32(frame_log_messages.rtp_timestamp_);
    size_t messages_in_frame = frame_log_messages.event_log_messages_.size();
    if (messages_in_frame > total_number_of_messages_to_send) {
      // We are running out of space.
      messages_in_frame = total_number_of_messages_to_send;
    }
    // Keep track of how many messages we have left to send.
    total_number_of_messages_to_send -= messages_in_frame;

    // On the wire format is number of messages - 1.
    big_endian_writer.WriteU8(static_cast<uint8>(messages_in_frame - 1));

    base::TimeTicks event_timestamp_base =
        frame_log_messages.event_log_messages_.front().event_timestamp;
    uint32 base_timestamp_ms =
        (event_timestamp_base - base::TimeTicks()).InMilliseconds();
    big_endian_writer.WriteU8(static_cast<uint8>(base_timestamp_ms >> 16));
    big_endian_writer.WriteU8(static_cast<uint8>(base_timestamp_ms >> 8));
    big_endian_writer.WriteU8(static_cast<uint8>(base_timestamp_ms));

    while (!frame_log_messages.event_log_messages_.empty() &&
           messages_in_frame > 0) {
      const RtcpReceiverEventLogMessage& event_message =
          frame_log_messages.event_log_messages_.front();
      uint16 event_type_and_timestamp_delta =
          MergeEventTypeAndTimestampForWireFormat(
              event_message.type,
              event_message.event_timestamp - event_timestamp_base);
      switch (event_message.type) {
        case FRAME_ACK_SENT:
        case FRAME_PLAYOUT:
        case FRAME_DECODED:
          big_endian_writer.WriteU16(
              static_cast<uint16>(event_message.delay_delta.InMilliseconds()));
          big_endian_writer.WriteU16(event_type_and_timestamp_delta);
          break;
        case PACKET_RECEIVED:
          big_endian_writer.WriteU16(event_message.packet_id);
          big_endian_writer.WriteU16(event_type_and_timestamp_delta);
          break;
        default:
          NOTREACHED();
      }
      messages_in_frame--;
      frame_log_messages.event_log_messages_.pop_front();
    }
    if (frame_log_messages.event_log_messages_.empty()) {
      // We sent all messages on this frame; pop the frame header.
      receiver_log_message.pop_front();
    }
  }
  DCHECK_EQ(total_number_of_messages_to_send, 0u);
}

bool RtcpSender::BuildRtcpReceiverLogMessage(
    const ReceiverRtcpEventSubscriber::RtcpEventMultiMap& rtcp_events,
    size_t start_size,
    RtcpReceiverLogMessage* receiver_log_message,
    size_t* number_of_frames,
    size_t* total_number_of_messages_to_send,
    size_t* rtcp_log_size) {
  size_t remaining_space =
      std::min(kMaxReceiverLogBytes, kMaxIpPacketSize - start_size);
  if (remaining_space < kRtcpCastLogHeaderSize + kRtcpReceiverFrameLogSize +
                            kRtcpReceiverEventLogSize) {
    return false;
  }

  // We use this to do event timestamp sorting and truncating for events of
  // a single frame.
  std::vector<RtcpReceiverEventLogMessage> sorted_log_messages;

  // Account for the RTCP header for an application-defined packet.
  remaining_space -= kRtcpCastLogHeaderSize;

  ReceiverRtcpEventSubscriber::RtcpEventMultiMap::const_reverse_iterator rit =
      rtcp_events.rbegin();

  while (rit != rtcp_events.rend() &&
         remaining_space >=
             kRtcpReceiverFrameLogSize + kRtcpReceiverEventLogSize) {
    const RtpTimestamp rtp_timestamp = rit->first;
    RtcpReceiverFrameLogMessage frame_log(rtp_timestamp);
    remaining_space -= kRtcpReceiverFrameLogSize;
    ++*number_of_frames;

    // Get all events of a single frame.
    sorted_log_messages.clear();
    do {
      RtcpReceiverEventLogMessage event_log_message;
      event_log_message.type = rit->second.type;
      event_log_message.event_timestamp = rit->second.timestamp;
      event_log_message.delay_delta = rit->second.delay_delta;
      event_log_message.packet_id = rit->second.packet_id;
      sorted_log_messages.push_back(event_log_message);
      ++rit;
    } while (rit != rtcp_events.rend() && rit->first == rtp_timestamp);

    std::sort(sorted_log_messages.begin(),
              sorted_log_messages.end(),
              &EventTimestampLessThan);

    // From |sorted_log_messages|, only take events that are no greater than
    // |kMaxWireFormatTimeDeltaMs| seconds away from the latest event. Events
    // older than that cannot be encoded over the wire.
    std::vector<RtcpReceiverEventLogMessage>::reverse_iterator sorted_rit =
        sorted_log_messages.rbegin();
    base::TimeTicks first_event_timestamp = sorted_rit->event_timestamp;
    size_t events_in_frame = 0;
    while (sorted_rit != sorted_log_messages.rend() &&
           events_in_frame < kRtcpMaxReceiverLogMessages &&
           remaining_space >= kRtcpReceiverEventLogSize) {
      base::TimeDelta delta(first_event_timestamp -
                            sorted_rit->event_timestamp);
      if (delta.InMilliseconds() > kMaxWireFormatTimeDeltaMs)
        break;
      frame_log.event_log_messages_.push_front(*sorted_rit);
      ++events_in_frame;
      ++*total_number_of_messages_to_send;
      remaining_space -= kRtcpReceiverEventLogSize;
      ++sorted_rit;
    }

    receiver_log_message->push_front(frame_log);
  }

  rtcp_events_history_.push_front(*receiver_log_message);

  // We don't try to match RTP timestamps of redundancy frame logs with those
  // from the newest set (which would save the space of an extra RTP timestamp
  // over the wire). Unless the redundancy frame logs are very recent, it's
  // unlikely there will be a match anyway.
  if (rtcp_events_history_.size() > kFirstRedundancyOffset) {
    // Add first redundnacy messages, if enough space remaining
    AddReceiverLog(rtcp_events_history_[kFirstRedundancyOffset],
                   receiver_log_message,
                   &remaining_space,
                   number_of_frames,
                   total_number_of_messages_to_send);
  }

  if (rtcp_events_history_.size() > kSecondRedundancyOffset) {
    // Add second redundancy messages, if enough space remaining
    AddReceiverLog(rtcp_events_history_[kSecondRedundancyOffset],
                   receiver_log_message,
                   &remaining_space,
                   number_of_frames,
                   total_number_of_messages_to_send);
  }

  if (rtcp_events_history_.size() > kReceiveLogMessageHistorySize) {
    rtcp_events_history_.pop_back();
  }

  DCHECK_LE(rtcp_events_history_.size(), kReceiveLogMessageHistorySize);

  *rtcp_log_size =
      kRtcpCastLogHeaderSize + *number_of_frames * kRtcpReceiverFrameLogSize +
      *total_number_of_messages_to_send * kRtcpReceiverEventLogSize;
  DCHECK_GE(kMaxIpPacketSize, start_size + *rtcp_log_size)
      << "Not enough buffer space.";

  VLOG(3) << "number of frames: " << *number_of_frames;
  VLOG(3) << "total messages to send: " << *total_number_of_messages_to_send;
  VLOG(3) << "rtcp log size: " << *rtcp_log_size;
  return *number_of_frames > 0;
}

}  // namespace cast
}  // namespace media
