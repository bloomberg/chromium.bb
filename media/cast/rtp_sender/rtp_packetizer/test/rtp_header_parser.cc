// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/rtp_sender/rtp_packetizer/test/rtp_header_parser.h"

#include <cstddef>

#include "net/base/big_endian.h"

namespace media {
namespace cast {

RtpHeaderParser::RtpHeaderParser(const uint8* rtp_data,
                                 const uint32 rtp_data_length)
  : rtp_data_begin_(rtp_data),
    rtp_data_end_(rtp_data ? (rtp_data + rtp_data_length) : NULL) {
}

RtpHeaderParser::~RtpHeaderParser() {}

bool RtpHeaderParser::Parse(RtpCastHeader* parsed_packet) const {
  const ptrdiff_t length = rtp_data_end_ - rtp_data_begin_;

  if (length < 12) {
    return false;
  }

  const uint8 version  = rtp_data_begin_[0] >> 6;
  if (version != 2) {
    return false;
  }

  const uint8 num_csrcs = rtp_data_begin_[0] & 0x0f;
  const bool marker = ((rtp_data_begin_[1] & 0x80) == 0) ? false : true;

  const uint8 payload_type = rtp_data_begin_[1] & 0x7f;

  const uint16 sequence_number = (rtp_data_begin_[2] << 8) +
      rtp_data_begin_[3];

  const uint8* ptr = &rtp_data_begin_[4];

  net::BigEndianReader big_endian_reader(ptr, 64);
  uint32 rtp_timestamp, ssrc;
  big_endian_reader.ReadU32(&rtp_timestamp);
  big_endian_reader.ReadU32(&ssrc);

  const uint8 csrc_octs = num_csrcs * 4;

  if ((ptr + csrc_octs) > rtp_data_end_) {
    return false;
  }

  parsed_packet->webrtc.header.markerBit      = marker;
  parsed_packet->webrtc.header.payloadType    = payload_type;
  parsed_packet->webrtc.header.sequenceNumber = sequence_number;
  parsed_packet->webrtc.header.timestamp      = rtp_timestamp;
  parsed_packet->webrtc.header.ssrc           = ssrc;
  parsed_packet->webrtc.header.numCSRCs       = num_csrcs;

  parsed_packet->webrtc.type.Audio.numEnergy =
      parsed_packet->webrtc.header.numCSRCs;

  parsed_packet->webrtc.header.headerLength   = 12 + csrc_octs;
  return true;
}

}  // namespace cast
}  // namespace media