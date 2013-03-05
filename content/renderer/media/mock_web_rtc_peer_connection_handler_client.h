// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_
#define CONTENT_RENDERER_MEDIA_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCICECandidate.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCPeerConnectionHandlerClient.h"

namespace content {

class MockWebRTCPeerConnectionHandlerClient
    : public WebKit::WebRTCPeerConnectionHandlerClient {
 public:
  MockWebRTCPeerConnectionHandlerClient();
  virtual ~MockWebRTCPeerConnectionHandlerClient();

  // WebRTCPeerConnectionHandlerClient implementation.
  virtual void negotiationNeeded() OVERRIDE;
  virtual void didGenerateICECandidate(
      const WebKit::WebRTCICECandidate& candidate) OVERRIDE;
  virtual void didChangeSignalingState(SignalingState state) OVERRIDE;
  virtual void didChangeICEGatheringState(ICEGatheringState state) OVERRIDE;
  virtual void didChangeICEConnectionState(ICEConnectionState state) OVERRIDE;
  virtual void didAddRemoteStream(
      const WebKit::WebMediaStream& stream_descriptor) OVERRIDE;
  virtual void didRemoveRemoteStream(
      const WebKit::WebMediaStream& stream_descriptor) OVERRIDE;

  bool renegotiate() const { return renegotiate_; }

  const std::string& candidate_sdp() const { return candidate_sdp_; }
  int candidate_mlineindex() const {
    return candidate_mline_index_;
  }
  const std::string& candidate_mid() const { return candidate_mid_ ; }
  SignalingState signaling_state() const { return signaling_state_; }
  ICEConnectionState ice_connection_state() const {
    return ice_connection_state_;
  }
  ICEGatheringState ice_gathering_state() const {
    return ice_gathering_state_;
  }
  const WebKit::WebMediaStream& remote_stream() const { return remote_steam_;}
  const std::string& stream_label() const { return stream_label_; }

 private:
  bool renegotiate_;
  std::string stream_label_;
  WebKit::WebMediaStream remote_steam_;
  SignalingState signaling_state_;
  ICEConnectionState ice_connection_state_;
  ICEGatheringState ice_gathering_state_;
  std::string candidate_sdp_;
  int candidate_mline_index_;
  std::string candidate_mid_;

  DISALLOW_COPY_AND_ASSIGN(MockWebRTCPeerConnectionHandlerClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_
