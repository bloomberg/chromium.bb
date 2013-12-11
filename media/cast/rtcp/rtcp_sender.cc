// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtcp/rtcp_sender.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "net/base/big_endian.h"

static const size_t kRtcpCastLogHeaderSize = 12;
static const size_t kRtcpSenderFrameLogSize = 4;
static const size_t kRtcpReceiverFrameLogSize = 8;
static const size_t kRtcpReceiverEventLogSize = 4;

namespace {
uint16 MergeEventTypeAndTimestampForWireFormat(
    const media::cast::CastLoggingEvent& event,
    const base::TimeDelta& time_delta) {
  int64 time_delta_ms = time_delta.InMilliseconds();
  // Max delta is 4096 milliseconds.
  DCHECK_GE(GG_INT64_C(0xfff), time_delta_ms);

  uint16 event_type_and_timestamp_delta =
      static_cast<uint16>(time_delta_ms & 0xfff);

  uint16 event_type = 0;
  switch (event) {
    case media::cast::kAckSent:
      event_type = 1;
      break;
    case media::cast::kAudioPlayoutDelay:
      event_type = 2;
      break;
    case media::cast::kAudioFrameDecoded:
      event_type = 3;
      break;
    case media::cast::kVideoFrameDecoded:
      event_type = 4;
      break;
    case media::cast::kVideoRenderDelay:
      event_type = 5;
      break;
    case media::cast::kPacketReceived:
      event_type = 6;
      break;
    default:
      NOTREACHED();
  }
  DCHECK(!(event_type & 0xfff0));
  return (event_type << 12) + event_type_and_timestamp_delta;
}

bool ScanRtcpReceiverLogMessage(
    const media::cast::RtcpReceiverLogMessage& receiver_log_message,
    size_t start_size,
    size_t* number_of_frames,
    size_t* total_number_of_messages_to_send,
    size_t* rtcp_log_size) {
  if (receiver_log_message.empty()) return false;

  size_t remaining_space = media::cast::kIpPacketSize - start_size;

  // We must have space for at least one message
  DCHECK_GE(remaining_space, kRtcpCastLogHeaderSize +
      kRtcpReceiverFrameLogSize + kRtcpReceiverEventLogSize)
      << "Not enough buffer space";

  if (remaining_space < kRtcpCastLogHeaderSize + kRtcpReceiverFrameLogSize +
      kRtcpReceiverEventLogSize) {
    return false;
  }
  // Account for the RTCP header for an application-defined packet.
  remaining_space -= kRtcpCastLogHeaderSize;

  media::cast::RtcpReceiverLogMessage::const_iterator frame_it =
      receiver_log_message.begin();
  for (; frame_it != receiver_log_message.end(); ++frame_it) {
    (*number_of_frames)++;

    remaining_space -= kRtcpReceiverFrameLogSize;

    size_t messages_in_frame = frame_it->event_log_messages_.size();
    size_t remaining_space_in_messages =
        remaining_space / kRtcpReceiverEventLogSize;
    size_t messages_to_send = std::min(messages_in_frame,
                                       remaining_space_in_messages);
    if (messages_to_send > media::cast::kRtcpMaxReceiverLogMessages) {
      // We can't send more than 256 messages.
      remaining_space -= media::cast::kRtcpMaxReceiverLogMessages *
          kRtcpReceiverEventLogSize;
      *total_number_of_messages_to_send +=
          media::cast::kRtcpMaxReceiverLogMessages;
      break;
    }
    remaining_space -= messages_to_send * kRtcpReceiverEventLogSize;
    *total_number_of_messages_to_send += messages_to_send;

    if (remaining_space <
        kRtcpReceiverFrameLogSize + kRtcpReceiverEventLogSize) {
      // Make sure that we have room for at least one more message.
      break;
    }
  }
  *rtcp_log_size = kRtcpCastLogHeaderSize +
      *number_of_frames * kRtcpReceiverFrameLogSize +
      *total_number_of_messages_to_send * kRtcpReceiverEventLogSize;
  DCHECK_GE(media::cast::kIpPacketSize,
            start_size + *rtcp_log_size) << "Not enough buffer space";

  VLOG(1) << "number of frames " << *number_of_frames;
  VLOG(1) << "total messages to send " << *total_number_of_messages_to_send;
  VLOG(1) << "rtcp log size " << *rtcp_log_size;
  return true;
}
}  // namespace

