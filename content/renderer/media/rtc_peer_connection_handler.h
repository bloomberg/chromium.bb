// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_
#define CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "content/renderer/media/peer_connection_handler_base.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCStatsRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRTCStatsResponse.h"

namespace WebKit {
class WebFrame;
class WebRTCDataChannelHandler;
}

namespace content {

class PeerConnectionTracker;

// Mockable wrapper for WebKit::WebRTCStatsResponse
class CONTENT_EXPORT LocalRTCStatsResponse
    : public NON_EXPORTED_BASE(talk_base::RefCountInterface) {
 public:
  explicit LocalRTCStatsResponse(const WebKit::WebRTCStatsResponse& impl)
      : impl_(impl) {
  }

  virtual WebKit::WebRTCStatsResponse webKitStatsResponse() const;
  virtual size_t addReport(WebKit::WebString type, WebKit::WebString id,
                           double timestamp);
  virtual void addStatistic(size_t report,
                            WebKit::WebString name, WebKit::WebString value);

 protected:
  virtual ~LocalRTCStatsResponse() {}
  // Constructor for creating mocks.
  LocalRTCStatsResponse() {}

 private:
  WebKit::WebRTCStatsResponse impl_;
};

// Mockable wrapper for WebKit::WebRTCStatsRequest
class CONTENT_EXPORT LocalRTCStatsRequest
    : public NON_EXPORTED_BASE(talk_base::RefCountInterface) {
 public:
  explicit LocalRTCStatsRequest(WebKit::WebRTCStatsRequest impl);
  // Constructor for testing.
  LocalRTCStatsRequest();

  virtual bool hasSelector() const;
  virtual WebKit::WebMediaStream stream() const;
  virtual WebKit::WebMediaStreamTrack component() const;
  virtual void requestSucceeded(const LocalRTCStatsResponse* response);
  virtual scoped_refptr<LocalRTCStatsResponse> createResponse();

 protected:
  virtual ~LocalRTCStatsRequest();

 private:
  WebKit::WebRTCStatsRequest impl_;
  talk_base::scoped_refptr<LocalRTCStatsResponse> response_;
};

// RTCPeerConnectionHandler is a delegate for the RTC PeerConnection API
// messages going between WebKit and native PeerConnection in libjingle. It's
// owned by WebKit.
// WebKit calls all of these methods on the main render thread.
// Callbacks to the webrtc::PeerConnectionObserver implementation also occur on
// the main render thread.
class CONTENT_EXPORT RTCPeerConnectionHandler
    : public PeerConnectionHandlerBase,
      NON_EXPORTED_BASE(public WebKit::WebRTCPeerConnectionHandler) {
 public:
  RTCPeerConnectionHandler(
      WebKit::WebRTCPeerConnectionHandlerClient* client,
      MediaStreamDependencyFactory* dependency_factory);
  virtual ~RTCPeerConnectionHandler();

  void associateWithFrame(WebKit::WebFrame* frame);

  // Initialize method only used for unit test.
  bool InitializeForTest(
      const WebKit::WebRTCConfiguration& server_configuration,
      const WebKit::WebMediaConstraints& options,
      PeerConnectionTracker* peer_connection_tracker);

  // WebKit::WebRTCPeerConnectionHandler implementation
  virtual bool initialize(
      const WebKit::WebRTCConfiguration& server_configuration,
      const WebKit::WebMediaConstraints& options) OVERRIDE;

  virtual void createOffer(
      const WebKit::WebRTCSessionDescriptionRequest& request,
      const WebKit::WebMediaConstraints& options) OVERRIDE;
  virtual void createAnswer(
      const WebKit::WebRTCSessionDescriptionRequest& request,
      const WebKit::WebMediaConstraints& options) OVERRIDE;

  virtual void setLocalDescription(
      const WebKit::WebRTCVoidRequest& request,
      const WebKit::WebRTCSessionDescription& description) OVERRIDE;
  virtual void setRemoteDescription(
        const WebKit::WebRTCVoidRequest& request,
        const WebKit::WebRTCSessionDescription& description) OVERRIDE;

  virtual WebKit::WebRTCSessionDescription localDescription()
      OVERRIDE;
  virtual WebKit::WebRTCSessionDescription remoteDescription()
      OVERRIDE;
  virtual bool updateICE(
      const WebKit::WebRTCConfiguration& server_configuration,
      const WebKit::WebMediaConstraints& options) OVERRIDE;
  virtual bool addICECandidate(
      const WebKit::WebRTCICECandidate& candidate) OVERRIDE;

  virtual bool addStream(
      const WebKit::WebMediaStream& stream,
      const WebKit::WebMediaConstraints& options) OVERRIDE;
  virtual void removeStream(
      const WebKit::WebMediaStream& stream) OVERRIDE;
  virtual void getStats(
      const WebKit::WebRTCStatsRequest& request) OVERRIDE;
  virtual WebKit::WebRTCDataChannelHandler* createDataChannel(
      const WebKit::WebString& label, bool reliable) OVERRIDE;
  virtual WebKit::WebRTCDTMFSenderHandler* createDTMFSender(
      const WebKit::WebMediaStreamTrack& track) OVERRIDE;
  virtual void stop() OVERRIDE;

  // webrtc::PeerConnectionObserver implementation
  virtual void OnError() OVERRIDE;
  // Triggered when the SignalingState changed.
  virtual void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) OVERRIDE;
  virtual void OnAddStream(webrtc::MediaStreamInterface* stream) OVERRIDE;
  virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream) OVERRIDE;
  virtual void OnIceCandidate(
      const webrtc::IceCandidateInterface* candidate) OVERRIDE;
  virtual void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) OVERRIDE;
  virtual void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) OVERRIDE;

  virtual void OnDataChannel(
      webrtc::DataChannelInterface* data_channel) OVERRIDE;
  virtual void OnRenegotiationNeeded() OVERRIDE;

  // Delegate functions to allow for mocking of WebKit interfaces.
  // getStats takes ownership of request parameter.
  virtual void getStats(LocalRTCStatsRequest* request);

  // Calls GetStats on |native_peer_connection_|.
  void GetStats(webrtc::StatsObserver* observer,
                webrtc::MediaStreamTrackInterface* track);

  PeerConnectionTracker* peer_connection_tracker();

 private:
  webrtc::SessionDescriptionInterface* CreateNativeSessionDescription(
      const WebKit::WebRTCSessionDescription& description,
      webrtc::SdpParseError* error);

  // |client_| is a weak pointer, and is valid until stop() has returned.
  WebKit::WebRTCPeerConnectionHandlerClient* client_;

  WebKit::WebFrame* frame_;

  PeerConnectionTracker* peer_connection_tracker_;

  DISALLOW_COPY_AND_ASSIGN(RTCPeerConnectionHandler);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_PEER_CONNECTION_HANDLER_H_
