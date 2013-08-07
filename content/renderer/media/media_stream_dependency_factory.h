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
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/p2p/socket_dispatcher.h"
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

namespace webrtc {
class PeerConnection;
}

namespace WebKit {
class WebFrame;
class WebMediaConstraints;
class WebMediaStream;
class WebRTCPeerConnectionHandler;
class WebRTCPeerConnectionHandlerClient;
}

namespace content {

class IpcNetworkManager;
class IpcPacketSocketFactory;
class VideoCaptureImplManager;
class WebRtcAudioCapturer;
class WebRtcAudioDeviceImpl;
class WebRtcLoggingHandlerImpl;
class WebRtcLoggingMessageFilter;
struct StreamDeviceInfo;

#if defined(GOOGLE_TV)
class RTCVideoDecoderFactoryTv;
#endif

// Object factory for RTC MediaStreams and RTC PeerConnections.
class CONTENT_EXPORT MediaStreamDependencyFactory
    : NON_EXPORTED_BASE(public base::NonThreadSafe) {
 public:
  // MediaSourcesCreatedCallback is used in CreateNativeMediaSources.
  typedef base::Callback<void(WebKit::WebMediaStream* web_stream,
                              bool live)> MediaSourcesCreatedCallback;
  MediaStreamDependencyFactory(
      VideoCaptureImplManager* vc_manager,
      P2PSocketDispatcher* p2p_socket_dispatcher);
  virtual ~MediaStreamDependencyFactory();

  // Create a RTCPeerConnectionHandler object that implements the
  // WebKit WebRTCPeerConnectionHandler interface.
  WebKit::WebRTCPeerConnectionHandler* CreateRTCPeerConnectionHandler(
      WebKit::WebRTCPeerConnectionHandlerClient* client);

  // CreateNativeMediaSources creates libjingle representations of
  // the underlying sources to the tracks in |web_stream|.
  // |sources_created| is invoked when the sources have either been created and
  // transitioned to a live state or failed.
  // The libjingle sources is stored in the extra data field of
  // WebMediaStreamSource.
  // |audio_constraints| and |video_constraints| set parameters for the sources.
  void CreateNativeMediaSources(
      int render_view_id,
      const WebKit::WebMediaConstraints& audio_constraints,
      const WebKit::WebMediaConstraints& video_constraints,
      WebKit::WebMediaStream* web_stream,
      const MediaSourcesCreatedCallback& sources_created);

  // Creates a libjingle representation of a MediaStream and stores
  // it in the extra data field of |web_stream|.
  void CreateNativeLocalMediaStream(
      WebKit::WebMediaStream* web_stream);

  // Creates a libjingle representation of a MediaStream and stores
  // it in the extra data field of |web_stream|.
  // |stream_stopped| is a callback that is run when a MediaStream have been
  // stopped.
  void CreateNativeLocalMediaStream(
      WebKit::WebMediaStream* web_stream,
      const MediaStreamExtraData::StreamStopCallback& stream_stop);

  // Adds a libjingle representation of a MediaStreamTrack to |stream| based
  // on the source of |track|.
  bool AddNativeMediaStreamTrack(const WebKit::WebMediaStream& stream,
                                 const WebKit::WebMediaStreamTrack& track);

  // Creates and adds libjingle representation of a MediaStreamTrack to |stream|
  // based on the desired |track_id| and |capturer|.
  bool AddNativeVideoMediaTrack(const std::string& track_id,
                                WebKit::WebMediaStream* stream,
                                cricket::VideoCapturer* capturer);

  bool RemoveNativeMediaStreamTrack(const WebKit::WebMediaStream& stream,
                                    const WebKit::WebMediaStreamTrack& track);

  // Asks the libjingle PeerConnection factory to create a libjingle
  // PeerConnection object.
  // The PeerConnection object is owned by PeerConnectionHandler.
  virtual scoped_refptr<webrtc::PeerConnectionInterface>
      CreatePeerConnection(
          const webrtc::PeerConnectionInterface::IceServers& ice_servers,
          const webrtc::MediaConstraintsInterface* constraints,
          WebKit::WebFrame* web_frame,
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

#if defined(GOOGLE_TV)
  RTCVideoDecoderFactoryTv* decoder_factory_tv() { return decoder_factory_tv_; }
#endif

 protected:
  // Asks the PeerConnection factory to create a Local MediaStream object.
  virtual scoped_refptr<webrtc::MediaStreamInterface>
      CreateLocalMediaStream(const std::string& label);

  // Asks the PeerConnection factory to create a Local Audio Source.
  virtual scoped_refptr<webrtc::AudioSourceInterface>
      CreateLocalAudioSource(
          const webrtc::MediaConstraintsInterface* constraints);

  // Asks the PeerConnection factory to create a Local Video Source.
  virtual scoped_refptr<webrtc::VideoSourceInterface>
      CreateLocalVideoSource(
          int video_session_id,
          bool is_screen_cast,
          const webrtc::MediaConstraintsInterface* constraints);

  // Creates a media::AudioCapturerSource with an implementation that is
  // specific for a WebAudio source. The created WebAudioCapturerSource
  // instance will function as audio source instead of the default
  // WebRtcAudioCapturer.
  virtual scoped_refptr<WebRtcAudioCapturer> CreateWebAudioSource(
      WebKit::WebMediaStreamSource* source);

  // Asks the PeerConnection factory to create a Local AudioTrack object.
  virtual scoped_refptr<webrtc::AudioTrackInterface>
      CreateLocalAudioTrack(const std::string& id,
                            const scoped_refptr<WebRtcAudioCapturer>& capturer,
                            webrtc::AudioSourceInterface* source);

  // Asks the PeerConnection factory to create a Local VideoTrack object.
  virtual scoped_refptr<webrtc::VideoTrackInterface>
      CreateLocalVideoTrack(const std::string& id,
                            webrtc::VideoSourceInterface* source);

  // Asks the PeerConnection factory to create a Local VideoTrack object with
  // the video source using |capturer|.
  virtual scoped_refptr<webrtc::VideoTrackInterface>
      CreateLocalVideoTrack(const std::string& id,
                            cricket::VideoCapturer* capturer);

  virtual bool EnsurePeerConnectionFactory();
  virtual bool PeerConnectionFactoryCreated();

  // Returns a new capturer or existing capturer based on the |render_view_id|
  // and |device_info|. When the |render_view_id| and |device_info| are valid,
  // it reuses existing capture if any; otherwise it creates a new capturer.
  virtual scoped_refptr<WebRtcAudioCapturer> MaybeCreateAudioCapturer(
      int render_view_id, const StreamDeviceInfo& device_info);

 private:
  // Creates and deletes |pc_factory_|, which in turn is used for
  // creating PeerConnection objects.
  bool CreatePeerConnectionFactory();

  void InitializeWorkerThread(talk_base::Thread** thread,
                              base::WaitableEvent* event);

  void CreateIpcNetworkManagerOnWorkerThread(base::WaitableEvent* event);
  void DeleteIpcNetworkManager();
  void CleanupPeerConnectionFactory();

  void CreateWebRtcLoggingHandler(WebRtcLoggingMessageFilter* filter,
                                  const std::string& app_session_id,
                                  const std::string& app_url);

  // We own network_manager_, must be deleted on the worker thread.
  // The network manager uses |p2p_socket_dispatcher_|.
  IpcNetworkManager* network_manager_;
  scoped_ptr<IpcPacketSocketFactory> socket_factory_;

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;

#if defined(GOOGLE_TV)
  // |pc_factory_| will hold the ownership of this object, and |pc_factory_|
  // outlives this object. Thus weak pointer is sufficient.
  RTCVideoDecoderFactoryTv* decoder_factory_tv_;
#endif

  scoped_refptr<VideoCaptureImplManager> vc_manager_;
  scoped_refptr<P2PSocketDispatcher> p2p_socket_dispatcher_;
  scoped_refptr<WebRtcAudioDeviceImpl> audio_device_;

  // PeerConnection threads. signaling_thread_ is created from the
  // "current" chrome thread.
  talk_base::Thread* signaling_thread_;
  talk_base::Thread* worker_thread_;
  base::Thread chrome_worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDependencyFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
