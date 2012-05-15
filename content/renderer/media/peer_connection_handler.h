// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "content/common/content_export.h"
#include "content/renderer/media/peer_connection_handler_base.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnectionHandler.h"

// PeerConnectionHandler is a delegate for the ROAP PeerConnection API messages
// going between WebKit and native PeerConnection in libjingle. It's owned by
// WebKit. ROAP PeerConnection will be removed soon.
class CONTENT_EXPORT PeerConnectionHandler
    : public PeerConnectionHandlerBase,
      NON_EXPORTED_BASE(public WebKit::WebPeerConnectionHandler) {
 public:
  PeerConnectionHandler(
      WebKit::WebPeerConnectionHandlerClient* client,
      MediaStreamDependencyFactory* dependency_factory);
  virtual ~PeerConnectionHandler();

  // WebKit::WebPeerConnectionHandler implementation
  virtual void initialize(
      const WebKit::WebString& server_configuration,
      const WebKit::WebString& username) OVERRIDE;
  virtual void produceInitialOffer(
      const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>&
          pending_add_streams) OVERRIDE;
  virtual void handleInitialOffer(const WebKit::WebString& sdp) OVERRIDE;
  virtual void processSDP(const WebKit::WebString& sdp) OVERRIDE;
  virtual void processPendingStreams(
      const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>&
          pending_add_streams,
      const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>&
          pending_remove_streams) OVERRIDE;
  virtual void sendDataStreamMessage(const char* data, size_t length) OVERRIDE;
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
  void OnAddStreamCallback(webrtc::MediaStreamInterface* stream);
  void OnRemoveStreamCallback(webrtc::MediaStreamInterface* stream);

  // client_ is a weak pointer, and is valid until stop() has returned.
  WebKit::WebPeerConnectionHandlerClient* client_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionHandler);
};

#endif  // CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_H_
