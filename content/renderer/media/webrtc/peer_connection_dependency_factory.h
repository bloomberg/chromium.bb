// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_PEER_CONNECTION_DEPENDENCY_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_PEER_CONNECTION_DEPENDENCY_FACTORY_H_

#include <string>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_process_observer.h"
#include "content/renderer/media/webrtc/stun_field_trial.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"
#include "third_party/webrtc/api/videosourceinterface.h"
#include "third_party/webrtc/p2p/stunprober/stunprober.h"

namespace base {
class WaitableEvent;
}

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace rtc {
class NetworkManager;
class PacketSocketFactory;
class Thread;
}

namespace blink {
class WebFrame;
class WebMediaConstraints;
class WebMediaStream;
class WebMediaStreamSource;
class WebMediaStreamTrack;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
}

namespace content {

class IpcNetworkManager;
class IpcPacketSocketFactory;
class MediaStreamAudioSource;
class RTCMediaConstraints;
class WebAudioCapturerSource;
class WebRtcAudioCapturer;
class WebRtcAudioDeviceImpl;
class WebRtcLocalAudioTrack;
class WebRtcLoggingHandlerImpl;
class WebRtcLoggingMessageFilter;
class WebRtcVideoCapturerAdapter;
struct StreamDeviceInfo;

// Object factory for RTC PeerConnections.
class CONTENT_EXPORT PeerConnectionDependencyFactory
    : NON_EXPORTED_BASE(base::MessageLoop::DestructionObserver),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  PeerConnectionDependencyFactory(
      P2PSocketDispatcher* p2p_socket_dispatcher);
  ~PeerConnectionDependencyFactory() override;

