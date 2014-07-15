// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/rtp/rtp_header_parser.h"

#include <cstddef>

#include "base/big_endian.h"

namespace media {
namespace cast {

static const uint8 kCastKeyFrameBitMask = 0x80;
static const uint8 kCastReferenceFrameIdBitMask = 0x40;
static const size_t kRtpCommonHeaderLength = 12;
static const size_t kRtpCastHeaderLength = 12;

RtpCastTestHeader::RtpCastTestHeader()
    : is_key_frame(false),
      frame_id(0),
      packet_id(0),
      max_packet_id(0),
      is_reference(false),
      reference_frame_id(0),
      marker(false),
      sequence_number(0),
      rtp_timestamp(0),
      ssrc(0),
      payload_type(0),
      num_csrcs(0),
      audio_num_energy(0),
      header_length(0) {}

RtpCastTestHeader::~RtpCastTestHeader() {}

RtpHeaderParser::RtpHeaderParser(const uint8* rtp_data, size_t rtp_data_length)
    : rtp_data_begin_(rtp_data), length_(rtp_data_length) {}

RtpHeaderParser::~RtpHeaderParser() {}

bool RtpHeaderParser::Parse(RtpCastTestHeader* parsed_packet) const {
  if (length_ < kRtpCommonHeaderLength + kRtpCastHeaderLength)
    return false;
  if (!ParseCommon(parsed_packet))
    return false;
  return ParseCast(parsed_packet);
}

bool RtpHeaderParser::ParseCommon(RtpCastTestHeader* parsed_packet) const {
  const uint8 version = rtp_data_begin_[0] >> 6;
  if (version != 2) {
    return false;
  }

  const uint8 num_csrcs = rtp_data_begin_[0] & 0x0f;
  const bool marker = ((rtp_data_begin_[1] & 0x80) == 0) ? false : true;
  const uint8 payload_type = rtp_data_begin_[1] & 0x7f;
  const uint16 sequence_number = (rtp_data_begin_[2] << 8) + rtp_data_begin_[3];

  const uint8* ptr = &rtp_data_begin_[4];

  base::BigEndianReader big_endian_reader(reinterpret_cast<const char*>(ptr),
                                          8);
  uint32 rtp_timestamp, ssrc;
  big_endian_reader.ReadU32(&rtp_timestamp);
  big_endian_reader.ReadU32(&ssrc);

  const uint8 csrc_octs = num_csrcs * 4;

  parsed_packet->marker = marker;
  parsed_packet->payload_type = payload_type;
  parsed_packet->sequence_number = sequence_number;
  parsed_packet->rtp_timestamp = rtp_timestamp;
  parsed_packet->ssrc = ssrc;
  parsed_packet->num_csrcs = num_csrcs;

  parsed_packet->audio_num_energy = parsed_packet->num_csrcs;

  parsed_packet->header_length = 12 + csrc_octs;
  return true;
}

bool RtpHeaderParser::ParseCast(RtpCastTestHeader* parsed_packet) const {
  const uint8* data = rtp_data_begin_ + kRtpCommonHeaderLength;
  parsed_packet->is_key_frame = (data[0] & kCastKeyFrameBitMask);
  parsed_packet->is_reference = (data[0] & kCastReferenceFrameIdBitMask);
  parsed_packet->frame_id = frame_id_wrap_helper_.MapTo32bitsFrameId(data[1]);

  base::BigEndianReader big_endian_reader(
      reinterpret_cast<const char*>(data + 2), 8);
  big_endian_reader.ReadU16(&parsed_packet->packet_id);
  big_endian_reader.ReadU16(&parsed_packet->max_packet_id);

  if (parsed_packet->is_reference) {
    parsed_packet->reference_frame_id =
        reference_frame_id_wrap_helper_.MapTo32bitsFrameId(data[6]);
  }
  return true;
}

}  // namespace cast
}  // namespace media
