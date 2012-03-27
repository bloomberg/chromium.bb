// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastream.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnectionHandler.h"

class MediaStreamDependencyFactory;
class MediaStreamImpl;

// PeerConnectionHandler is a delegate for the PeerConnection API messages going
// between WebKit and native PeerConnection in libjingle. It's owned by
// WebKit.
class CONTENT_EXPORT PeerConnectionHandler
    : NON_EXPORTED_BASE(public WebKit::WebPeerConnectionHandler),
      NON_EXPORTED_BASE(public webrtc::PeerConnectionObserver) {
 public:
  PeerConnectionHandler(
      WebKit::WebPeerConnectionHandlerClient* client,
      MediaStreamImpl* msi,
      MediaStreamDependencyFactory* dependency_factory);
  virtual ~PeerConnectionHandler();

  // Set the video renderer for the specified stream.
  virtual void SetVideoRenderer(
      const std::string& stream_label,
      webrtc::VideoRendererWrapperInterface* renderer);

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
  FRIEND_TEST_ALL_PREFIXES(PeerConnectionHandlerTest, Basic);

  void AddStreams(
      const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>& streams);
  void RemoveStreams(
      const WebKit::WebVector<WebKit::WebMediaStreamDescriptor>& streams);
  void OnAddStreamCallback(webrtc::MediaStreamInterface* stream);
  void OnRemoveStreamCallback(webrtc::MediaStreamInterface* stream);
  WebKit::WebMediaStreamDescriptor CreateWebKitStreamDescriptor(
      webrtc::MediaStreamInterface* stream);

  // client_ is a weak pointer, and is valid until stop() has returned.
  WebKit::WebPeerConnectionHandlerClient* client_;

  // media_stream_impl_ is a weak pointer, and is valid for the lifetime of this
  // class. Calls to it must be done on the render thread.
  MediaStreamImpl* media_stream_impl_;

  // dependency_factory_ is a weak pointer, and is valid for the lifetime of
  // MediaStreamImpl.
  MediaStreamDependencyFactory* dependency_factory_;

  // native_peer_connection_ is the native PeerConnection object,
  // it handles the ICE processing and media engine.
  talk_base::scoped_refptr<webrtc::PeerConnectionInterface>
      native_peer_connection_;

  typedef std::map<webrtc::MediaStreamInterface*,
                   WebKit::WebMediaStreamDescriptor> RemoteStreamMap;
  RemoteStreamMap remote_streams_;

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionHandler);
};

#endif  // CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_H_
