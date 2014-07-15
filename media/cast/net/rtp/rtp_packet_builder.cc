// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/rtp_packet_builder.h"

#include "base/big_endian.h"
#include "base/logging.h"

namespace media {
namespace cast {

const uint32 kCastRtpHeaderLength = 7;
const uint32 kGenericRtpHeaderLength = 12;
static const uint8 kRtpMarkerBitMask = 0x80;

RtpPacketBuilder::RtpPacketBuilder()
    : is_key_(false),
      frame_id_(0),
      packet_id_(0),
      max_packet_id_(0),
      reference_frame_id_(0),
      timestamp_(0),
      sequence_number_(0),
      marker_(false),
      payload_type_(0),
      ssrc_(0) {}

void RtpPacketBuilder::SetKeyFrame(bool is_key) { is_key_ = is_key; }

void RtpPacketBuilder::SetFrameIds(uint32 frame_id, uint32 reference_frame_id) {
  frame_id_ = frame_id;
  reference_frame_id_ = reference_frame_id;
}

void RtpPacketBuilder::SetPacketId(uint16 packet_id) { packet_id_ = packet_id; }

void RtpPacketBuilder::SetMaxPacketId(uint16 max_packet_id) {
  max_packet_id_ = max_packet_id;
}

void RtpPacketBuilder::SetTimestamp(uint32 timestamp) {
  timestamp_ = timestamp;
}

void RtpPacketBuilder::SetSequenceNumber(uint16 sequence_number) {
  sequence_number_ = sequence_number;
}

void RtpPacketBuilder::SetMarkerBit(bool marker) { marker_ = marker; }

void RtpPacketBuilder::SetPayloadType(int payload_type) {
  payload_type_ = payload_type;
}

void RtpPacketBuilder::SetSsrc(uint32 ssrc) { ssrc_ = ssrc; }

void RtpPacketBuilder::BuildHeader(uint8* data, uint32 data_length) {
  BuildCommonHeader(data, data_length);
  BuildCastHeader(data + kGenericRtpHeaderLength,
                  data_length - kGenericRtpHeaderLength);
}

void RtpPacketBuilder::BuildCastHeader(uint8* data, uint32 data_length) {
  // Build header.
  DCHECK_LE(kCastRtpHeaderLength, data_length);
  // Set the first 7 bytes to 0.
  memset(data, 0, kCastRtpHeaderLength);
  base::BigEndianWriter big_endian_writer(reinterpret_cast<char*>(data), 56);
  const bool includes_specific_frame_reference =
      (is_key_ && (reference_frame_id_ != frame_id_)) ||
      (!is_key_ && (reference_frame_id_ != (frame_id_ - 1)));
  big_endian_writer.WriteU8((is_key_ ? 0x80 : 0) |
                            (includes_specific_frame_reference ? 0x40 : 0));
  big_endian_writer.WriteU8(frame_id_);
  big_endian_writer.WriteU16(packet_id_);
  big_endian_writer.WriteU16(max_packet_id_);
  if (includes_specific_frame_reference) {
    big_endian_writer.WriteU8(reference_frame_id_);
  }
}

void RtpPacketBuilder::BuildCommonHeader(uint8* data, uint32 data_length) {
  DCHECK_LE(kGenericRtpHeaderLength, data_length);
  base::BigEndianWriter big_endian_writer(reinterpret_cast<char*>(data), 96);
  big_endian_writer.WriteU8(0x80);
  big_endian_writer.WriteU8(payload_type_ | (marker_ ? kRtpMarkerBitMask : 0));
  big_endian_writer.WriteU16(sequence_number_);
  big_endian_writer.WriteU32(timestamp_);
  big_endian_writer.WriteU32(ssrc_);
}

}  // namespace cast
}  // namespace media
