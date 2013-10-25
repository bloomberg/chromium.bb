// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MEDIA_CAST_SEND_TRANSPORT_H_
#define CHROME_RENDERER_MEDIA_CAST_SEND_TRANSPORT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace WebKit {
class WebMediaStreamTrack;
}  // namespace WebKit

class CastSession;
class CastUdpTransport;

// A key value pair structure for codec specific parameters.
struct CastCodecSpecificParam {
  std::string key;
  std::string value;

  CastCodecSpecificParam();
  ~CastCodecSpecificParam();
};

// Defines the basic properties of a payload supported by cast transport.
struct CastRtpPayloadParam {
  // RTP specific field that identifies the content type.
  int payload_type;

  // RTP specific field to identify a stream.
  int ssrc;

  // Update frequency of payload sample.
  int clock_rate;

  // Uncompressed bitrate.
  int bitrate;

  // Number of audio channels.
  int channels;

  // Width and height of the video content.
  int width;
  int height;

  // Name of the codec used.
  std::string codec_name;

  // List of codec specific parameters.
  std::vector<CastCodecSpecificParam> codec_specific_params;

  CastRtpPayloadParam();
  ~CastRtpPayloadParam();
};

// Defines the capabilities of the transport.
struct CastRtpCaps {
  // Defines a list of supported payloads.
  std::vector<CastRtpPayloadParam> payloads;

  // Names of supported RTCP features.
  std::vector<std::string> rtcp_features;

  // Names of supported FEC (Forward Error Correction) mechanisms.
  std::vector<std::string> fec_mechanism;

  CastRtpCaps();
  ~CastRtpCaps();
};

typedef CastRtpCaps CastRtpParams;

// This class takes input from audio and/or video WebMediaStreamTracks
// and then send the encoded streams to the underlying transport,
// e.g. a UDP transport. It also allows configuration of the encoded
// stream.
class CastSendTransport {
 public:
  explicit CastSendTransport(CastUdpTransport* udp_transport);
  ~CastSendTransport();

  // Return capabilities currently spported by this transport.
  CastRtpCaps GetCaps();

  // Return parameters set to this transport.
  CastRtpParams GetParams();

  // Return the best parameters given the capabilities of remote peer.
  CastRtpParams CreateParams(CastRtpCaps remote_caps);

  // Begin encoding of media stream from |audio_track| and |video_track|
  // and then submit the encoded streams to underlying transport.
  // Either stream can be NULL but it is invalid for both streams to be
  // NULL.
  void Start(WebKit::WebMediaStreamTrack* audio_track,
             WebKit::WebMediaStreamTrack* video_track,
             CastRtpParams params);

  // Stop encoding.
  void Stop();

 private:
  const scoped_refptr<CastSession> cast_session_;

  DISALLOW_COPY_AND_ASSIGN(CastSendTransport);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_SEND_TRANSPORT_H_
