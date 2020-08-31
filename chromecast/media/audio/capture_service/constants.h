// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_
#define CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_

#include <cstdint>

namespace chromecast {
namespace media {
namespace capture_service {

constexpr char kDefaultUnixDomainSocketPath[] = "/tmp/capture-service";
constexpr int kDefaultTcpPort = 12855;

enum class SampleFormat : uint8_t {
  INTERLEAVED_INT16 = 0,
  INTERLEAVED_INT32 = 1,
  INTERLEAVED_FLOAT = 2,
  PLANAR_INT16 = 3,
  PLANAR_INT32 = 4,
  PLANAR_FLOAT = 5,
  LAST_FORMAT = PLANAR_FLOAT,
};

enum class StreamType : uint8_t {
  // Raw microphone capture from ALSA or other platform interface.
  kMicRaw = 0,
  // Echo cancelled capture using software AEC.
  kSoftwareEchoCancelled,
  // Echo linearly removed capture using software eraser that has lower cost but
  // will have some non-linear echo residual left.
  kSoftwareEchoCancelledLinear,
  // Hardware echo cancelled capture, e.g., from DSP.
  kHardwareEchoCancelled,
  // Echo rescaled capture that balances the volume of both far-end and near-end
  // captures. The far-end sound is the echo voice that travels out from the
  // loudspeaker and then is picked up by the system microphone, whereas the
  // near-end sound is the remaining capture sound without the echo voice.
  kEchoRescaled,
  // Mark the last type.
  kLastType = kEchoRescaled,
};

enum class MessageType : uint8_t {
  // Acknowledge message that has stream header but empty body. It is used by
  // receiver notifying the stream it is observing, and sender can confirm the
  // parameters are all correct.
  kAck = 0,
  // Audio message that has stream header and audio data in the message body.
  // The audio data will match the parameters in the header.
  kAudio,
  // Metadata message that doesn't have stream header but a serialized proto
  // data besides the type bits.
  kMetadata,
};

struct StreamInfo {
  StreamType stream_type;
  int num_channels = 0;
  SampleFormat sample_format;
  int sample_rate = 0;
  int frames_per_buffer = 0;
};

// Info describes the message packet. |timestamp_us| is about when the buffer is
// captured. If the audio source is from ALSA, i.e., stream type is raw mic,
// it's the ALSA capture timestamp; otherwise, it may be shifted based on the
// samples and sample rate upon raw mic input.
struct PacketInfo {
  MessageType message_type;
  StreamInfo stream_info;
  int64_t timestamp_us = 0;
};

}  // namespace capture_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_CAPTURE_SERVICE_CONSTANTS_H_
