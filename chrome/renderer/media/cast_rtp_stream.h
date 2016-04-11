// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_RTP_STREAM_H_
#define CHROME_RENDERER_MEDIA_CAST_RTP_STREAM_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/constants.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

namespace base {
class BinaryValue;
class DictionaryValue;
}

class CastAudioSink;
class CastSession;
class CastVideoSink;

// A key value pair structure for codec specific parameters.
struct CastCodecSpecificParams {
  std::string key;
  std::string value;

  CastCodecSpecificParams();
  ~CastCodecSpecificParams();
};

// Defines the basic properties of a payload supported by cast transport.
struct CastRtpPayloadParams {
  // RTP specific field that identifies the content type.
  int payload_type = media::cast::kDefaultRtpVideoPayloadType;

  // Maximum latency in milliseconds. Implemetation tries to keep latency
  // under this threshold.
  int max_latency_ms = media::cast::kDefaultRtpMaxDelayMs;

  // Minimum latency.
  // Default value (0) means use max_latency_ms.
  int min_latency_ms = 0;

  // Starting latency on animated content.
  // Default value (0) means use max_latency_ms.
  int animated_latency_ms = 0;

  // RTP specific field to identify a stream.
  int ssrc = 1;

  // RTP specific field to idenfity the feedback stream.
  int feedback_ssrc = 2;

  // Update frequency of payload sample.
  int clock_rate = media::cast::kVideoFrequency;

  // Maximum bitrate in kilobits per second.
  int max_bitrate = media::cast::kDefaultMaxVideoKbps;

  // Minimum bitrate in kilobits per second.
  int min_bitrate = media::cast::kDefaultMinVideoKbps;

  // Number of audio channels.
  int channels = 1;

  // The maximum frame rate.
  double max_frame_rate = media::cast::kDefaultMaxFrameRate;

  // Name of the codec used.
  std::string codec_name;

  // AES encryption key.
  std::string aes_key;

  // AES encryption IV mask.
  std::string aes_iv_mask;

  // List of codec specific parameters.
  std::vector<CastCodecSpecificParams> codec_specific_params;

  CastRtpPayloadParams();
  CastRtpPayloadParams(const CastRtpPayloadParams& other);
  ~CastRtpPayloadParams();
};

// Defines the parameters of a RTP stream.
struct CastRtpParams {
  explicit CastRtpParams(const CastRtpPayloadParams& payload_params);

  // Payload parameters.
  CastRtpPayloadParams payload;

  // Names of supported RTCP features.
  std::vector<std::string> rtcp_features;

  CastRtpParams();
  CastRtpParams(const CastRtpParams& other);
  ~CastRtpParams();
};

// This object represents a RTP stream that encodes and optionally
// encrypt audio or video data from a WebMediaStreamTrack.
// Note that this object does not actually output packets. It allows
// configuration of encoding and RTP parameters and control such a logical
// stream.
class CastRtpStream {
 public:
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  CastRtpStream(const blink::WebMediaStreamTrack& track,
                const scoped_refptr<CastSession>& session);
  ~CastRtpStream();

  // Return parameters currently supported by this stream.
  std::vector<CastRtpParams> GetSupportedParams();

  // Return parameters set to this stream.
  CastRtpParams GetParams();

  // Begin encoding of media stream and then submit the encoded streams
  // to underlying transport.
  // When the stream is started |start_callback| is called.
  // When the stream is stopped |stop_callback| is called.
  // When there is an error |error_callback| is called with a message.
  void Start(const CastRtpParams& params,
             const base::Closure& start_callback,
             const base::Closure& stop_callback,
             const ErrorCallback& error_callback);

  // Stop encoding.
  void Stop();

  // Enables or disables logging for this stream.
  void ToggleLogging(bool enable);

  // Get serialized raw events for this stream with |extra_data| attached,
  // and invokes |callback| with the result.
  void GetRawEvents(
      const base::Callback<void(std::unique_ptr<base::BinaryValue>)>& callback,
      const std::string& extra_data);

  // Get stats in DictionaryValue format and invokves |callback| with
  // the result.
  void GetStats(const base::Callback<
                void(std::unique_ptr<base::DictionaryValue>)>& callback);

 private:
  // Return true if this track is an audio track. Return false if this
  // track is a video track.
  bool IsAudio() const;

  void DidEncounterError(const std::string& message);

  blink::WebMediaStreamTrack track_;
  const scoped_refptr<CastSession> cast_session_;
  std::unique_ptr<CastAudioSink> audio_sink_;
  std::unique_ptr<CastVideoSink> video_sink_;
  CastRtpParams params_;
  base::Closure stop_callback_;
  ErrorCallback error_callback_;

  base::WeakPtrFactory<CastRtpStream> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastRtpStream);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_RTP_STREAM_H_
