// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_CONFIG_H_
#define MEDIA_CAST_CAST_CONFIG_H_

#include <list>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/cast/cast_defines.h"
#include "media/cast/transport/cast_transport_config.h"

namespace media {
namespace cast {

enum RtcpMode {
  kRtcpCompound,  // Compound RTCP mode is described by RFC 4585.
  kRtcpReducedSize,  // Reduced-size RTCP mode is described by RFC 5506.
};

struct AudioSenderConfig {
  AudioSenderConfig();

  uint32 sender_ssrc;
  uint32 incoming_feedback_ssrc;

  int rtcp_interval;
  std::string rtcp_c_name;
  RtcpMode rtcp_mode;

  int rtp_history_ms;  // The time RTP packets are stored for retransmissions.
  int rtp_max_delay_ms;
  int rtp_payload_type;

  bool use_external_encoder;
  int frequency;
  int channels;
  int bitrate;  // Set to <= 0 for "auto variable bitrate" (libopus knows best).
  transport::AudioCodec codec;

  std::string aes_key;  // Binary string of size kAesKeySize.
  std::string aes_iv_mask;  // Binary string of size kAesKeySize.
};

struct VideoSenderConfig {
  VideoSenderConfig();

  uint32 sender_ssrc;
  uint32 incoming_feedback_ssrc;

  int rtcp_interval;
  std::string rtcp_c_name;
  RtcpMode rtcp_mode;

  int rtp_history_ms;  // The time RTP packets are stored for retransmissions.
  int rtp_max_delay_ms;
  int rtp_payload_type;

  bool use_external_encoder;
  int width;  // Incoming frames will be scaled to this size.
  int height;

  float congestion_control_back_off;
  int max_bitrate;
  int min_bitrate;
  int start_bitrate;
  int max_qp;
  int min_qp;
  int max_frame_rate;
  int max_number_of_video_buffers_used;  // Max value depend on codec.
  transport::VideoCodec codec;
  int number_of_cores;

  std::string aes_key;  // Binary string of size kAesKeySize.
  std::string aes_iv_mask;  // Binary string of size kAesKeySize.
};

struct AudioReceiverConfig {
  AudioReceiverConfig();

  uint32 feedback_ssrc;
  uint32 incoming_ssrc;

  int rtcp_interval;
  std::string rtcp_c_name;
  RtcpMode rtcp_mode;

  // The time the receiver is prepared to wait for retransmissions.
  int rtp_max_delay_ms;
  int rtp_payload_type;

  bool use_external_decoder;
  int frequency;
  int channels;
  transport::AudioCodec codec;

  std::string aes_key;  // Binary string of size kAesKeySize.
  std::string aes_iv_mask;  // Binary string of size kAesKeySize.
};

struct VideoReceiverConfig {
  VideoReceiverConfig();

  uint32 feedback_ssrc;
  uint32 incoming_ssrc;

  int rtcp_interval;
  std::string rtcp_c_name;
  RtcpMode rtcp_mode;

  // The time the receiver is prepared to wait for retransmissions.
  int rtp_max_delay_ms;
  int rtp_payload_type;

  bool use_external_decoder;
  int max_frame_rate;

  // Some HW decoders can not run faster than the frame rate, preventing it
  // from catching up after a glitch.
  bool decoder_faster_than_max_frame_rate;
  transport::VideoCodec codec;

  std::string aes_key;  // Binary string of size kAesKeySize.
  std::string aes_iv_mask;  // Binary string of size kAesKeySize.
};

// DEPRECATED: Do not use in new code.  Please migrate existing code to use
// media::AudioBus.
struct PcmAudioFrame {
  PcmAudioFrame();
  ~PcmAudioFrame();

  int channels;  // Samples in interleaved stereo format. L0, R0, L1 ,R1 ,...
  int frequency;
  std::vector<int16> samples;
};

typedef std::vector<uint8> Packet;
typedef std::vector<Packet> PacketList;

class PacketSender {
 public:
  // All packets to be sent to the network will be delivered via these
  // functions.
  virtual bool SendPackets(const PacketList& packets) = 0;

  virtual bool SendPacket(const Packet& packet) = 0;

  virtual ~PacketSender() {}
};

class PacketReceiver : public base::RefCountedThreadSafe<PacketReceiver> {
 public:
  // All packets received from the network should be delivered via this
  // function.
  virtual void ReceivedPacket(const uint8* packet, size_t length,
                              const base::Closure callback) = 0;

  static void DeletePacket(const uint8* packet);

 protected:
  virtual ~PacketReceiver() {}

 private:
  friend class base::RefCountedThreadSafe<PacketReceiver>;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_CONFIG_H_