namespace media {
namespace cast {

RtcpSender::RtcpSender(scoped_refptr<CastEnvironment> cast_environment,
                       PacedPacketSender* outgoing_transport,
                       uint32 sending_ssrc,
                       const std::string& c_name)
     : ssrc_(sending_ssrc),
       c_name_(c_name),
       transport_(outgoing_transport),
       cast_environment_(cast_environment) {
  DCHECK_LT(c_name_.length(), kRtcpCnameSize) << "Invalid config";
}

RtcpSender::~RtcpSender() {}

void RtcpSender::SendRtcpFromRtpSender(uint32 packet_type_flags,
                                       const RtcpSenderInfo* sender_info,
                                       const RtcpDlrrReportBlock* dlrr,
                                       RtcpSenderLogMessage* sender_log) {
  if (packet_type_flags & kRtcpRr ||
      packet_type_flags & kRtcpPli ||
      packet_type_flags & kRtcpRrtr ||
      packet_type_flags & kRtcpCast ||
      packet_type_flags & kRtcpReceiverLog ||
      packet_type_flags & kRtcpRpsi ||
      packet_type_flags & kRtcpRemb ||
      packet_type_flags & kRtcpNack) {
    NOTREACHED() << "Invalid argument";
  }

  std::vector<uint8> packet;
  packet.reserve(kIpPacketSize);
  if (packet_type_flags & kRtcpSr) {
    DCHECK(sender_info) << "Invalid argument";
    BuildSR(*sender_info, NULL, &packet);
    BuildSdec(&packet);
  }
  if (packet_type_flags & kRtcpBye) {
    BuildBye(&packet);
  }
  if (packet_type_flags & kRtcpDlrr) {
    DCHECK(dlrr) << "Invalid argument";
    BuildDlrrRb(dlrr, &packet);
  }
  if (packet_type_flags & kRtcpSenderLog) {
    DCHECK(sender_log) << "Invalid argument";
    BuildSenderLog(sender_log, &packet);
  }
  if (packet.empty())
    return;  // Sanity don't send empty packets.

  transport_->SendRtcpPacket(packet);
}

void RtcpSender::SendRtcpFromRtpReceiver(
    uint32 packet_type_flags,
    const RtcpReportBlock* report_block,
    const RtcpReceiverReferenceTimeReport* rrtr,
    const RtcpCastMessage* cast_message,
    RtcpReceiverLogMessage* receiver_log) {
  if (packet_type_flags & kRtcpSr ||
      packet_type_flags & kRtcpDlrr ||
      packet_type_flags & kRtcpSenderLog) {
    NOTREACHED() << "Invalid argument";
  }
  if (packet_type_flags & kRtcpPli ||
      packet_type_flags & kRtcpRpsi ||
      packet_type_flags & kRtcpRemb ||
      packet_type_flags & kRtcpNack) {
    // Implement these for webrtc interop.
    NOTIMPLEMENTED();
  }
  std::vector<uint8> packet;
  packet.reserve(kIpPacketSize);

  if (packet_type_flags & kRtcpRr) {
    BuildRR(report_block, &packet);
    if (!c_name_.empty()) {
      BuildSdec(&packet);
    }
  }
  if (packet_type_flags & kRtcpBye) {
    BuildBye(&packet);
  }
  if (packet_type_flags & kRtcpRrtr) {
    DCHECK(rrtr) << "Invalid argument";
    BuildRrtr(rrtr, &packet);
  }
  if (packet_type_flags & kRtcpCast) {
    DCHECK(cast_message) << "Invalid argument";
    BuildCast(cast_message, &packet);
  }
  if (packet_type_flags & kRtcpReceiverLog) {
    DCHECK(receiver_log) << "Invalid argument";
    BuildReceiverLog(receiver_log, &packet);
  }
  if (packet.empty()) return;  // Sanity don't send empty packets.

  transport_->SendRtcpPacket(packet);
}

void RtcpSender::BuildSR(const RtcpSenderInfo& sender_info,
                         const RtcpReportBlock* report_block,
                         std::vector<uint8>* packet) const {
  // Sender report.
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 52, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 52 > kIpPacketSize) return;

