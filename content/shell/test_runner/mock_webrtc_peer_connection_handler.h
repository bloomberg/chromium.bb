// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_MOCK_WEBRTC_PEER_CONNECTION_HANDLER_H_
#define CONTENT_SHELL_TEST_RUNNER_MOCK_WEBRTC_PEER_CONNECTION_HANDLER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/WebKit/public/platform/WebRTCError.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandler.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescription.h"
#include "third_party/WebKit/public/platform/WebRTCSessionDescriptionRequest.h"
#include "third_party/WebKit/public/platform/WebRTCStatsRequest.h"

namespace blink {
class WebRTCPeerConnectionHandlerClient;
};

namespace test_runner {

class TestInterfaces;

class MockWebRTCPeerConnectionHandler
    : public blink::WebRTCPeerConnectionHandler {
 public:
  MockWebRTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client,
      TestInterfaces* interfaces);
  ~MockWebRTCPeerConnectionHandler() override;

  // WebRTCPeerConnectionHandler related methods
  bool Initialize(const blink::WebRTCConfiguration& configuration,
                  const blink::WebMediaConstraints& constraints) override;

  void CreateOffer(const blink::WebRTCSessionDescriptionRequest& request,
                   const blink::WebMediaConstraints& constraints) override;
  void CreateOffer(const blink::WebRTCSessionDescriptionRequest& request,
                   const blink::WebRTCOfferOptions& options) override;
  void CreateAnswer(const blink::WebRTCSessionDescriptionRequest& request,
                    const blink::WebMediaConstraints& constraints) override;
  void CreateAnswer(const blink::WebRTCSessionDescriptionRequest& request,
                    const blink::WebRTCAnswerOptions& options) override;
  void SetLocalDescription(
      const blink::WebRTCVoidRequest& request,
      const blink::WebRTCSessionDescription& local_description) override;
  void SetRemoteDescription(
      const blink::WebRTCVoidRequest& request,
      const blink::WebRTCSessionDescription& remote_description) override;
  blink::WebRTCSessionDescription LocalDescription() override;
  blink::WebRTCSessionDescription RemoteDescription() override;
  blink::WebRTCErrorType SetConfiguration(
      const blink::WebRTCConfiguration& configuration) override;
  bool AddICECandidate(const blink::WebRTCICECandidate& ice_candidate) override;
  bool AddICECandidate(const blink::WebRTCVoidRequest& request,
                       const blink::WebRTCICECandidate& ice_candidate) override;
  bool AddStream(const blink::WebMediaStream& stream,
                 const blink::WebMediaConstraints& constraints) override;
  void RemoveStream(const blink::WebMediaStream& stream) override;
  void GetStats(const blink::WebRTCStatsRequest& request) override;
  void GetStats(std::unique_ptr<blink::WebRTCStatsReportCallback>) override;
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

 private:
  MockWebRTCPeerConnectionHandler();

  // UpdateRemoteStreams uses the collection of |local_streams_| to create
  // remote MediaStreams with the same number of tracks and notifies |client_|
  // about added and removed streams. It's triggered when setRemoteDescription
  // is called.
  void UpdateRemoteStreams();

  void ReportInitializeCompleted();
  void ReportCreationOfDataChannel();

  void PostRequestResult(
      const blink::WebRTCSessionDescriptionRequest& request,
      const blink::WebRTCSessionDescription& session_description);
  void PostRequestFailure(
      const blink::WebRTCSessionDescriptionRequest& request);
  void PostRequestResult(const blink::WebRTCVoidRequest& request);
  void PostRequestFailure(const blink::WebRTCVoidRequest& request);

  blink::WebRTCPeerConnectionHandlerClient* client_;
  bool stopped_;
  blink::WebRTCSessionDescription local_description_;
  blink::WebRTCSessionDescription remote_description_;
  int stream_count_;
  TestInterfaces* interfaces_;
  typedef std::map<std::string, blink::WebMediaStream> StreamMap;
  StreamMap local_streams_;
  StreamMap remote_streams_;
  // Tracks added with |addTrack|, not including tracks of |local_streams_|.
  std::map<std::string, blink::WebMediaStreamTrack> tracks_;
  std::map<std::string, uintptr_t> id_by_track_;

  base::WeakPtrFactory<MockWebRTCPeerConnectionHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockWebRTCPeerConnectionHandler);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_MOCK_WEBRTC_PEER_CONNECTION_HANDLER_H_
