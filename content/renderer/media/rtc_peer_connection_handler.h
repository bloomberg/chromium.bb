// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/renderer/media/peer_connection_handler_base.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCPeerConnectionHandler.h"

// RTCPeerConnectionHandler is a delegate for the RTC PeerConnection API
// messages going between WebKit and native PeerConnection in libjingle. It's
// owned by WebKit.
// WebKit call all of these methods on the main render thread.
// Callbacks to the webrtc::PeerConnectionObserver implementation also occur on
// the main render thread.
class CONTENT_EXPORT RTCPeerConnectionHandler
    : public PeerConnectionHandlerBase,
      NON_EXPORTED_BASE(public WebKit::WebRTCPeerConnectionHandler) {
 public:
  RTCPeerConnectionHandler(
      WebKit::WebRTCPeerConnectionHandlerClient* client,
      MediaStreamDependencyFactory* dependency_factory);
  virtual ~RTCPeerConnectionHandler();

  // WebKit::WebRTCPeerConnectionHandler implementation
  virtual bool initialize(
      const WebKit::WebRTCConfiguration& server_configuration,
      const WebKit::WebMediaConstraints& options) OVERRIDE;

  virtual void createOffer(
      const WebKit::WebRTCSessionDescriptionRequest& request,
      const WebKit::WebMediaConstraints& options) OVERRIDE;
  virtual void createAnswer(
      const WebKit::WebRTCSessionDescriptionRequest& request,
      const WebKit::WebMediaConstraints& options) OVERRIDE;

  virtual void setLocalDescription(
      const WebKit::WebRTCVoidRequest& request,
      const WebKit::WebRTCSessionDescription& description) OVERRIDE;
  virtual void setRemoteDescription(
        const WebKit::WebRTCVoidRequest& request,
        const WebKit::WebRTCSessionDescription& description) OVERRIDE;

  virtual WebKit::WebRTCSessionDescription localDescription()
      OVERRIDE;
  virtual WebKit::WebRTCSessionDescription remoteDescription()
      OVERRIDE;
  virtual bool updateICE(
      const WebKit::WebRTCConfiguration& server_configuration,
      const WebKit::WebMediaConstraints& options) OVERRIDE;
  virtual bool addICECandidate(
      const WebKit::WebRTCICECandidate& candidate) OVERRIDE;

  virtual bool addStream(
      const WebKit::WebMediaStreamDescriptor& stream,
      const WebKit::WebMediaConstraints& options) OVERRIDE;
  virtual void removeStream(
      const WebKit::WebMediaStreamDescriptor& stream) OVERRIDE;
  // We will be deleted by WebKit after stop has been returned.
  virtual void stop() OVERRIDE;

  // webrtc::PeerConnectionObserver implementation
  virtual void OnError() OVERRIDE;
  virtual void OnStateChange(StateType state_changed) OVERRIDE;
  virtual void OnAddStream(webrtc::MediaStreamInterface* stream) OVERRIDE;
  virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream) OVERRIDE;
  virtual void OnIceCandidate(
      const webrtc::IceCandidateInterface* candidate) OVERRIDE;
  virtual void OnIceComplete() OVERRIDE;
  virtual void OnRenegotiationNeeded() OVERRIDE;

 private:
  webrtc::SessionDescriptionInterface* CreateNativeSessionDescription(
      const WebKit::WebRTCSessionDescription& description);

  // |client_| is a weak pointer, and is valid until stop() has returned.
  WebKit::WebRTCPeerConnectionHandlerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(RTCPeerConnectionHandler);
};

#endif  // CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_
