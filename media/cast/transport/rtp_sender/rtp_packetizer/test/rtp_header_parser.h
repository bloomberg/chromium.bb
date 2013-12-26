// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility parser for rtp packetizer unittests
#ifndef MEDIA_CAST_TRANSPORT_RTP_SENDER_RTP_PACKETIZER_TEST_RTP_HEADER_PARSER_H_
#define MEDIA_CAST_TRANSPORT_RTP_SENDER_RTP_PACKETIZER_TEST_RTP_HEADER_PARSER_H_

#include "base/basictypes.h"
#include "media/cast/transport/cast_transport_defines.h"

namespace media {
namespace cast {
namespace transport {

struct RtpCastTestHeader {
  RtpCastTestHeader();
  ~RtpCastTestHeader();
  // Cast specific.
  bool is_key_frame;
  uint32 frame_id;
  uint16 packet_id;
  uint16 max_packet_id;
  bool is_reference;  // Set to true if the previous frame is not available,
                      // and the reference frame id  is available.
  uint32 reference_frame_id;

  // Rtp Generic.
  bool marker;
  uint16 sequence_number;
  uint32 rtp_timestamp;
  uint32 ssrc;
  int payload_type;
  uint8 num_csrcs;
  uint8 audio_num_energy;
  int header_length;
};

class RtpHeaderParser {
 public:
  RtpHeaderParser(const uint8* rtpData, size_t rtpDataLength);
  ~RtpHeaderParser();

  bool Parse(RtpCastTestHeader* parsed_packet) const;

 private:
  bool ParseCommon(RtpCastTestHeader* parsed_packet) const;
  bool ParseCast(RtpCastTestHeader* parsed_packet) const;
  const uint8* const rtp_data_begin_;
  size_t length_;

  mutable transport::FrameIdWrapHelper frame_id_wrap_helper_;
  mutable transport::FrameIdWrapHelper reference_frame_id_wrap_helper_;

  DISALLOW_COPY_AND_ASSIGN(RtpHeaderParser);
};

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_RTP_SENDER_RTP_PACKETIZER_TEST_RTP_HEADER_PARSER_H_
