// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "content/renderer/media/webrtc/media_stream_track_metrics.h"
#include "third_party/WebKit/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/public/platform/WebRTCStatsRequest.h"
#include "third_party/WebKit/public/platform/WebRTCStatsResponse.h"

namespace blink {
class WebFrame;
class WebRTCDataChannelHandler;
class WebRTCOfferOptions;
class WebRTCPeerConnectionHandlerClient;
}

namespace content {

class PeerConnectionDependencyFactory;
class PeerConnectionTracker;
class RemoteMediaStreamImpl;
class RtcDataChannelHandler;
class RTCMediaConstraints;
class WebRtcMediaStreamAdapter;

// Mockable wrapper for blink::WebRTCStatsResponse
class CONTENT_EXPORT LocalRTCStatsResponse
    : public NON_EXPORTED_BASE(rtc::RefCountInterface) {
 public:
  explicit LocalRTCStatsResponse(const blink::WebRTCStatsResponse& impl)
      : impl_(impl) {
  }

  virtual blink::WebRTCStatsResponse webKitStatsResponse() const;
  virtual size_t addReport(blink::WebString type, blink::WebString id,
                           double timestamp);
  virtual void addStatistic(size_t report,
                            blink::WebString name, blink::WebString value);

 protected:
  ~LocalRTCStatsResponse() override {}
  // Constructor for creating mocks.
  LocalRTCStatsResponse() {}

 private:
  blink::WebRTCStatsResponse impl_;
};

// Mockable wrapper for blink::WebRTCStatsRequest
class CONTENT_EXPORT LocalRTCStatsRequest
    : public NON_EXPORTED_BASE(rtc::RefCountInterface) {
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
    : NON_EXPORTED_BASE(public blink::WebRTCPeerConnectionHandler) {
 public:
  RTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client,
      PeerConnectionDependencyFactory* dependency_factory);
  virtual ~RTCPeerConnectionHandler();

  // Destroy all existing RTCPeerConnectionHandler objects.
  static void DestructAllHandlers();

  static void ConvertOfferOptionsToConstraints(
      const blink::WebRTCOfferOptions& options,
      RTCMediaConstraints* output);

  void associateWithFrame(blink::WebFrame* frame);

  // Initialize method only used for unit test.
  bool InitializeForTest(
      const blink::WebRTCConfiguration& server_configuration,
      const blink::WebMediaConstraints& options,
      const base::WeakPtr<PeerConnectionTracker>& peer_connection_tracker);

  // blink::WebRTCPeerConnectionHandler implementation
  virtual bool initialize(
      const blink::WebRTCConfiguration& server_configuration,
      const blink::WebMediaConstraints& options) override;

  virtual void createOffer(
      const blink::WebRTCSessionDescriptionRequest& request,
      const blink::WebMediaConstraints& options) override;
  virtual void createOffer(
      const blink::WebRTCSessionDescriptionRequest& request,
      const blink::WebRTCOfferOptions& options) override;

  virtual void createAnswer(
      const blink::WebRTCSessionDescriptionRequest& request,
      const blink::WebMediaConstraints& options) override;

  virtual void setLocalDescription(
      const blink::WebRTCVoidRequest& request,
      const blink::WebRTCSessionDescription& description) override;
  virtual void setRemoteDescription(
        const blink::WebRTCVoidRequest& request,
        const blink::WebRTCSessionDescription& description) override;

  virtual blink::WebRTCSessionDescription localDescription()
      override;
  virtual blink::WebRTCSessionDescription remoteDescription()
      override;

  virtual bool updateICE(
      const blink::WebRTCConfiguration& server_configuration,
      const blink::WebMediaConstraints& options) override;
  virtual bool addICECandidate(
      const blink::WebRTCICECandidate& candidate) override;
  virtual bool addICECandidate(
      const blink::WebRTCVoidRequest& request,
      const blink::WebRTCICECandidate& candidate) override;
  virtual void OnaddICECandidateResult(const blink::WebRTCVoidRequest& request,
                                       bool result);

  virtual bool addStream(
      const blink::WebMediaStream& stream,
      const blink::WebMediaConstraints& options) override;
  virtual void removeStream(
      const blink::WebMediaStream& stream) override;
  virtual void getStats(
      const blink::WebRTCStatsRequest& request) override;
  virtual blink::WebRTCDataChannelHandler* createDataChannel(
      const blink::WebString& label,
      const blink::WebRTCDataChannelInit& init) override;
  virtual blink::WebRTCDTMFSenderHandler* createDTMFSender(
      const blink::WebMediaStreamTrack& track) override;
  virtual void stop() override;

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
  void CloseClientPeerConnection();

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
  void OnAddStream(scoped_ptr<RemoteMediaStreamImpl> stream);
  void OnRemoveStream(
      const scoped_refptr<webrtc::MediaStreamInterface>& stream);
  void OnDataChannel(scoped_ptr<RtcDataChannelHandler> handler);
  void OnIceCandidate(const std::string& sdp, const std::string& sdp_mid,
      int sdp_mline_index, int component, int address_family);

 private:
  webrtc::SessionDescriptionInterface* CreateNativeSessionDescription(
      const std::string& sdp, const std::string& type,
      webrtc::SdpParseError* error);

  // Virtual to allow mocks to override.
  virtual scoped_refptr<base::SingleThreadTaskRunner> signaling_thread() const;

  void RunSynchronousClosureOnSignalingThread(const base::Closure& closure,
                                              const char* trace_event_name);

  base::ThreadChecker thread_checker_;

  // |client_| is a weak pointer, and is valid until stop() has returned.
  blink::WebRTCPeerConnectionHandlerClient* client_;

  // |dependency_factory_| is a raw pointer, and is valid for the lifetime of
  // RenderThreadImpl.
  PeerConnectionDependencyFactory* const dependency_factory_;

  blink::WebFrame* frame_;

  ScopedVector<WebRtcMediaStreamAdapter> local_streams_;

  base::WeakPtr<PeerConnectionTracker> peer_connection_tracker_;

  MediaStreamTrackMetrics track_metrics_;

  // Counter for a UMA stat reported at destruction time.
  int num_data_channels_created_;

  // Counter for number of IPv4 and IPv6 local candidates.
  int num_local_candidates_ipv4_;
  int num_local_candidates_ipv6_;

  // To make sure the observer is released after the native_peer_connection_,
  // it has to come first.
  scoped_refptr<Observer> peer_connection_observer_;

  // |native_peer_connection_| is the libjingle native PeerConnection object.
  scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection_;

  typedef std::map<webrtc::MediaStreamInterface*,
      content::RemoteMediaStreamImpl*> RemoteStreamMap;
  RemoteStreamMap remote_streams_;
  scoped_refptr<webrtc::UMAObserver> uma_observer_;
  base::TimeTicks ice_connection_checking_start_;
  base::WeakPtrFactory<RTCPeerConnectionHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RTCPeerConnectionHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_
