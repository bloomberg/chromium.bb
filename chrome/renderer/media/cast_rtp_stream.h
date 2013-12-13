// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_RTP_STREAM_H_
#define CHROME_RENDERER_MEDIA_CAST_RTP_STREAM_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"

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
  int payload_type;

  // RTP specific field to identify a stream.
  int ssrc;

  // Update frequency of payload sample.
  int clock_rate;

  // Maximum bitrate.
  int max_bitrate;

  // Minimum bitrate.
  int min_bitrate;

  // Number of audio channels.
  int channels;

  // Width and height of the video content.
  int width;
  int height;

  // Name of the codec used.
  std::string codec_name;

  // List of codec specific parameters.
  std::vector<CastCodecSpecificParams> codec_specific_params;

  CastRtpPayloadParams();
  ~CastRtpPayloadParams();
};

// Defines the capabilities of the transport.
struct CastRtpCaps {
  // Defines a list of supported payloads.
  std::vector<CastRtpPayloadParams> payloads;

  // Names of supported RTCP features.
  std::vector<std::string> rtcp_features;

  // Names of supported FEC (Forward Error Correction) mechanisms.
  std::vector<std::string> fec_mechanisms;

  CastRtpCaps();
  ~CastRtpCaps();
};

typedef CastRtpCaps CastRtpParams;

// This object represents a RTP stream that encodes and optionally
// encrypt audio or video data from a WebMediaStreamTrack.
// Note that this object does not actually output packets. It allows
// configuration of encoding and RTP parameters and control such a logical
// stream.
class CastRtpStream {
 public:
  CastRtpStream(const blink::WebMediaStreamTrack& track,
                const scoped_refptr<CastSession>& session);
  ~CastRtpStream();

  // Return capabilities currently supported by this transport.
  CastRtpCaps GetCaps();

  // Return parameters set to this transport.
  CastRtpParams GetParams();

  // Begin encoding of media stream and then submit the encoded streams
  // to underlying transport.
  void Start(const CastRtpParams& params);

  // Stop encoding.
  void Stop();

 private:
  // Return true if this track is an audio track. Return false if this
  // track is a video track.
  bool IsAudio() const;

  blink::WebMediaStreamTrack track_;
  const scoped_refptr<CastSession> cast_session_;
  scoped_ptr<CastAudioSink> audio_sink_;
  scoped_ptr<CastVideoSink> video_sink_;
  CastRtpParams params_;

  DISALLOW_COPY_AND_ASSIGN(CastRtpStream);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_RTP_STREAM_H_
