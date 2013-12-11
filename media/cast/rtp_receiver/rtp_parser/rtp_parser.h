// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_RTP_RECEIVER_RTP_PARSER_RTP_PARSER_H_
#define MEDIA_CAST_RTP_RECEIVER_RTP_PARSER_RTP_PARSER_H_

#include "media/cast/net/cast_net_defines.h"
#include "media/cast/rtp_receiver/rtp_receiver_defines.h"

namespace media {
namespace cast {

class RtpData;

struct RtpParserConfig {
  RtpParserConfig() {
    ssrc = 0;
    payload_type = 0;
    audio_channels = 0;
  }

  uint32 ssrc;
  int payload_type;
  AudioCodec audio_codec;
  VideoCodec video_codec;
  int audio_channels;
};

class RtpParser {
 public:
  RtpParser(RtpData* incoming_payload_callback,
            const RtpParserConfig parser_config);

  ~RtpParser();

  bool ParsePacket(const uint8* packet, size_t length,
                   RtpCastHeader* rtp_header);

 private:
  bool ParseCommon(const uint8* packet, size_t length,
                   RtpCastHeader* rtp_header);

  bool ParseCast(const uint8* packet, size_t length,
                 RtpCastHeader* rtp_header);

  RtpData* data_callback_;
  RtpParserConfig parser_config_;
  FrameIdWrapHelper frame_id_wrap_helper_;
  FrameIdWrapHelper reference_frame_id_wrap_helper_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_RTP_RECEIVER_RTP_PARSER_RTP_PARSER_H_
