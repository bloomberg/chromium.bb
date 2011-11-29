// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPeerConnectionHandler.h"

namespace cricket {
class HttpPortAllocator;
}

namespace talk_base {
class Thread;
}

class MediaStreamDependencyFactory;
class MediaStreamImpl;

// PeerConnectionHandler is a delegate for the PeerConnection API messages going
// between WebKit and native PeerConnection in libjingle. It's owned by
// MediaStreamImpl.
class CONTENT_EXPORT PeerConnectionHandler
    : NON_EXPORTED_BASE(public WebKit::WebPeerConnectionHandler),
      NON_EXPORTED_BASE(public webrtc::PeerConnectionObserver) {
 public:
  PeerConnectionHandler(
      WebKit::WebPeerConnectionHandlerClient* client,
      MediaStreamImpl* msi,
      MediaStreamDependencyFactory* dependency_factory,
      talk_base::Thread* signaling_thread,
      cricket::HttpPortAllocator* port_allocator);
  virtual ~PeerConnectionHandler();

  // Set the video renderer for the specified stream.
  virtual bool SetVideoRenderer(const std::string& stream_label,
                                cricket::VideoRenderer* renderer);

  // WebKit::WebPeerConnectionHandler implementation
  virtual void initialize(
      const WebKit::WebString& server_configuration,
      const WebKit::WebSecurityOrigin& security_origin) OVERRIDE;
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
  virtual void stop() OVERRIDE;

  // webrtc::PeerConnectionObserver implementation
  virtual void OnSignalingMessage(const std::string& msg) OVERRIDE;
  virtual void OnAddStream(const std::string& stream_id, bool video) OVERRIDE;
  virtual void OnRemoveStream(
      const std::string& stream_id,
      bool video) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(PeerConnectionHandlerTest, Basic);

  void AddStream(const std::string label);
  void OnAddStreamCallback(const std::string& stream_label);
  void OnRemoveStreamCallback(const std::string& stream_label);

  // client_ is a weak pointer, and is valid until stop() has returned.
  WebKit::WebPeerConnectionHandlerClient* client_;

  // media_stream_impl_ is a weak pointer, and is valid for the lifetime of this
  // class.
  MediaStreamImpl* media_stream_impl_;

  // dependency_factory_ is a weak pointer, and is valid for the lifetime of
  // MediaStreamImpl.
  MediaStreamDependencyFactory* dependency_factory_;

  // native_peer_connection_ is the native PeerConnection object,
  // it handles the ICE processing and media engine.
  scoped_ptr<webrtc::PeerConnection> native_peer_connection_;

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  talk_base::Thread* signaling_thread_;

  cricket::HttpPortAllocator* port_allocator_;

  // Currently, a stream in WebKit has audio and/or video and has one label.
  // Local and remote streams have different labels.
  // In native PeerConnection, a stream has audio or video (not both), and they
  // have separate labels. A remote stream has the same label as the
  // corresponding local stream. Hence the workarounds in the implementation to
  // handle this. It could look like this:
  // WebKit: Local, audio and video: label "foo".
  //         Remote, audio and video: label "foo-remote".
  // Libjingle: Local and remote, audio: label "foo-audio".
  //            Local and remote, video: label "foo".
  // TODO(grunell): This shall be removed or changed when native PeerConnection
  // has been updated to closer follow the specification.
  std::string local_label_;   // Label used in WebKit
  std::string remote_label_;  // Label used in WebKit

  // Call states. Possible transitions:
  // NOT_STARTED -> INITIATING -> SENDING_AND_RECEIVING
  // NOT_STARTED -> RECEIVING
  // RECEIVING -> NOT_STARTED
  // RECEIVING -> SENDING_AND_RECEIVING
  // SENDING_AND_RECEIVING -> NOT_STARTED
  // Note that when in state SENDING_AND_RECEIVING, the other side may or may
  // not send media. Thus, this state does not necessarily mean full duplex.
  // TODO(grunell): This shall be removed or changed when native PeerConnection
  // has been updated to closer follow the specification.
  enum CallState {
    NOT_STARTED,
    INITIATING,
    RECEIVING,
    SENDING_AND_RECEIVING
  };
  CallState call_state_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionHandler);
};

#endif  // CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_H_
