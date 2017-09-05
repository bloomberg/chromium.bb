// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/media_stream_track_metrics.h"
#include "content/renderer/media/webrtc/rtc_rtp_sender.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_adapter_map.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/public/platform/WebRTCStatsRequest.h"
#include "third_party/WebKit/public/platform/WebRTCStatsResponse.h"

namespace blink {
class WebLocalFrame;
class WebRTCAnswerOptions;
class WebRTCDataChannelHandler;
class WebRTCLegacyStats;
class WebRTCOfferOptions;
class WebRTCPeerConnectionHandlerClient;
}

namespace content {

class PeerConnectionDependencyFactory;
class PeerConnectionTracker;
class RemoteMediaStreamImpl;
class RtcDataChannelHandler;

// Mockable wrapper for blink::WebRTCStatsResponse
class CONTENT_EXPORT LocalRTCStatsResponse : public rtc::RefCountInterface {
 public:
  explicit LocalRTCStatsResponse(const blink::WebRTCStatsResponse& impl)
      : impl_(impl) {
  }

  virtual blink::WebRTCStatsResponse webKitStatsResponse() const;
  virtual void addStats(const blink::WebRTCLegacyStats& stats);

 protected:
  ~LocalRTCStatsResponse() override {}
  // Constructor for creating mocks.
  LocalRTCStatsResponse() {}

 private:
  blink::WebRTCStatsResponse impl_;
};

// Mockable wrapper for blink::WebRTCStatsRequest
class CONTENT_EXPORT LocalRTCStatsRequest : public rtc::RefCountInterface {
 public:
  explicit LocalRTCStatsRequest(blink::WebRTCStatsRequest impl);
  // Constructor for testing.
  LocalRTCStatsRequest();

  virtual bool hasSelector() const;
  virtual blink::WebMediaStreamTrack component() const;
  virtual void requestSucceeded(const LocalRTCStatsResponse* response);
  virtual scoped_refptr<LocalRTCStatsResponse> createResponse();

 protected:
  ~LocalRTCStatsRequest() override;

 private:
  blink::WebRTCStatsRequest impl_;
};

// RTCPeerConnectionHandler is a delegate for the RTC PeerConnection API
// messages going between WebKit and native PeerConnection in libjingle. It's
// owned by WebKit.
// WebKit calls all of these methods on the main render thread.
// Callbacks to the webrtc::PeerConnectionObserver implementation also occur on
// the main render thread.
class CONTENT_EXPORT RTCPeerConnectionHandler
    : public blink::WebRTCPeerConnectionHandler {
 public:
  RTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client,
      PeerConnectionDependencyFactory* dependency_factory);
  ~RTCPeerConnectionHandler() override;

  // Destroy all existing RTCPeerConnectionHandler objects.
  static void DestructAllHandlers();

  void associateWithFrame(blink::WebLocalFrame* frame);

  // Initialize method only used for unit test.
  bool InitializeForTest(
      const blink::WebRTCConfiguration& server_configuration,
      const blink::WebMediaConstraints& options,
      const base::WeakPtr<PeerConnectionTracker>& peer_connection_tracker);

  // blink::WebRTCPeerConnectionHandler implementation
  bool Initialize(const blink::WebRTCConfiguration& server_configuration,
                  const blink::WebMediaConstraints& options) override;

  void CreateOffer(const blink::WebRTCSessionDescriptionRequest& request,
                   const blink::WebMediaConstraints& options) override;
  void CreateOffer(const blink::WebRTCSessionDescriptionRequest& request,
                   const blink::WebRTCOfferOptions& options) override;

  void CreateAnswer(const blink::WebRTCSessionDescriptionRequest& request,
                    const blink::WebMediaConstraints& options) override;
  void CreateAnswer(const blink::WebRTCSessionDescriptionRequest& request,
                    const blink::WebRTCAnswerOptions& options) override;

  void SetLocalDescription(
      const blink::WebRTCVoidRequest& request,
      const blink::WebRTCSessionDescription& description) override;
  void SetRemoteDescription(
      const blink::WebRTCVoidRequest& request,
      const blink::WebRTCSessionDescription& description) override;

  blink::WebRTCSessionDescription LocalDescription() override;
  blink::WebRTCSessionDescription RemoteDescription() override;

