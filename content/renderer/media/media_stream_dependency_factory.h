// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"

namespace base {
class WaitableEvent;
}

namespace content {
class IpcNetworkManager;
class IpcPacketSocketFactory;
}

namespace talk_base {
class NetworkManager;
class PacketSocketFactory;
class Thread;
}

namespace webrtc {
class PeerConnection;
}

namespace WebKit {
class WebMediaStreamDescriptor;
class WebPeerConnection00Handler;
class WebPeerConnection00HandlerClient;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
}

class WebRtcAudioDeviceImpl;
class VideoCaptureImplManager;

// Object factory for RTC MediaStreams and RTC PeerConnections.
class CONTENT_EXPORT MediaStreamDependencyFactory
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  MediaStreamDependencyFactory(
      VideoCaptureImplManager* vc_manager,
      content::P2PSocketDispatcher* p2p_socket_dispatcher);
  virtual ~MediaStreamDependencyFactory();

  // Create a PeerConnectionHandlerJsep object that implements the
  // WebKit WebPeerConnection00Handler interface.
  WebKit::WebPeerConnection00Handler* CreatePeerConnectionHandlerJsep(
      WebKit::WebPeerConnection00HandlerClient* client);

  // Create a RTCPeerConnectionHandler object that implements the
  // WebKit WebRTCPeerConnectionHandler interface.
  WebKit::WebRTCPeerConnectionHandler* CreateRTCPeerConnectionHandler(
      WebKit::WebRTCPeerConnectionHandlerClient* client);

  // Creates a libjingle representation of a MediaStream and stores
  // it in the extra data field of |description|
  bool CreateNativeLocalMediaStream(
      WebKit::WebMediaStreamDescriptor* description);

  // Asks the libjingle PeerConnection factory to create a libjingle
  // PeerConnection object.
  // The PeerConnection object is owned by PeerConnectionHandler.
  virtual scoped_refptr<webrtc::PeerConnectionInterface>
      CreatePeerConnection(const std::string& config,
                           webrtc::PeerConnectionObserver* observer);

  virtual scoped_refptr<webrtc::PeerConnectionInterface>
      CreatePeerConnection(const webrtc::JsepInterface::IceServers& ice_servers,
                           const webrtc::MediaConstraintsInterface* constraints,
                           webrtc::PeerConnectionObserver* observer);

  // Creates a libjingle representation of a Session description. Used by a
  // PeerConnectionHandlerJsep instance.
  virtual webrtc::SessionDescriptionInterface* CreateSessionDescription(
      const std::string& sdp);

  // Creates a libjingle representation of a Session description. Used by a
  // RTCPeerConnectionHandler instance.
  virtual webrtc::SessionDescriptionInterface* CreateSessionDescription(
      const std::string& type,
      const std::string& sdp);

  // Creates a libjingle representation of an ice candidate.
  virtual webrtc::IceCandidateInterface* CreateIceCandidate(
      const std::string& sdp_mid,
      int sdp_mline_index,
      const std::string& sdp);

 protected:
  // Asks the PeerConnection factory to create a Local MediaStream object.
  virtual scoped_refptr<webrtc::LocalMediaStreamInterface>
      CreateLocalMediaStream(const std::string& label);

  // Asks the PeerConnection factory to create a Local VideoTrack object.
  virtual scoped_refptr<webrtc::LocalVideoTrackInterface>
      CreateLocalVideoTrack(const std::string& label,
                            int video_session_id);

  // Asks the PeerConnection factory to create a Local AudioTrack object.
  virtual scoped_refptr<webrtc::LocalAudioTrackInterface>
      CreateLocalAudioTrack(const std::string& label,
                            webrtc::AudioDeviceModule* audio_device);

  virtual bool EnsurePeerConnectionFactory();
  virtual void SetAudioDeviceSessionId(int session_id);

 private:
  // Creates and deletes |pc_factory_|, which in turn is used for
  // creating PeerConnection objects.
  bool CreatePeerConnectionFactory(
      talk_base::Thread* worker_thread,
      talk_base::Thread* signaling_thread,
      content::P2PSocketDispatcher* socket_dispatcher,
      talk_base::NetworkManager* network_manager,
      talk_base::PacketSocketFactory* socket_factory);
  bool PeerConnectionFactoryCreated();

  void InitializeWorkerThread(talk_base::Thread** thread,
                              base::WaitableEvent* event);

  void CreateIpcNetworkManagerOnWorkerThread(base::WaitableEvent* event);
  void DeleteIpcNetworkManager();
  void CleanupPeerConnectionFactory();

  // We own network_manager_, must be deleted on the worker thread.
  // The network manager uses |p2p_socket_dispatcher_|.
  content::IpcNetworkManager* network_manager_;
  scoped_ptr<content::IpcPacketSocketFactory> socket_factory_;

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;

  scoped_refptr<VideoCaptureImplManager> vc_manager_;
  scoped_refptr<content::P2PSocketDispatcher> p2p_socket_dispatcher_;
  scoped_refptr<WebRtcAudioDeviceImpl> audio_device_;

  // PeerConnection threads. signaling_thread_ is created from the
  // "current" chrome thread.
  talk_base::Thread* signaling_thread_;
  talk_base::Thread* worker_thread_;
  base::Thread chrome_worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDependencyFactory);
};

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
