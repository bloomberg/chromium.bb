// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtcp/test_rtcp_packet_builder.h"

#include "base/logging.h"
#include "media/cast/net/rtcp/rtcp_utility.h"

namespace media {
namespace cast {

TestRtcpPacketBuilder::TestRtcpPacketBuilder()
    : ptr_of_length_(NULL),
      big_endian_writer_(reinterpret_cast<char*>(buffer_), kMaxIpPacketSize),
      big_endian_reader_(NULL, 0) {}

void TestRtcpPacketBuilder::AddSr(uint32_t sender_ssrc,
                                  int number_of_report_blocks) {
  AddRtcpHeader(200, number_of_report_blocks);
  big_endian_writer_.WriteU32(sender_ssrc);
  big_endian_writer_.WriteU32(kNtpHigh);  // NTP timestamp.
  big_endian_writer_.WriteU32(kNtpLow);
  big_endian_writer_.WriteU32(kRtpTimestamp);
  big_endian_writer_.WriteU32(kSendPacketCount);
  big_endian_writer_.WriteU32(kSendOctetCount);
}

void TestRtcpPacketBuilder::AddSrWithNtp(uint32_t sender_ssrc,
                                         uint32_t ntp_high,
                                         uint32_t ntp_low,
                                         uint32_t rtp_timestamp) {
  AddRtcpHeader(200, 0);
  big_endian_writer_.WriteU32(sender_ssrc);
  big_endian_writer_.WriteU32(ntp_high);
  big_endian_writer_.WriteU32(ntp_low);
  big_endian_writer_.WriteU32(rtp_timestamp);
  big_endian_writer_.WriteU32(kSendPacketCount);
  big_endian_writer_.WriteU32(kSendOctetCount);
}

void TestRtcpPacketBuilder::AddRr(uint32_t sender_ssrc,
                                  int number_of_report_blocks) {
  AddRtcpHeader(201, number_of_report_blocks);
  big_endian_writer_.WriteU32(sender_ssrc);
}

void TestRtcpPacketBuilder::AddRb(uint32_t rtp_ssrc) {
  big_endian_writer_.WriteU32(rtp_ssrc);
  big_endian_writer_.WriteU32(kLoss);
  big_endian_writer_.WriteU32(kExtendedMax);
  big_endian_writer_.WriteU32(kTestJitter);
  big_endian_writer_.WriteU32(kLastSr);
  big_endian_writer_.WriteU32(kDelayLastSr);
}

void TestRtcpPacketBuilder::AddXrHeader(uint32_t sender_ssrc) {
  AddRtcpHeader(207, 0);
  big_endian_writer_.WriteU32(sender_ssrc);
}

void TestRtcpPacketBuilder::AddXrUnknownBlock() {
  big_endian_writer_.WriteU8(9);   // Block type.
  big_endian_writer_.WriteU8(0);   // Reserved.
  big_endian_writer_.WriteU16(4);  // Block length.
  // First receiver same as sender of this report.
  big_endian_writer_.WriteU32(0);
  big_endian_writer_.WriteU32(0);
  big_endian_writer_.WriteU32(0);
  big_endian_writer_.WriteU32(0);
}

void TestRtcpPacketBuilder::AddUnknownBlock() {
  AddRtcpHeader(99, 0);
  big_endian_writer_.WriteU32(42);
  big_endian_writer_.WriteU32(42);
  big_endian_writer_.WriteU32(42);
}

void TestRtcpPacketBuilder::AddXrDlrrBlock(uint32_t sender_ssrc) {
  big_endian_writer_.WriteU8(5);   // Block type.
  big_endian_writer_.WriteU8(0);   // Reserved.
  big_endian_writer_.WriteU16(3);  // Block length.

  // First receiver same as sender of this report.
  big_endian_writer_.WriteU32(sender_ssrc);
  big_endian_writer_.WriteU32(kLastRr);
  big_endian_writer_.WriteU32(kDelayLastRr);
}

void TestRtcpPacketBuilder::AddXrExtendedDlrrBlock(uint32_t sender_ssrc) {
  big_endian_writer_.WriteU8(5);   // Block type.
  big_endian_writer_.WriteU8(0);   // Reserved.
  big_endian_writer_.WriteU16(9);  // Block length.
  big_endian_writer_.WriteU32(0xaaaaaaaa);
  big_endian_writer_.WriteU32(0xaaaaaaaa);
  big_endian_writer_.WriteU32(0xaaaaaaaa);

  // First receiver same as sender of this report.
  big_endian_writer_.WriteU32(sender_ssrc);
  big_endian_writer_.WriteU32(kLastRr);
  big_endian_writer_.WriteU32(kDelayLastRr);
  big_endian_writer_.WriteU32(0xbbbbbbbb);
  big_endian_writer_.WriteU32(0xbbbbbbbb);
  big_endian_writer_.WriteU32(0xbbbbbbbb);
}

void TestRtcpPacketBuilder::AddXrRrtrBlock() {
  big_endian_writer_.WriteU8(4);   // Block type.
  big_endian_writer_.WriteU8(0);   // Reserved.
  big_endian_writer_.WriteU16(2);  // Block length.
  big_endian_writer_.WriteU32(kNtpHigh);
  big_endian_writer_.WriteU32(kNtpLow);
}

void TestRtcpPacketBuilder::AddNack(uint32_t sender_ssrc, uint32_t media_ssrc) {
  AddRtcpHeader(205, 1);
  big_endian_writer_.WriteU32(sender_ssrc);
  big_endian_writer_.WriteU32(media_ssrc);
  big_endian_writer_.WriteU16(kMissingPacket);
  big_endian_writer_.WriteU16(0);
}

void TestRtcpPacketBuilder::AddSendReportRequest(uint32_t sender_ssrc,
                                                 uint32_t media_ssrc) {
  AddRtcpHeader(205, 5);
  big_endian_writer_.WriteU32(sender_ssrc);
  big_endian_writer_.WriteU32(media_ssrc);
}

void TestRtcpPacketBuilder::AddCast(uint32_t sender_ssrc,
                                    uint32_t media_ssrc,
                                    base::TimeDelta target_delay) {
  AddRtcpHeader(206, 15);
  big_endian_writer_.WriteU32(sender_ssrc);
  big_endian_writer_.WriteU32(media_ssrc);
  big_endian_writer_.WriteU8('C');
  big_endian_writer_.WriteU8('A');
  big_endian_writer_.WriteU8('S');
  big_endian_writer_.WriteU8('T');
  big_endian_writer_.WriteU8(kAckFrameId);
  big_endian_writer_.WriteU8(3);     // Loss fields.
  big_endian_writer_.WriteU16(target_delay.InMilliseconds());
  big_endian_writer_.WriteU8(kLostFrameId);
  big_endian_writer_.WriteU16(kRtcpCastAllPacketsLost);
  big_endian_writer_.WriteU8(0);  // Lost packet id mask.
  big_endian_writer_.WriteU8(kFrameIdWithLostPackets);
  big_endian_writer_.WriteU16(kLostPacketId1);
  big_endian_writer_.WriteU8(0x2);  // Lost packet id mask.
  big_endian_writer_.WriteU8(kFrameIdWithLostPackets);
  big_endian_writer_.WriteU16(kLostPacketId3);
  big_endian_writer_.WriteU8(0);  // Lost packet id mask.
}

void TestRtcpPacketBuilder::AddReceiverLog(uint32_t sender_ssrc) {
  AddRtcpHeader(204, 2);
  big_endian_writer_.WriteU32(sender_ssrc);
  big_endian_writer_.WriteU8('C');
  big_endian_writer_.WriteU8('A');
  big_endian_writer_.WriteU8('S');
  big_endian_writer_.WriteU8('T');
}

void TestRtcpPacketBuilder::AddReceiverFrameLog(uint32_t rtp_timestamp,
                                                int num_events,
                                                uint32_t event_timesamp_base) {
  big_endian_writer_.WriteU32(rtp_timestamp);
  big_endian_writer_.WriteU8(static_cast<uint8_t>(num_events - 1));
  big_endian_writer_.WriteU8(static_cast<uint8_t>(event_timesamp_base >> 16));
  big_endian_writer_.WriteU8(static_cast<uint8_t>(event_timesamp_base >> 8));
  big_endian_writer_.WriteU8(static_cast<uint8_t>(event_timesamp_base));
}

void TestRtcpPacketBuilder::AddReceiverEventLog(uint16_t event_data,
                                                CastLoggingEvent event,
                                                uint16_t event_timesamp_delta) {
  big_endian_writer_.WriteU16(event_data);
  uint8_t event_id = ConvertEventTypeToWireFormat(event);
  uint16_t type_and_delta = static_cast<uint16_t>(event_id) << 12;
  type_and_delta += event_timesamp_delta & 0x0fff;
  big_endian_writer_.WriteU16(type_and_delta);
}

scoped_ptr<media::cast::Packet> TestRtcpPacketBuilder::GetPacket() {
  PatchLengthField();
  return scoped_ptr<media::cast::Packet>(
      new media::cast::Packet(buffer_, buffer_ + Length()));
}

const uint8_t* TestRtcpPacketBuilder::Data() {
  PatchLengthField();
  return buffer_;
}

base::BigEndianReader* TestRtcpPacketBuilder::Reader() {
  big_endian_reader_ = base::BigEndianReader(
      reinterpret_cast<const char *>(Data()), Length());
  return &big_endian_reader_;
}

void TestRtcpPacketBuilder::PatchLengthField() {
  if (ptr_of_length_) {
    // Back-patch the packet length. The client must have taken
    // care of proper padding to 32-bit words.
    int this_packet_length = (big_endian_writer_.ptr() - ptr_of_length_ - 2);
    DCHECK_EQ(0, this_packet_length % 4)
        << "Packets must be a multiple of 32 bits long";
    *ptr_of_length_ = this_packet_length >> 10;
    *(ptr_of_length_ + 1) = (this_packet_length >> 2) & 0xFF;
    ptr_of_length_ = NULL;
  }
}

// Set the 5-bit value in the 1st byte of the header
// and the payload type. Set aside room for the length field,
// and make provision for back-patching it.
void TestRtcpPacketBuilder::AddRtcpHeader(int payload, int format_or_count) {
  PatchLengthField();
  big_endian_writer_.WriteU8(0x80 | (format_or_count & 0x1F));
  big_endian_writer_.WriteU8(payload);
  ptr_of_length_ = big_endian_writer_.ptr();

  // Initialize length to "clearly illegal".
  big_endian_writer_.WriteU16(0xDEAD);
}

}  // namespace cast
}  // namespace media