  blink::WebRTCErrorType SetConfiguration(
      const blink::WebRTCConfiguration& configuration) override;
  bool AddICECandidate(const blink::WebRTCICECandidate& candidate) override;
  bool AddICECandidate(const blink::WebRTCVoidRequest& request,
                       const blink::WebRTCICECandidate& candidate) override;
  virtual void OnaddICECandidateResult(const blink::WebRTCVoidRequest& request,
                                       bool result);

  bool AddStream(const blink::WebMediaStream& stream,
                 const blink::WebMediaConstraints& options) override;
  void RemoveStream(const blink::WebMediaStream& stream) override;
  void GetStats(const blink::WebRTCStatsRequest& request) override;
  void GetStats(
      std::unique_ptr<blink::WebRTCStatsReportCallback> callback) override;
  blink::WebVector<std::unique_ptr<blink::WebRTCRtpSender>> GetSenders()
      override;
  blink::WebVector<std::unique_ptr<blink::WebRTCRtpReceiver>> GetReceivers()
      override;
  std::unique_ptr<blink::WebRTCRtpSender> AddTrack(
      const blink::WebMediaStreamTrack& web_track,
      const blink::WebVector<blink::WebMediaStream>& web_streams) override;
  bool RemoveTrack(blink::WebRTCRtpSender* web_sender) override;

  blink::WebRTCDataChannelHandler* CreateDataChannel(
      const blink::WebString& label,
      const blink::WebRTCDataChannelInit& init) override;
  blink::WebRTCDTMFSenderHandler* CreateDTMFSender(
      const blink::WebMediaStreamTrack& track) override;
  void Stop() override;

  // Delegate functions to allow for mocking of WebKit interfaces.
  // getStats takes ownership of request parameter.
  virtual void getStats(const scoped_refptr<LocalRTCStatsRequest>& request);

  // Asynchronously calls native_peer_connection_->getStats on the signaling
  // thread.  If the |track_id| is empty, the |track_type| parameter is ignored.
  void GetStats(webrtc::StatsObserver* observer,
                webrtc::PeerConnectionInterface::StatsOutputLevel level,
                const std::string& track_id,
                blink::WebMediaStreamSource::Type track_type);

  // Tells the |client_| to close RTCPeerConnection.
  // Make it virtual for testing purpose.
  virtual void CloseClientPeerConnection();

  // Start recording an event log.
  void StartEventLog(IPC::PlatformFileForTransit file,
                     int64_t max_file_size_bytes);
  // Stop recording an event log.
  void StopEventLog();

 protected:
  webrtc::PeerConnectionInterface* native_peer_connection() {
    return native_peer_connection_.get();
  }

