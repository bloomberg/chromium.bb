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
  RtpHeaderParser(const uint8* rtpData,
                  const uint32 rtpDataLength);
  ~RtpHeaderParser();

  bool Parse(RtpCastHeader* parsed_packet) const;
 private:
  const uint8* const rtp_data_begin_;
  const uint8* const rtp_data_end_;

  DISALLOW_COPY_AND_ASSIGN(RtpHeaderParser);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_SENDER_RTP_PACKETIZER_TEST_RTP_HEADER_PARSER_H_
