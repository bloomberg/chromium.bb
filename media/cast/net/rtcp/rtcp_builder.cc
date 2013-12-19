// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/rtcp_builder.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "media/cast/net/cast_net_defines.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/rtcp/rtcp_utility.h"
#include "net/base/big_endian.h"

static const size_t kRtcpCastLogHeaderSize = 12;
static const size_t kRtcpSenderFrameLogSize = 4;
static const size_t kRtcpReceiverFrameLogSize = 8;
static const size_t kRtcpReceiverEventLogSize = 4;

namespace media {
namespace cast {

RtcpBuilder::RtcpBuilder(PacedPacketSender* outgoing_transport,
                             uint32 sending_ssrc,
                             const std::string& c_name)
     : ssrc_(sending_ssrc),
       c_name_(c_name),
       transport_(outgoing_transport) {
  DCHECK_LT(c_name_.length(), kRtcpCnameSize) << "Invalid config";
}

RtcpBuilder::~RtcpBuilder() {}

void RtcpBuilder::SendRtcpFromRtpSender(uint32 packet_type_flags,
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

void RtcpBuilder::BuildSR(const RtcpSenderInfo& sender_info,
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

void RtcpBuilder::AddReportBlocks(const RtcpReportBlock& report_block,
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

void RtcpBuilder::BuildSdec(std::vector<uint8>* packet) const {
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

void RtcpBuilder::BuildBye(std::vector<uint8>* packet) const {
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
void RtcpBuilder::BuildDlrrRb(const RtcpDlrrReportBlock* dlrr,
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

void RtcpBuilder::BuildSenderLog(RtcpSenderLogMessage* sender_log_message,
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

}  // namespace cast
}  // namespace media
