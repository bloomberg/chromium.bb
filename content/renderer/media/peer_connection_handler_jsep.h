// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_JSEP_H_
#define CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_JSEP_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "content/common/content_export.h"
#include "content/renderer/media/peer_connection_handler_base.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnection00Handler.h"

// PeerConnectionHandlerJsep is a delegate for the JSEP PeerConnection API
// messages going between WebKit and native PeerConnection in libjingle. It's
// owned by WebKit.
class CONTENT_EXPORT PeerConnectionHandlerJsep
    : public PeerConnectionHandlerBase,
      NON_EXPORTED_BASE(public WebKit::WebPeerConnection00Handler) {
 public:
  PeerConnectionHandlerJsep(
      WebKit::WebPeerConnection00HandlerClient* client,
      MediaStreamDependencyFactory* dependency_factory);
  virtual ~PeerConnectionHandlerJsep();

  // WebKit::WebPeerConnection00Handler implementation
  virtual void initialize(
      const WebKit::WebString& server_configuration,
      const WebKit::WebString& username) OVERRIDE;
  virtual WebKit::WebSessionDescriptionDescriptor createOffer(
      const WebKit::WebMediaHints& hints) OVERRIDE;
  virtual WebKit::WebSessionDescriptionDescriptor createAnswer(
      const WebKit::WebString& offer,
      const WebKit::WebMediaHints& hints) OVERRIDE;
  virtual bool setLocalDescription(
      Action action,
      const WebKit::WebSessionDescriptionDescriptor& description) OVERRIDE;
  virtual bool setRemoteDescription(
      Action action,
      const WebKit::WebSessionDescriptionDescriptor& description) OVERRIDE;
  virtual WebKit::WebSessionDescriptionDescriptor localDescription() OVERRIDE;
  virtual WebKit::WebSessionDescriptionDescriptor remoteDescription() OVERRIDE;
  virtual bool startIce(const WebKit::WebICEOptions& options) OVERRIDE;
  virtual bool processIceMessage(
      const WebKit::WebICECandidateDescriptor& candidate) OVERRIDE;
  virtual void addStream(
      const WebKit::WebMediaStreamDescriptor& stream) OVERRIDE;
  virtual void removeStream(
      const WebKit::WebMediaStreamDescriptor& stream) OVERRIDE;
  // We will be deleted by WebKit after stop has been returned.
  virtual void stop() OVERRIDE;

  // webrtc::PeerConnectionObserver implementation
  virtual void OnError() OVERRIDE;
  virtual void OnMessage(const std::string& msg) OVERRIDE;
  virtual void OnSignalingMessage(const std::string& msg) OVERRIDE;
  virtual void OnStateChange(StateType state_changed) OVERRIDE;
  virtual void OnAddStream(webrtc::MediaStreamInterface* stream) OVERRIDE;
  virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream) OVERRIDE;
  virtual void OnIceCandidate(
      const webrtc::IceCandidateInterface* candidate) OVERRIDE;
  virtual void OnIceComplete() OVERRIDE;

 private:
  webrtc::SessionDescriptionInterface* CreateNativeSessionDescription(
      const WebKit::WebSessionDescriptionDescriptor& description);
  WebKit::WebSessionDescriptionDescriptor CreateWebKitSessionDescription(
      const webrtc::SessionDescriptionInterface* native_desc);
  bool GetNativeAction(
      const Action action,
      webrtc::PeerConnectionInterface::Action* native_action);

  // client_ is a weak pointer, and is valid until stop() has returned.
  WebKit::WebPeerConnection00HandlerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionHandlerJsep);
};

#endif  // CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_JSEP_H_