  // Create a RTCPeerConnectionHandler object that implements the
  // WebKit WebRTCPeerConnectionHandler interface.
  blink::WebRTCPeerConnectionHandler* CreateRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client);

  // Asks the PeerConnection factory to create a Local MediaStream object.
  virtual scoped_refptr<webrtc::MediaStreamInterface>
      CreateLocalMediaStream(const std::string& label);

  // InitializeMediaStreamAudioSource initialize a MediaStream source object
  // for audio input.
  bool InitializeMediaStreamAudioSource(
      int render_frame_id,
      const blink::WebMediaConstraints& audio_constraints,
      MediaStreamAudioSource* source_data);

  // Creates an implementation of a cricket::VideoCapturer object that can be
  // used when creating a libjingle webrtc::VideoSourceInterface object.
  virtual WebRtcVideoCapturerAdapter* CreateVideoCapturer(
      bool is_screen_capture);

  // Creates an instance of WebRtcLocalAudioTrack and stores it
  // in the extraData field of |track|.
  void CreateLocalAudioTrack(const blink::WebMediaStreamTrack& track);

  // Creates an instance of MediaStreamRemoteAudioTrack and associates with the
  // |track| object.
  void CreateRemoteAudioTrack(const blink::WebMediaStreamTrack& track);

  // Asks the PeerConnection factory to create a Local VideoTrack object.
  virtual scoped_refptr<webrtc::VideoTrackInterface>
      CreateLocalVideoTrack(const std::string& id,
                            webrtc::VideoSourceInterface* source);

  // Asks the PeerConnection factory to create a Video Source.
  // The video source takes ownership of |capturer|.
  virtual scoped_refptr<webrtc::VideoSourceInterface>
      CreateVideoSource(cricket::VideoCapturer* capturer,
                        const blink::WebMediaConstraints& constraints);

  // Asks the libjingle PeerConnection factory to create a libjingle
  // PeerConnection object.
  // The PeerConnection object is owned by PeerConnectionHandler.
  virtual scoped_refptr<webrtc::PeerConnectionInterface>
      CreatePeerConnection(
          const webrtc::PeerConnectionInterface::RTCConfiguration& config,
          const webrtc::MediaConstraintsInterface* constraints,
          blink::WebFrame* web_frame,
          webrtc::PeerConnectionObserver* observer);

  // Creates a libjingle representation of a Session description. Used by a
  // RTCPeerConnectionHandler instance.
  virtual webrtc::SessionDescriptionInterface* CreateSessionDescription(
      const std::string& type,
      const std::string& sdp,
      webrtc::SdpParseError* error);

  // Creates a libjingle representation of an ice candidate.
  virtual webrtc::IceCandidateInterface* CreateIceCandidate(
      const std::string& sdp_mid,
      int sdp_mline_index,
      const std::string& sdp);

  // Starts recording an RTC event log.
  virtual bool StartRtcEventLog(base::PlatformFile file);

  // Starts recording an RTC event log.
  virtual void StopRtcEventLog();

  WebRtcAudioDeviceImpl* GetWebRtcAudioDevice();

  void EnsureInitialized();
  scoped_refptr<base::SingleThreadTaskRunner> GetWebRtcWorkerThread() const;
  scoped_refptr<base::SingleThreadTaskRunner> GetWebRtcSignalingThread() const;

 protected:
  // Asks the PeerConnection factory to create a Local Audio Source.
  virtual scoped_refptr<webrtc::AudioSourceInterface>
      CreateLocalAudioSource(
          const webrtc::MediaConstraintsInterface* constraints);

  // Creates a media::AudioCapturerSource with an implementation that is
  // specific for a WebAudio source. The created WebAudioCapturerSource
  // instance will function as audio source instead of the default
  // WebRtcAudioCapturer.
  virtual scoped_refptr<WebAudioCapturerSource> CreateWebAudioSource(
      blink::WebMediaStreamSource* source);

  // Asks the PeerConnection factory to create a Local VideoTrack object with
  // the video source using |capturer|.
  virtual scoped_refptr<webrtc::VideoTrackInterface>
      CreateLocalVideoTrack(const std::string& id,
                            cricket::VideoCapturer* capturer);

  virtual const scoped_refptr<webrtc::PeerConnectionFactoryInterface>&
      GetPcFactory();
  virtual bool PeerConnectionFactoryCreated();

  // Returns a new capturer or existing capturer based on the |render_frame_id|
  // and |device_info|; if both are valid, it reuses existing capture if any --
  // otherwise it creates a new capturer.
  virtual scoped_refptr<WebRtcAudioCapturer> CreateAudioCapturer(
      int render_frame_id,
      const StreamDeviceInfo& device_info,
      const blink::WebMediaConstraints& constraints,
      MediaStreamAudioSource* audio_source);

  // Adds the audio device as a sink to the audio track and starts the local
  // audio track. This is virtual for test purposes since no real audio device
  // exist in unit tests.
  virtual void StartLocalAudioTrack(WebRtcLocalAudioTrack* audio_track);

 private:
  // Implement base::MessageLoop::DestructionObserver.
  // This makes sure the libjingle PeerConnectionFactory is released before
  // the renderer message loop is destroyed.
  void WillDestroyCurrentMessageLoop() override;

  // Functions related to Stun probing trial to determine how fast we could send
  // Stun request without being dropped by NAT.
  void TryScheduleStunProbeTrial();
  void StartStunProbeTrialOnWorkerThread(const std::string& params);

  // Creates |pc_factory_|, which in turn is used for
  // creating PeerConnection objects.
  void CreatePeerConnectionFactory();

  void InitializeSignalingThread(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      base::WaitableEvent* event);

  void InitializeWorkerThread(rtc::Thread** thread,
                              base::WaitableEvent* event);

  void CreateIpcNetworkManagerOnWorkerThread(base::WaitableEvent* event);
  void DeleteIpcNetworkManager();
  void CleanupPeerConnectionFactory();

  // Helper method to create a WebRtcAudioDeviceImpl.
  void EnsureWebRtcAudioDeviceImpl();

  // We own network_manager_, must be deleted on the worker thread.
  // The network manager uses |p2p_socket_dispatcher_|.
  IpcNetworkManager* network_manager_;
  scoped_ptr<IpcPacketSocketFactory> socket_factory_;

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;

  scoped_refptr<P2PSocketDispatcher> p2p_socket_dispatcher_;
  scoped_refptr<WebRtcAudioDeviceImpl> audio_device_;

  scoped_ptr<StunProberTrial> stun_trial_;

  // PeerConnection threads. signaling_thread_ is created from the
  // "current" chrome thread.
  rtc::Thread* signaling_thread_;
  rtc::Thread* worker_thread_;
  base::Thread chrome_signaling_thread_;
  base::Thread chrome_worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionDependencyFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_PEER_CONNECTION_DEPENDENCY_FACTORY_H_
