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
  CastRtpParams CreateParams(const CastRtpCaps& remote_caps);

  // Begin encoding of media stream and then submit the encoded streams
  // to underlying transport.
  void Start(const CastRtpParams& params);

  // Stop encoding.
  void Stop();

 private:
  const scoped_refptr<CastSession> cast_session_;

  DISALLOW_COPY_AND_ASSIGN(CastSendTransport);
};

#endif  // CHROME_RENDERER_MEDIA_CAST_SEND_TRANSPORT_H_
