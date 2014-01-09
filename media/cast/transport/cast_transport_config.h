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

namespace media {
namespace cast {
namespace transport {

enum RtcpMode {
  kRtcpCompound,  // Compound RTCP mode is described by RFC 4585.
  kRtcpReducedSize,  // Reduced-size RTCP mode is described by RFC 5506.
};

enum VideoCodec {
  kVp8,
  kH264,
};

enum AudioCodec {
  kOpus,
  kPcm16,
  kExternalAudio,
};

struct CastTransportConfig {
  CastTransportConfig();
  ~CastTransportConfig();
  uint32 sender_ssrc;

  VideoCodec video_codec;
  AudioCodec audio_codec;

  int rtp_history_ms;
  int rtp_max_delay_ms;
  int rtp_payload_type;

  int frequency;
  int channels;

  std::string aes_key;  // Binary string of size kAesKeySize.
  std::string aes_iv_mask;  // Binary string of size kAesBlockSize.
};

struct EncodedVideoFrame {
  EncodedVideoFrame();
  ~EncodedVideoFrame();

  VideoCodec codec;
  bool key_frame;
  uint32 frame_id;
  uint32 last_referenced_frame_id;
  std::string data;
};

struct EncodedAudioFrame {
  EncodedAudioFrame();
  ~EncodedAudioFrame();

  AudioCodec codec;
  uint32 frame_id;  // Needed to release the frame.
  int samples;  // Needed send side to advance the RTP timestamp.
                // Not used receive side.
  // Support for max sampling rate of 48KHz, 2 channels, 100 ms duration.
  static const int kMaxNumberOfSamples = 48 * 2 * 100;
  std::string data;
};

typedef std::vector<uint8> Packet;
typedef std::vector<Packet> PacketList;

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

}  // namespace transport
}  // namespace cast
}  // namespace media

#endif // MEDIA_CAST_TRANSPORT_CAST_TRANSPORT_CONFIG_H_