  uint16 number_of_rows = (report_block) ? 12 : 6;
  packet->resize(start_size + 28);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 28);
  big_endian_writer.WriteU8(0x80 + (report_block ? 1 : 0));
  big_endian_writer.WriteU8(kPacketTypeSenderReport);
  big_endian_writer.WriteU16(number_of_rows);
  big_endian_writer.WriteU32(ssrc_);
  big_endian_writer.WriteU32(sender_info.ntp_seconds);
  big_endian_writer.WriteU32(sender_info.ntp_fraction);
  big_endian_writer.WriteU32(sender_info.rtp_timestamp);
  big_endian_writer.WriteU32(sender_info.send_packet_count);
  big_endian_writer.WriteU32(static_cast<uint32>(sender_info.send_octet_count));

  if (report_block) {
    AddReportBlocks(*report_block, packet);  // Adds 24 bytes.
  }
}

void RtcpSender::BuildRR(const RtcpReportBlock* report_block,
                         std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 32, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 32 > kIpPacketSize) return;

  uint16 number_of_rows = (report_block) ? 7 : 1;
  packet->resize(start_size + 8);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 8);
  big_endian_writer.WriteU8(0x80 + (report_block ? 1 : 0));
  big_endian_writer.WriteU8(kPacketTypeReceiverReport);
  big_endian_writer.WriteU16(number_of_rows);
  big_endian_writer.WriteU32(ssrc_);

  if (report_block) {
    AddReportBlocks(*report_block, packet);  // Adds 24 bytes.
  }
}

void RtcpSender::AddReportBlocks(const RtcpReportBlock& report_block,
                                 std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 24, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 24 > kIpPacketSize) return;

  packet->resize(start_size + 24);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 24);
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

void RtcpSender::BuildSdec(std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size +  12 + c_name_.length(), kIpPacketSize)
      << "Not enough buffer space";
  if (start_size + 12 > kIpPacketSize) return;

  // SDES Source Description.
  packet->resize(start_size + 10);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 10);
  // We always need to add one SDES CNAME.
  big_endian_writer.WriteU8(0x80 + 1);
  big_endian_writer.WriteU8(kPacketTypeSdes);

  // Handle SDES length later on.
  uint32 sdes_length_position = static_cast<uint32>(start_size) + 3;
  big_endian_writer.WriteU16(0);
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(1);  // CNAME = 1
  big_endian_writer.WriteU8(static_cast<uint8>(c_name_.length()));

  size_t sdes_length = 10 + c_name_.length();
  packet->insert(packet->end(), c_name_.c_str(),
                 c_name_.c_str() + c_name_.length());

  size_t padding = 0;

  // We must have a zero field even if we have an even multiple of 4 bytes.
  if ((packet->size() % 4) == 0) {
    padding++;
    packet->push_back(0);
  }
  while ((packet->size() % 4) != 0) {
    padding++;
    packet->push_back(0);
  }
  sdes_length += padding;

  // In 32-bit words minus one and we don't count the header.
  uint8 buffer_length = static_cast<uint8>((sdes_length / 4) - 1);
  (*packet)[sdes_length_position] = buffer_length;
}

