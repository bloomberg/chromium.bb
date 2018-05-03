// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_INTERFACE_H_
#define COMPONENTS_MIRRORING_SERVICE_INTERFACE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/values.h"
#include "media/capture/mojom/video_capture.mojom.h"
#include "media/cast/cast_config.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace mirroring {

// TODO(xjz): All interfaces defined in this file will be replaced with mojo
// interfaces.

// Errors occurred in a mirroring session.
enum SessionError {
  SESSION_START_ERROR,   // Error occurred while starting.
  AUDIO_CAPTURE_ERROR,   // Error occurred in audio capturing.
  VIDEO_CAPTURE_ERROR,   // Error occurred in video capturing.
  CAST_STREAMING_ERROR,  // Error occurred in cast streaming.
  CAST_TRANSPORT_ERROR,  // Error occurred in cast transport.
};

enum SessionType {
  AUDIO_ONLY,
  VIDEO_ONLY,
  AUDIO_AND_VIDEO,
};

constexpr char kRemotingNamespace[] = "urn:x-cast:com.google.cast.remoting";
constexpr char kWebRtcNamespace[] = "urn:x-cast:com.google.cast.webrtc";

struct CastMessage {
  std::string message_namespace;
  base::Value data;
};

class CastMessageChannel {
 public:
  virtual ~CastMessageChannel() {}
  virtual void Send(const CastMessage& message) = 0;
};

class SessionClient {
 public:
  virtual ~SessionClient() {}

  // Called when error occurred. The session will be stopped.
  virtual void OnError(SessionError error) = 0;

  // Called when session completes starting.
  virtual void DidStart() = 0;

  // Called when the session is stopped.
  virtual void DidStop() = 0;

  virtual void GetVideoCaptureHost(
      media::mojom::VideoCaptureHostRequest request) = 0;
  virtual void GetNetWorkContext(
      network::mojom::NetworkContextRequest request) = 0;
  // TODO(xjz): Add interface to get AudioCaptureHost.
  // TODO(xjz): Add interface for HW encoder profiles query and VEA create
  // support.

  // TODO(xjz): Change this with an interface to send/receive messages to/from
  // receiver through cast channel, and generate/parse the OFFER/ANSWER message
  // in Mirroing service.
  using GetAnswerCallback = base::OnceCallback<void(
      const media::cast::FrameSenderConfig& audio_config,
      const media::cast::FrameSenderConfig& video_config)>;
  virtual void DoOfferAnswerExchange(
      const std::vector<media::cast::FrameSenderConfig>& audio_configs,
      const std::vector<media::cast::FrameSenderConfig>& video_configs,
      GetAnswerCallback callback) = 0;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_INTERFACE_H_
