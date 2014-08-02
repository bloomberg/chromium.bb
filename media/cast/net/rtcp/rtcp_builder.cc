// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/rtcp_builder.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/big_endian.h"
#include "base/logging.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/pacing/paced_sender.h"

namespace media {
namespace cast {

RtcpBuilder::RtcpBuilder(PacedSender* const outgoing_transport)
    : transport_(outgoing_transport),
      ssrc_(0) {
}

RtcpBuilder::~RtcpBuilder() {}

void RtcpBuilder::SendRtcpFromRtpSender(
    uint32 packet_type_flags,
    const RtcpSenderInfo& sender_info,
    const RtcpDlrrReportBlock& dlrr,
    uint32 sending_ssrc) {
  if (packet_type_flags & kRtcpRr ||
      packet_type_flags & kRtcpRrtr ||
      packet_type_flags & kRtcpCast ||
      packet_type_flags & kRtcpReceiverLog ||
      packet_type_flags & kRtcpNack) {
    NOTREACHED() << "Invalid argument";
  }
  ssrc_ = sending_ssrc;
  PacketRef packet(new base::RefCountedData<Packet>);
  packet->data.reserve(kMaxIpPacketSize);
  if (packet_type_flags & kRtcpSr) {
    if (!BuildSR(sender_info, &packet->data)) return;
    if (!BuildSdec(&packet->data)) return;
  }
  if (packet_type_flags & kRtcpDlrr) {
    if (!BuildDlrrRb(dlrr, &packet->data)) return;
  }
  if (packet->data.empty())
    return;  // Sanity - don't send empty packets.

  transport_->SendRtcpPacket(ssrc_, packet);
}

bool RtcpBuilder::BuildSR(const RtcpSenderInfo& sender_info,
                          Packet* packet) const {
  // Sender report.
  size_t start_size = packet->size();
  if (start_size + 52 > kMaxIpPacketSize) {
    DLOG(FATAL) << "Not enough buffer space";
    return false;
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
  return true;
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
bool RtcpBuilder::BuildDlrrRb(const RtcpDlrrReportBlock& dlrr,
                              Packet* packet) const {
  size_t start_size = packet->size();
  if (start_size + 24 > kMaxIpPacketSize) {
    DLOG(FATAL) << "Not enough buffer space";
    return false;
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
  return true;
}

}  // namespace cast
}  // namespace media
