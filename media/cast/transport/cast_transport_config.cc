// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/transport/cast_transport_config.h"

namespace media {
namespace cast {
namespace transport {

namespace {
const int kDefaultRtpHistoryMs = 1000;
const int kDefaultRtpMaxDelayMs = 100;
}  // namespace

CastTransportConfig::CastTransportConfig()
    : receive_port(0),
      send_port(0),
      audio_ssrc(0),
      video_ssrc(0),
      video_codec(kVp8),
      audio_codec(kOpus),
      audio_rtp_history_ms(kDefaultRtpHistoryMs),
      video_rtp_history_ms(kDefaultRtpHistoryMs),
      audio_rtp_max_delay_ms(kDefaultRtpMaxDelayMs),
      video_rtp_max_delay_ms(kDefaultRtpMaxDelayMs),
      audio_rtp_payload_type(0),
      video_rtp_payload_type(0),
      audio_frequency(0),
      audio_channels(0),
      aes_key (""),
      aes_iv_mask("") {}

CastTransportConfig::~CastTransportConfig() {}

EncodedVideoFrame::EncodedVideoFrame() {}
EncodedVideoFrame::~EncodedVideoFrame() {}

EncodedAudioFrame::EncodedAudioFrame() {}
EncodedAudioFrame::~EncodedAudioFrame() {}

RtcpSenderFrameLogMessage::RtcpSenderFrameLogMessage ()
    : frame_status(kRtcpSenderFrameStatusUnknown),
      rtp_timestamp(0) {}
RtcpSenderFrameLogMessage::~RtcpSenderFrameLogMessage () {}

RtcpSenderInfo::RtcpSenderInfo()
    : ntp_seconds(0),
      ntp_fraction(0),
      rtp_timestamp(0),
      send_packet_count(0),
      send_octet_count(0) {}
RtcpSenderInfo::~RtcpSenderInfo() {}

RtcpReportBlock::RtcpReportBlock()
    : remote_ssrc(0),
      media_ssrc(0),
      fraction_lost(0),
      cumulative_lost(0),
      extended_high_sequence_number(0),
      jitter(0),
      last_sr(0),
      delay_since_last_sr(0) {}
RtcpReportBlock::~RtcpReportBlock() {}

RtcpDlrrReportBlock::RtcpDlrrReportBlock()
    : last_rr(0),
      delay_since_last_rr(0) {}
RtcpDlrrReportBlock::~RtcpDlrrReportBlock() {}

}  // namespace transport
}  // namespace cast
}  // namespace media
