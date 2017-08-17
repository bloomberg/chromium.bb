// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_
#define CONTENT_RENDERER_MEDIA_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebRTCICECandidate.h"
#include "third_party/WebKit/public/platform/WebRTCPeerConnectionHandlerClient.h"

namespace content {

class MockWebRTCPeerConnectionHandlerClient
    : public blink::WebRTCPeerConnectionHandlerClient {
 public:
  MockWebRTCPeerConnectionHandlerClient();
  virtual ~MockWebRTCPeerConnectionHandlerClient();

  // WebRTCPeerConnectionHandlerClient implementation.
  MOCK_METHOD0(NegotiationNeeded, void());
  MOCK_METHOD1(DidGenerateICECandidate,
               void(const blink::WebRTCICECandidate& candidate));
  MOCK_METHOD1(DidChangeSignalingState, void(SignalingState state));
  MOCK_METHOD1(DidChangeICEGatheringState, void(ICEGatheringState state));
  MOCK_METHOD1(DidChangeICEConnectionState, void(ICEConnectionState state));
  MOCK_METHOD2(
      DidAddRemoteStream,
      void(const blink::WebMediaStream& stream_descriptor,
           blink::WebVector<std::unique_ptr<blink::WebRTCRtpReceiver>>*));
  MOCK_METHOD1(DidRemoveRemoteStream,
               void(const blink::WebMediaStream& stream_descriptor));
  MOCK_METHOD1(DidAddRemoteDataChannel, void(blink::WebRTCDataChannelHandler*));
  MOCK_METHOD0(ReleasePeerConnectionHandler, void());

  void didGenerateICECandidateWorker(
      const blink::WebRTCICECandidate& candidate);
  void didAddRemoteStreamWorker(
      const blink::WebMediaStream& stream_descriptor,
      blink::WebVector<std::unique_ptr<blink::WebRTCRtpReceiver>>*
          stream_web_rtp_receivers);
  void didRemoveRemoteStreamWorker(
      const blink::WebMediaStream& stream_descriptor);

  const std::string& candidate_sdp() const { return candidate_sdp_; }
  int candidate_mlineindex() const {
    return candidate_mline_index_;
  }
  const std::string& candidate_mid() const { return candidate_mid_ ; }
  const blink::WebMediaStream& remote_stream() const { return remote_steam_;}

 private:
  blink::WebMediaStream remote_steam_;
  std::string candidate_sdp_;
  int candidate_mline_index_;
  std::string candidate_mid_;

  DISALLOW_COPY_AND_ASSIGN(MockWebRTCPeerConnectionHandlerClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_
