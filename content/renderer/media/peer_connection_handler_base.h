// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_BASE_H_
#define CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_BASE_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastream.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"

class MediaStreamDependencyFactory;

// PeerConnectionHandlerBase is the base class of a delegate for the
// PeerConnection API messages going between WebKit and native
// PeerConnection in libjingle.
class CONTENT_EXPORT PeerConnectionHandlerBase
    : NON_EXPORTED_BASE(public webrtc::PeerConnectionObserver) {
 public:
  PeerConnectionHandlerBase(
      MediaStreamDependencyFactory* dependency_factory);

 protected:
  virtual ~PeerConnectionHandlerBase();

  void AddStream(const WebKit::WebMediaStreamDescriptor& stream);
  bool AddStream(const WebKit::WebMediaStreamDescriptor& stream,
                 const webrtc::MediaConstraintsInterface* constraints);
  void RemoveStream(const WebKit::WebMediaStreamDescriptor& stream);
  WebKit::WebMediaStreamDescriptor CreateWebKitStreamDescriptor(
      webrtc::MediaStreamInterface* stream);

  // dependency_factory_ is a raw pointer, and is valid for the lifetime of
  // MediaStreamImpl.
  MediaStreamDependencyFactory* dependency_factory_;

  // native_peer_connection_ is the native PeerConnection object,
  // it handles the ICE processing and media engine.
  talk_base::scoped_refptr<webrtc::PeerConnectionInterface>
      native_peer_connection_;

  typedef std::map<webrtc::MediaStreamInterface*,
                   WebKit::WebMediaStreamDescriptor> RemoteStreamMap;
  RemoteStreamMap remote_streams_;

  // The message loop we are created on and on which to make calls to WebKit.
  // This should be the render thread message loop.
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionHandlerBase);
};

#endif  // CONTENT_RENDERER_MEDIA_PEER_CONNECTION_HANDLER_BASE_H_
