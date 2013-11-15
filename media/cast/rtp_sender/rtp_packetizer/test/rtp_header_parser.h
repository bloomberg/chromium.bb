// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility parser for rtp packetizer unittests
#ifndef MEDIA_CAST_RTP_SENDER_RTP_PACKETIZER_TEST_RTP_HEADER_PARSER_H_
#define MEDIA_CAST_RTP_SENDER_RTP_PACKETIZER_TEST_RTP_HEADER_PARSER_H_

#include "media/cast/rtp_common/rtp_defines.h"

namespace media {
namespace cast {

class RtpHeaderParser {
 public:
  RtpHeaderParser(const uint8* rtpData, size_t rtpDataLength);
  ~RtpHeaderParser();

  bool Parse(RtpCastHeader* parsed_packet) const;
 private:
  bool ParseCommon(RtpCastHeader* parsed_packet) const;
  bool ParseCast(RtpCastHeader* parsed_packet) const;
  const uint8* const rtp_data_begin_;
  size_t length_;

  mutable FrameIdWrapHelper frame_id_wrap_helper_;
  mutable FrameIdWrapHelper reference_frame_id_wrap_helper_;

  DISALLOW_COPY_AND_ASSIGN(RtpHeaderParser);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_SENDER_RTP_PACKETIZER_TEST_RTP_HEADER_PARSER_H_