void RtcpSender::BuildPli(uint32 remote_ssrc,
                          std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 12, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 12 > kIpPacketSize) return;

  packet->resize(start_size + 12);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 12);
  uint8 FMT = 1;  // Picture loss indicator.
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(kPacketTypePayloadSpecific);
  big_endian_writer.WriteU16(2);  // Used fixed length of 2.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(remote_ssrc);  // Add the remote SSRC.
}

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      PB       |0| Payload Type|    Native Rpsi bit string     |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   defined per codec          ...                | Padding (0) |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
void RtcpSender::BuildRpsi(const RtcpRpsiMessage* rpsi,
                           std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 24, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 24 > kIpPacketSize) return;

  packet->resize(start_size + 24);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 24);
  uint8 FMT = 3;  // Reference Picture Selection Indication.
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(kPacketTypePayloadSpecific);

  // Calculate length.
  uint32 bits_required = 7;
  uint8 bytes_required = 1;
  while ((rpsi->picture_id >> bits_required) > 0) {
    bits_required += 7;
    bytes_required++;
  }
  uint8 size = 3;
  if (bytes_required > 6) {
    size = 5;
  } else if (bytes_required > 2) {
    size = 4;
  }
  big_endian_writer.WriteU8(0);
  big_endian_writer.WriteU8(size);
  big_endian_writer.WriteU32(ssrc_);
  big_endian_writer.WriteU32(rpsi->remote_ssrc);

  uint8 padding_bytes = 4 - ((2 + bytes_required) % 4);
  if (padding_bytes == 4) {
    padding_bytes = 0;
  }
  // Add padding length in bits, padding can be 0, 8, 16 or 24.
  big_endian_writer.WriteU8(padding_bytes * 8);
  big_endian_writer.WriteU8(rpsi->payload_type);

  // Add picture ID.
  for (int i = bytes_required - 1; i > 0; i--) {
    big_endian_writer.WriteU8(
        0x80 | static_cast<uint8>(rpsi->picture_id >> (i * 7)));
  }
  // Add last byte of picture ID.
  big_endian_writer.WriteU8(static_cast<uint8>(rpsi->picture_id & 0x7f));

  // Add padding.
  for (int j = 0; j < padding_bytes; ++j) {
    big_endian_writer.WriteU8(0);
  }
}

void RtcpSender::BuildRemb(const RtcpRembMessage* remb,
                           std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  size_t remb_size = 20 + 4 * remb->remb_ssrcs.size();
  DCHECK_LT(start_size + remb_size, kIpPacketSize)
      << "Not enough buffer space";
  if (start_size + remb_size > kIpPacketSize) return;

  packet->resize(start_size + remb_size);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), remb_size);

  // Add application layer feedback.
  uint8 FMT = 15;
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(kPacketTypePayloadSpecific);
  big_endian_writer.WriteU8(0);
  big_endian_writer.WriteU8(static_cast<uint8>(remb->remb_ssrcs.size() + 4));
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(0);  // Remote SSRC must be 0.
  big_endian_writer.WriteU32(kRemb);
  big_endian_writer.WriteU8(static_cast<uint8>(remb->remb_ssrcs.size()));

  // 6 bit exponent and a 18 bit mantissa.
  uint8 bitrate_exponent;
  uint32 bitrate_mantissa;
  BitrateToRembExponentBitrate(remb->remb_bitrate,
                               &bitrate_exponent,
                               &bitrate_mantissa);

  big_endian_writer.WriteU8(static_cast<uint8>((bitrate_exponent << 2) +
      ((bitrate_mantissa >> 16) & 0x03)));
  big_endian_writer.WriteU8(static_cast<uint8>(bitrate_mantissa >> 8));
  big_endian_writer.WriteU8(static_cast<uint8>(bitrate_mantissa));

  std::list<uint32>::const_iterator it = remb->remb_ssrcs.begin();
  for (; it != remb->remb_ssrcs.end(); ++it) {
    big_endian_writer.WriteU32(*it);
  }
  cast_environment_->Logging()->InsertGenericEvent(kRembBitrate,
                                                   remb->remb_bitrate);
}

