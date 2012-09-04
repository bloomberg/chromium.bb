// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"

namespace content {
class IpcNetworkManager;
class IpcPacketSocketFactory;
class P2PSocketDispatcher;
}

namespace cricket {
class PortAllocator;
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

class VideoCaptureImplManager;
class WebRtcAudioDeviceImpl;

// Object factory for MediaStreamImpl and PeerConnectionHandler.
class CONTENT_EXPORT MediaStreamDependencyFactory {
 public:
  explicit MediaStreamDependencyFactory(VideoCaptureImplManager* vc_manager);
  virtual ~MediaStreamDependencyFactory();

  // Creates and deletes |pc_factory_|, which in turn is used for
  // creating PeerConnection objects.
  virtual bool CreatePeerConnectionFactory(
      talk_base::Thread* worker_thread,
      talk_base::Thread* signaling_thread,
      content::P2PSocketDispatcher* socket_dispatcher,
      talk_base::NetworkManager* network_manager,
      talk_base::PacketSocketFactory* socket_factory);
  virtual void ReleasePeerConnectionFactory();
  virtual bool PeerConnectionFactoryCreated();

  // Asks the PeerConnection factory to create a PeerConnection object.
  // The PeerConnection object is owned by PeerConnectionHandler.
  virtual talk_base::scoped_refptr<webrtc::PeerConnectionInterface>
      CreatePeerConnection(const std::string& config,
                           webrtc::PeerConnectionObserver* observer);

  // Asks the PeerConnection factory to create a Local MediaStream object.
  virtual talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface>
      CreateLocalMediaStream(const std::string& label);

  // Asks the PeerConnection factory to create a Local VideoTrack object.
  virtual talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface>
      CreateLocalVideoTrack(const std::string& label,
                            int video_session_id);

  // Asks the PeerConnection factory to create a Local AudioTrack object.
  virtual talk_base::scoped_refptr<webrtc::LocalAudioTrackInterface>
      CreateLocalAudioTrack(const std::string& label,
                            webrtc::AudioDeviceModule* audio_device);

  virtual webrtc::SessionDescriptionInterface* CreateSessionDescription(
      const std::string& sdp);
  virtual webrtc::IceCandidateInterface* CreateIceCandidate(
      const std::string& sdp_mid,
      int sdp_mline_index,
      const std::string& sdp);

  virtual void SetAudioDeviceSessionId(int session_id);

 private:
  talk_base::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;
  scoped_refptr<VideoCaptureImplManager> vc_manager_;
  scoped_refptr<WebRtcAudioDeviceImpl> audio_device_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDependencyFactory);
};

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
