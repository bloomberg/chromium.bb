// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_CONFIG_H_
#define MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_CONFIG_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/cast/transport/cast_transport_defines.h"
#include "net/base/ip_endpoint.h"

namespace media {
namespace cast {
namespace transport {

enum RtcpMode {
  kRtcpCompound,     // Compound RTCP mode is described by RFC 4585.
  kRtcpReducedSize,  // Reduced-size RTCP mode is described by RFC 5506.
};

enum VideoCodec { kVp8, kH264, };

enum AudioCodec { kOpus, kPcm16, kExternalAudio, };

struct RtpConfig {
  RtpConfig();
  int history_ms;  // The time RTP packets are stored for retransmissions.
  int max_delay_ms;
  int payload_type;
};

struct CastTransportConfig {
  CastTransportConfig();
  ~CastTransportConfig();

  // Transport: Local receiver.
  net::IPEndPoint receiver_endpoint;
  net::IPEndPoint local_endpoint;

  uint32 audio_ssrc;
  uint32 video_ssrc;

  VideoCodec video_codec;
  AudioCodec audio_codec;

  // RTP.
  RtpConfig audio_rtp_config;
  RtpConfig video_rtp_config;

  int audio_frequency;
  int audio_channels;

  std::string aes_key;      // Binary string of size kAesKeySize.
  std::string aes_iv_mask;  // Binary string of size kAesBlockSize.
};

struct EncodedVideoFrame {
  EncodedVideoFrame();
  ~EncodedVideoFrame();

  VideoCodec codec;
  bool key_frame;
  uint32 frame_id;
  uint32 last_referenced_frame_id;
  uint32 rtp_timestamp;
  std::string data;
};

struct EncodedAudioFrame {
  EncodedAudioFrame();
  ~EncodedAudioFrame();

  AudioCodec codec;
  uint32 frame_id;  // Needed to release the frame.
  uint32 rtp_timestamp;
  // Support for max sampling rate of 48KHz, 2 channels, 100 ms duration.
  static const int kMaxNumberOfSamples = 48 * 2 * 100;
  std::string data;
};

typedef std::vector<uint8> Packet;
typedef std::vector<Packet> PacketList;

typedef base::Callback<void(scoped_ptr<Packet> packet)> PacketReceiverCallback;

class PacketSender {
 public:
  // All packets to be sent to the network will be delivered via these
  // functions.
  virtual bool SendPacket(const transport::Packet& packet) = 0;

  virtual ~PacketSender() {}
};

// Log messages form sender to receiver.
// TODO(mikhal): Refactor to Chromium style (MACRO_STYLE).
enum RtcpSenderFrameStatus {
  kRtcpSenderFrameStatusUnknown = 0,
  kRtcpSenderFrameStatusDroppedByEncoder = 1,
  kRtcpSenderFrameStatusDroppedByFlowControl = 2,
  kRtcpSenderFrameStatusSentToNetwork = 3,
};

struct RtcpSenderFrameLogMessage {
  RtcpSenderFrameLogMessage();
  ~RtcpSenderFrameLogMessage();
  RtcpSenderFrameStatus frame_status;
  uint32 rtp_timestamp;
};

typedef std::vector<RtcpSenderFrameLogMessage> RtcpSenderLogMessage;

struct RtcpSenderInfo {
  RtcpSenderInfo();
  ~RtcpSenderInfo();
  // First three members are used for lipsync.
  // First two members are used for rtt.
  uint32 ntp_seconds;
  uint32 ntp_fraction;
  uint32 rtp_timestamp;
  uint32 send_packet_count;
  size_t send_octet_count;
};

struct RtcpReportBlock {
  RtcpReportBlock();
  ~RtcpReportBlock();
  uint32 remote_ssrc;  // SSRC of sender of this report.
  uint32 media_ssrc;   // SSRC of the RTP packet sender.
  uint8 fraction_lost;
  uint32 cumulative_lost;  // 24 bits valid.
  uint32 extended_high_sequence_number;
  uint32 jitter;
  uint32 last_sr;
  uint32 delay_since_last_sr;
};

struct RtcpDlrrReportBlock {
  RtcpDlrrReportBlock();
  ~RtcpDlrrReportBlock();
  uint32 last_rr;
  uint32 delay_since_last_rr;
};

// This is only needed because IPC messages don't support more than
// 5 arguments.
struct SendRtcpFromRtpSenderData {
  SendRtcpFromRtpSenderData();
  ~SendRtcpFromRtpSenderData();
  uint32 packet_type_flags;
  uint32 sending_ssrc;
  std::string c_name;
};

inline bool operator==(RtcpSenderInfo lhs, RtcpSenderInfo rhs) {
  return lhs.ntp_seconds == rhs.ntp_seconds &&
         lhs.ntp_fraction == rhs.ntp_fraction &&
         lhs.rtp_timestamp == rhs.rtp_timestamp &&
         lhs.send_packet_count == rhs.send_packet_count &&
         lhs.send_octet_count == rhs.send_octet_count;
}

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_CONFIG_H_