void RtcpSender::BuildNack(const RtcpNackMessage* nack,
                           std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 16, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 16 > kIpPacketSize) return;

  packet->resize(start_size + 16);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 16);

  uint8 FMT = 1;
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(kPacketTypeGenericRtpFeedback);
  big_endian_writer.WriteU8(0);
  size_t nack_size_pos = start_size + 3;
  big_endian_writer.WriteU8(3);
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(nack->remote_ssrc);  // Add the remote SSRC.

  // Build NACK bitmasks and write them to the Rtcp message.
  // The nack list should be sorted and not contain duplicates.
  size_t number_of_nack_fields = 0;
  size_t max_number_of_nack_fields = std::min<size_t>(kRtcpMaxNackFields,
      (kIpPacketSize - packet->size()) / 4);

  std::list<uint16>::const_iterator it = nack->nack_list.begin();
  while (it != nack->nack_list.end() &&
         number_of_nack_fields < max_number_of_nack_fields) {
    uint16 nack_sequence_number = *it;
    uint16 bitmask = 0;
    ++it;
    while (it != nack->nack_list.end()) {
      int shift = static_cast<uint16>(*it - nack_sequence_number) - 1;
      if (shift >= 0 && shift <= 15) {
        bitmask |= (1 << shift);
        ++it;
      } else {
        break;
      }
    }
    // Write the sequence number and the bitmask to the packet.
    start_size = packet->size();
    DCHECK_LT(start_size + 4, kIpPacketSize) << "Not enough buffer space";
    if (start_size + 4 > kIpPacketSize) return;

    packet->resize(start_size + 4);
    net::BigEndianWriter big_endian_nack_writer(&((*packet)[start_size]), 4);
    big_endian_nack_writer.WriteU16(nack_sequence_number);
    big_endian_nack_writer.WriteU16(bitmask);
    number_of_nack_fields++;
  }
  DCHECK_GE(kRtcpMaxNackFields, number_of_nack_fields);
  (*packet)[nack_size_pos] = static_cast<uint8>(2 + number_of_nack_fields);
}

void RtcpSender::BuildBye(std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 8, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 8 > kIpPacketSize) return;

  packet->resize(start_size + 8);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 8);
  big_endian_writer.WriteU8(0x80 + 1);
  big_endian_writer.WriteU8(kPacketTypeBye);
  big_endian_writer.WriteU16(1);  // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
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
  |                 SSRC_1 (SSRC of first receiver)               | sub-
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
  |                         last RR (LRR)                         |   1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                   delay since last RR (DLRR)                  |
  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/
void RtcpSender::BuildDlrrRb(const RtcpDlrrReportBlock* dlrr,
                             std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 24, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 24 > kIpPacketSize) return;

  packet->resize(start_size + 24);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 24);
  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(kPacketTypeXr);
  big_endian_writer.WriteU16(5);  // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(5);  // Add block type.
  big_endian_writer.WriteU8(0);  // Add reserved.
  big_endian_writer.WriteU16(3);  // Block length.
  big_endian_writer.WriteU32(ssrc_);  // Add the media (received RTP) SSRC.
  big_endian_writer.WriteU32(dlrr->last_rr);
  big_endian_writer.WriteU32(dlrr->delay_since_last_rr);
}

void RtcpSender::BuildRrtr(const RtcpReceiverReferenceTimeReport* rrtr,
                           std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 20, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 20 > kIpPacketSize) return;

  packet->resize(start_size + 20);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 20);

  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(kPacketTypeXr);
  big_endian_writer.WriteU16(4);  // Length.
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU8(4);  // Add block type.
  big_endian_writer.WriteU8(0);  // Add reserved.
  big_endian_writer.WriteU16(2);  // Block length.

  // Add the media (received RTP) SSRC.
  big_endian_writer.WriteU32(rrtr->ntp_seconds);
  big_endian_writer.WriteU32(rrtr->ntp_fraction);
}

