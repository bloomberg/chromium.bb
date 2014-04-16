// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_process_observer.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnectioninterface.h"
#include "third_party/libjingle/source/talk/app/webrtc/videosourceinterface.h"

namespace base {
class WaitableEvent;
}

namespace talk_base {
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

// Object factory for RTC MediaStreams and RTC PeerConnections.
class CONTENT_EXPORT MediaStreamDependencyFactory
    : NON_EXPORTED_BASE(public base::NonThreadSafe),
      public RenderProcessObserver {
 public:
  // MediaSourcesCreatedCallback is used in CreateNativeMediaSources.
  typedef base::Callback<void(blink::WebMediaStream* web_stream,
                              bool live)> MediaSourcesCreatedCallback;
  MediaStreamDependencyFactory(
      P2PSocketDispatcher* p2p_socket_dispatcher);
  virtual ~MediaStreamDependencyFactory();

  // Create a RTCPeerConnectionHandler object that implements the
  // WebKit WebRTCPeerConnectionHandler interface.
  blink::WebRTCPeerConnectionHandler* CreateRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client);

  // InitializeMediaStreamAudioSource initialize a MediaStream source object
  // for audio input.
  bool InitializeMediaStreamAudioSource(
      int render_view_id,
      const blink::WebMediaConstraints& audio_constraints,
      MediaStreamAudioSource* source_data);

  // Creates an implementation of a cricket::VideoCapturer object that can be
  // used when creating a libjingle webrtc::VideoSourceInterface object.
  virtual WebRtcVideoCapturerAdapter* CreateVideoCapturer(
      bool is_screen_capture);

  // Creates a libjingle representation of a MediaStream.
  scoped_refptr<webrtc::MediaStreamInterface> CreateNativeLocalMediaStream(
      const blink::WebMediaStream& web_stream);

  // Create an instance of WebRtcLocalAudioTrack and store it
  // in the extraData field of |track|.
  void CreateLocalAudioTrack(const blink::WebMediaStreamTrack& track);

  // Asks the PeerConnection factory to create a Local VideoTrack object.
  virtual scoped_refptr<webrtc::VideoTrackInterface>
      CreateLocalVideoTrack(const std::string& id,
                            webrtc::VideoSourceInterface* source);

  // Adds a libjingle representation of a MediaStreamTrack to the libjingle
  // Representation of |stream|.
  bool AddNativeMediaStreamTrack(const blink::WebMediaStream& stream,
                                 const blink::WebMediaStreamTrack& track);

  // Removes a libjingle MediaStreamTrack from the libjingle representation of
  // |stream|.
  bool RemoveNativeMediaStreamTrack(const blink::WebMediaStream& stream,
                                    const blink::WebMediaStreamTrack& track);

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
          const webrtc::PeerConnectionInterface::IceServers& ice_servers,
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

  WebRtcAudioDeviceImpl* GetWebRtcAudioDevice();

  static void AddNativeAudioTrackToBlinkTrack(
      webrtc::MediaStreamTrackInterface* native_track,
      const blink::WebMediaStreamTrack& webkit_track,
      bool is_local_track);

 protected:
  // Asks the PeerConnection factory to create a Local MediaStream object.
  virtual scoped_refptr<webrtc::MediaStreamInterface>
      CreateLocalMediaStream(const std::string& label);

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

  // Returns a new capturer or existing capturer based on the |render_view_id|
  // and |device_info|. When the |render_view_id| and |device_info| are valid,
  // it reuses existing capture if any; otherwise it creates a new capturer.
  virtual scoped_refptr<WebRtcAudioCapturer> CreateAudioCapturer(
      int render_view_id, const StreamDeviceInfo& device_info,
      const blink::WebMediaConstraints& constraints,
      MediaStreamAudioSource* audio_source);

  // Adds the audio device as a sink to the audio track and starts the local
  // audio track. This is virtual for test purposes since no real audio device
  // exist in unit tests.
  virtual void StartLocalAudioTrack(WebRtcLocalAudioTrack* audio_track);

 private:
  // Creates |pc_factory_|, which in turn is used for
  // creating PeerConnection objects.
  void CreatePeerConnectionFactory();

  void InitializeWorkerThread(talk_base::Thread** thread,
                              base::WaitableEvent* event);

  void CreateIpcNetworkManagerOnWorkerThread(base::WaitableEvent* event);
  void DeleteIpcNetworkManager();
  void CleanupPeerConnectionFactory();

  // RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnAecDumpFile(IPC::PlatformFileForTransit file_handle);
  void OnDisableAecDump();

  void StartAecDump(const base::PlatformFile& aec_dump_file);

  // Helper method to create a WebRtcAudioDeviceImpl.
  void EnsureWebRtcAudioDeviceImpl();

  // We own network_manager_, must be deleted on the worker thread.
  // The network manager uses |p2p_socket_dispatcher_|.
  IpcNetworkManager* network_manager_;
  scoped_ptr<IpcPacketSocketFactory> socket_factory_;

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;

  scoped_refptr<P2PSocketDispatcher> p2p_socket_dispatcher_;
  scoped_refptr<WebRtcAudioDeviceImpl> audio_device_;

  // PeerConnection threads. signaling_thread_ is created from the
  // "current" chrome thread.
  talk_base::Thread* signaling_thread_;
  talk_base::Thread* worker_thread_;
  base::Thread chrome_worker_thread_;

  base::PlatformFile aec_dump_file_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDependencyFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
