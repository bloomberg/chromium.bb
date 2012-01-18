// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectionfactory.h"
#include "webkit/glue/p2p_transport.h"

namespace content {
class IpcNetworkManager;
class IpcPacketSocketFactory;
class P2PSocketDispatcher;
}

namespace cricket {
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

  // WebRtcMediaEngine is used in CreatePeerConnectionFactory().
  virtual cricket::WebRtcMediaEngine* CreateWebRtcMediaEngine();

  // Creates and deletes |pc_factory_|, which in turn is used for
  // creating PeerConnection objects. |media_engine| is the engine created by
  // CreateWebRtcMediaEngine(). |port_allocator| and |media_engine| will be
  // owned by |pc_factory_|. |worker_thread| is owned by MediaStreamImpl.
  virtual bool CreatePeerConnectionFactory(
      cricket::MediaEngineInterface* media_engine,
      talk_base::Thread* worker_thread);
  virtual void DeletePeerConnectionFactory();
  virtual bool PeerConnectionFactoryCreated();

  // The port allocator is used in CreatePeerConnection().
  virtual cricket::PortAllocator* CreatePortAllocator(
      content::P2PSocketDispatcher* socket_dispatcher,
      talk_base::NetworkManager* network_manager,
      talk_base::PacketSocketFactory* socket_factory,
      const webkit_glue::P2PTransport::Config& config);

  // Asks the PeerConnection factory to create a PeerConnection object. See
  // MediaStreamImpl for details about |signaling_thread|. The PeerConnection
  // object is owned by PeerConnectionHandler.
  virtual webrtc::PeerConnection* CreatePeerConnection(
      cricket::PortAllocator* port_allocator,
      talk_base::Thread* signaling_thread);

 private:
  scoped_ptr<webrtc::PeerConnectionFactory> pc_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDependencyFactory);
};

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