void RtcpSender::BuildCast(const RtcpCastMessage* cast,
                           std::vector<uint8>* packet) const {
  size_t start_size = packet->size();
  DCHECK_LT(start_size + 20, kIpPacketSize) << "Not enough buffer space";
  if (start_size + 20 > kIpPacketSize) return;

  packet->resize(start_size + 20);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), 20);
  uint8 FMT = 15;  // Application layer feedback.
  big_endian_writer.WriteU8(0x80 + FMT);
  big_endian_writer.WriteU8(kPacketTypePayloadSpecific);
  big_endian_writer.WriteU8(0);
  size_t cast_size_pos = start_size + 3; // Save length position.
  big_endian_writer.WriteU8(4);
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(cast->media_ssrc_);  // Remote SSRC.
  big_endian_writer.WriteU32(kCast);
  big_endian_writer.WriteU8(static_cast<uint8>(cast->ack_frame_id_));
  size_t cast_loss_field_pos = start_size + 17;  // Save loss field position.
  big_endian_writer.WriteU8(0);  // Overwritten with number_of_loss_fields.
  big_endian_writer.WriteU8(0);  // Reserved.
  big_endian_writer.WriteU8(0);  // Reserved.

  size_t number_of_loss_fields = 0;
  size_t max_number_of_loss_fields = std::min<size_t>(kRtcpMaxCastLossFields,
      (kIpPacketSize - packet->size()) / 4);

  MissingFramesAndPacketsMap::const_iterator frame_it =
      cast->missing_frames_and_packets_.begin();

  for (; frame_it != cast->missing_frames_and_packets_.end() &&
      number_of_loss_fields < max_number_of_loss_fields; ++frame_it) {
    // Iterate through all frames with missing packets.
    if (frame_it->second.empty()) {
      // Special case all packets in a frame is missing.
      start_size = packet->size();
      packet->resize(start_size + 4);
      net::BigEndianWriter big_endian_nack_writer(&((*packet)[start_size]), 4);
      big_endian_nack_writer.WriteU8(static_cast<uint8>(frame_it->first));
      big_endian_nack_writer.WriteU16(kRtcpCastAllPacketsLost);
      big_endian_nack_writer.WriteU8(0);
      ++number_of_loss_fields;
    } else {
      PacketIdSet::const_iterator packet_it = frame_it->second.begin();
      while (packet_it != frame_it->second.end()) {
        uint16 packet_id = *packet_it;

        start_size = packet->size();
        packet->resize(start_size + 4);
        net::BigEndianWriter big_endian_nack_writer(
            &((*packet)[start_size]), 4);

        // Write frame and packet id to buffer before calculating bitmask.
        big_endian_nack_writer.WriteU8(static_cast<uint8>(frame_it->first));
        big_endian_nack_writer.WriteU16(packet_id);

        uint8 bitmask = 0;
        ++packet_it;
        while (packet_it != frame_it->second.end()) {
          int shift = static_cast<uint8>(*packet_it - packet_id) - 1;
          if (shift >= 0 && shift <= 7) {
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
  DCHECK_LE(number_of_loss_fields, kRtcpMaxCastLossFields);
  (*packet)[cast_size_pos] = static_cast<uint8>(4 + number_of_loss_fields);
  (*packet)[cast_loss_field_pos] = static_cast<uint8>(number_of_loss_fields);
}

void RtcpSender::BuildSenderLog(RtcpSenderLogMessage* sender_log_message,
                                std::vector<uint8>* packet) const {
  DCHECK(sender_log_message);
  DCHECK(packet);
  size_t start_size = packet->size();
  size_t remaining_space = kIpPacketSize - start_size;
  DCHECK_GE(remaining_space, kRtcpCastLogHeaderSize + kRtcpSenderFrameLogSize)
       << "Not enough buffer space";
  if (remaining_space < kRtcpCastLogHeaderSize + kRtcpSenderFrameLogSize)
    return;

  size_t space_for_x_messages =
      (remaining_space - kRtcpCastLogHeaderSize) / kRtcpSenderFrameLogSize;
  size_t number_of_messages = std::min(space_for_x_messages,
                                       sender_log_message->size());

  size_t log_size = kRtcpCastLogHeaderSize +
      number_of_messages * kRtcpSenderFrameLogSize;
  packet->resize(start_size + log_size);

  net::BigEndianWriter big_endian_writer(&((*packet)[start_size]), log_size);
  big_endian_writer.WriteU8(0x80 + kSenderLogSubtype);
  big_endian_writer.WriteU8(kPacketTypeApplicationDefined);
  big_endian_writer.WriteU16(static_cast<uint16>(2 + number_of_messages));
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(kCast);

  for (; number_of_messages > 0; --number_of_messages) {
    DCHECK(!sender_log_message->empty());
    const RtcpSenderFrameLogMessage& message = sender_log_message->front();
    big_endian_writer.WriteU8(static_cast<uint8>(message.frame_status));
    // We send the 24 east significant bits of the RTP timestamp.
    big_endian_writer.WriteU8(static_cast<uint8>(message.rtp_timestamp >> 16));
    big_endian_writer.WriteU8(static_cast<uint8>(message.rtp_timestamp >> 8));
    big_endian_writer.WriteU8(static_cast<uint8>(message.rtp_timestamp));
    sender_log_message->pop_front();
  }
}

void RtcpSender::BuildReceiverLog(RtcpReceiverLogMessage* receiver_log_message,
                                  std::vector<uint8>* packet) const {
  DCHECK(receiver_log_message);
  const size_t packet_start_size = packet->size();
  size_t number_of_frames = 0;
  size_t total_number_of_messages_to_send = 0;
  size_t rtcp_log_size = 0;

  if (!ScanRtcpReceiverLogMessage(*receiver_log_message,
                                  packet_start_size,
                                  &number_of_frames,
                                  &total_number_of_messages_to_send,
                                  &rtcp_log_size)) {
    return;
  }
  packet->resize(packet_start_size + rtcp_log_size);

  net::BigEndianWriter big_endian_writer(&((*packet)[packet_start_size]),
                                         rtcp_log_size);
  big_endian_writer.WriteU8(0x80 + kReceiverLogSubtype);
  big_endian_writer.WriteU8(kPacketTypeApplicationDefined);
  big_endian_writer.WriteU16(static_cast<uint16>(2 + 2 * number_of_frames +
                             total_number_of_messages_to_send));
  big_endian_writer.WriteU32(ssrc_);  // Add our own SSRC.
  big_endian_writer.WriteU32(kCast);

  while (!receiver_log_message->empty() &&
         total_number_of_messages_to_send > 0) {
    RtcpReceiverFrameLogMessage& frame_log_messages =
        receiver_log_message->front();
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
          MergeEventTypeAndTimestampForWireFormat(event_message.type,
          event_message.event_timestamp - event_timestamp_base);
      switch (event_message.type) {
        case kAckSent:
        case kAudioPlayoutDelay:
        case kAudioFrameDecoded:
        case kVideoFrameDecoded:
        case kVideoRenderDelay:
          big_endian_writer.WriteU16(static_cast<uint16>(
              event_message.delay_delta.InMilliseconds()));
          big_endian_writer.WriteU16(event_type_and_timestamp_delta);
          break;
        case kPacketReceived:
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
      receiver_log_message->pop_front();
    }
  }
  DCHECK_EQ(total_number_of_messages_to_send, 0);
}

}  // namespace cast
}  // namespace media