  class Observer;
  friend class Observer;

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state);
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state);
  void OnRenegotiationNeeded();
  void OnAddStream(
      std::unique_ptr<RemoteMediaStreamImpl> stream,
      std::vector<std::unique_ptr<blink::WebRTCRtpReceiver>> web_receivers);
  void OnRemoveStream(
      const scoped_refptr<webrtc::MediaStreamInterface>& stream);
  void OnDataChannel(std::unique_ptr<RtcDataChannelHandler> handler);
  void OnIceCandidate(const std::string& sdp, const std::string& sdp_mid,
      int sdp_mline_index, int component, int address_family);

 private:
  // Record info about the first SessionDescription from the local and
  // remote side to record UMA stats once both are set.
  struct FirstSessionDescription {
    FirstSessionDescription(const webrtc::SessionDescriptionInterface* desc);

    bool audio = false;
    bool video = false;
    // If audio or video will use RTCP-mux (if there is no audio or
    // video, then false).
    bool rtcp_mux = false;
  };

  webrtc::SessionDescriptionInterface* CreateNativeSessionDescription(
      const std::string& sdp, const std::string& type,
      webrtc::SdpParseError* error);

  // Report to UMA whether an IceConnectionState has occurred. It only records
  // the first occurrence of a given state.
  void ReportICEState(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);

  // Reset UMA related members to the initial state. This is invoked at the
  // constructor as well as after Ice Restart.
  void ResetUMAStats();

  void ReportFirstSessionDescriptions(
      const FirstSessionDescription& local,
      const FirstSessionDescription& remote);

  std::unique_ptr<blink::WebRTCRtpReceiver> GetWebRTCRtpReceiver(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> webrtc_receiver);

  scoped_refptr<base::SingleThreadTaskRunner> signaling_thread() const;

  void RunSynchronousClosureOnSignalingThread(const base::Closure& closure,
                                              const char* trace_event_name);

  base::ThreadChecker thread_checker_;

  // |client_| is a weak pointer to the blink object (blink::RTCPeerConnection)
  // that owns this object.
  // It is valid for the lifetime of this object.
  blink::WebRTCPeerConnectionHandlerClient* const client_;
  // True if this PeerConnection has been closed.
  // After the PeerConnection has been closed, this object may no longer
  // forward callbacks to blink.
  bool is_closed_;

  // |dependency_factory_| is a raw pointer, and is valid for the lifetime of
  // RenderThreadImpl.
  PeerConnectionDependencyFactory* const dependency_factory_;

  blink::WebLocalFrame* frame_ = nullptr;

  // Map and owners of track adapters. Every track that is in use by the peer
  // connection has an associated blink and webrtc layer representation of it.
  // The map keeps track of the relationship between
  // |blink::WebMediaStreamTrack|s and |webrtc::MediaStreamTrackInterface|s.
  // Track adapters are created on the fly when a component (such as a stream)
  // needs to reference it, and automatically disposed when there are no longer
  // any components referencing it.
  scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_adapter_map_;
  // Map and owners of stream adapters. Every stream that is in use by the peer
  // connection has an associated blink and webrtc layer representation of it.
  // The map keeps track of the relationship between |blink::WebMediaStream|s
  // and |webrtc::MediaStreamInterface|s. Stream adapters are created on the fly
  // when a component (such as |local_streams_| or a sender) needs to reference
  // it, and automatically disposed when there are no longer any components
  // referencing it.
  // TODO(hbos): Update and use the map for the |remote_streams_| case too.
  // crbug.com/705901
  scoped_refptr<WebRtcMediaStreamAdapterMap> stream_adapter_map_;
  // Local stream adapters. Every stream that is in use by the peer connection
  // has an associated blink and webrtc layer representation of it. This vector
  // keeps track of the relationship between |blink::WebMediaStream|s and
  // |webrtc::MediaStreamInterface|s. Streams are added and removed from the
  // peer connection using |AddStream| and |RemoveStream|.
  std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
      local_streams_;
  // Maps |RTCRtpSender::getId|s of |webrtc::RtpSenderInterface| to the
  // corresponding content layer sender. This is needed to retain the senders'
  // associated set of streams for senders created by |AddTrack|.
  std::map<uintptr_t, std::unique_ptr<RTCRtpSender>> rtp_senders_;
  // We don't need an "rtp_receivers_" map as long as the blink layer receivers
  // are not GC'd (protecting the relevant adapters from destruction). These can
  // be constructed anew on every |GetReceivers| call.
  // TODO(hbos): When receivers can be created separately from remote streams we
  // should add an "rtp_receivers_" map too to get rid of the requirement for
  // the blink layer to keep a receiver reference. https://crbug.com/741619

  base::WeakPtr<PeerConnectionTracker> peer_connection_tracker_;

  MediaStreamTrackMetrics track_metrics_;

  // Counter for a UMA stat reported at destruction time.
  int num_data_channels_created_ = 0;

  // Counter for number of IPv4 and IPv6 local candidates.
  int num_local_candidates_ipv4_ = 0;
  int num_local_candidates_ipv6_ = 0;

  // To make sure the observers are released after native_peer_connection_,
  // they have to come first.
  scoped_refptr<Observer> peer_connection_observer_;
  scoped_refptr<webrtc::UMAObserver> uma_observer_;

  // |native_peer_connection_| is the libjingle native PeerConnection object.
  scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection_;

  // The last applied configuration. Used so that the constraints
  // used when constructing the PeerConnection carry over when
  // SetConfiguration is called.
  webrtc::PeerConnectionInterface::RTCConfiguration configuration_;

  // Record info about the first SessionDescription from the local and
  // remote side to record UMA stats once both are set.  We only check
  // for the first offer or answer.  "pranswer"s and "unknown"s (from
  // unit tests) are ignored.
  std::unique_ptr<FirstSessionDescription> first_local_description_;
  std::unique_ptr<FirstSessionDescription> first_remote_description_;

  std::map<webrtc::MediaStreamInterface*,
           std::unique_ptr<content::RemoteMediaStreamImpl>>
      remote_streams_;
  base::TimeTicks ice_connection_checking_start_;

  // Track which ICE Connection state that this PeerConnection has gone through.
  bool ice_state_seen_[webrtc::PeerConnectionInterface::kIceConnectionMax] = {};

  base::WeakPtrFactory<RTCPeerConnectionHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RTCPeerConnectionHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_
