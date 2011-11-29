// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectionfactory.h"

namespace content {
class IpcNetworkManager;
class IpcPacketSocketFactory;
class P2PSocketDispatcher;
}

namespace cricket {
class HttpPortAllocator;
class MediaEngineInterface;
class PortAllocator;
class WebRtcMediaEngine;
}

namespace talk_base {
class NetworkManager;
class PacketSocketFactory;
class Thread;
}

namespace webrtc {
class PeerConnection;
class VideoCaptureModule;
}

// Object factory for MediaStreamImpl and PeerConnectionHandler.
class CONTENT_EXPORT MediaStreamDependencyFactory {
 public:
  MediaStreamDependencyFactory();
  virtual ~MediaStreamDependencyFactory();

  // WebRtcMediaEngine is used in CreatePeerConnectionFactory(), see below.
  virtual cricket::WebRtcMediaEngine* CreateWebRtcMediaEngine();

  // Creates and deletes |pc_factory_|, which in turn is used for
  // creating PeerConnection objects. |media_engine| is the engine created by
  // CreateWebRtcMediaEngine(). |port_allocator| and |media_engine| will be
  // owned by |pc_factory_|. |worker_thread| is owned by MediaStreamImpl.
  virtual bool CreatePeerConnectionFactory(
      cricket::PortAllocator* port_allocator,
      cricket::MediaEngineInterface* media_engine,
      talk_base::Thread* worker_thread);
  virtual void DeletePeerConnectionFactory();
  virtual bool PeerConnectionFactoryCreated();

  // Asks the PeerConnection factory to create a PeerConnection object. See
  // MediaStreamImpl for details about |signaling_thread|. The PeerConnection
  // object is owned by PeerConnectionHandler.
  virtual webrtc::PeerConnection* CreatePeerConnection(
      talk_base::Thread* signaling_thread);

 private:
  scoped_ptr<webrtc::PeerConnectionFactory> pc_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDependencyFactory);
};

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
