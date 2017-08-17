// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebString.h"

using testing::_;

namespace content {

MockWebRTCPeerConnectionHandlerClient::
MockWebRTCPeerConnectionHandlerClient()
    : candidate_mline_index_(-1) {
  ON_CALL(*this, DidGenerateICECandidate(_))
      .WillByDefault(
          testing::Invoke(this, &MockWebRTCPeerConnectionHandlerClient::
                                    didGenerateICECandidateWorker));
  ON_CALL(*this, DidAddRemoteStream(_, _))
      .WillByDefault(testing::Invoke(
          this,
          &MockWebRTCPeerConnectionHandlerClient::didAddRemoteStreamWorker));
  ON_CALL(*this, DidRemoveRemoteStream(_))
      .WillByDefault(testing::Invoke(
          this,
          &MockWebRTCPeerConnectionHandlerClient::didRemoveRemoteStreamWorker));
}

MockWebRTCPeerConnectionHandlerClient::
~MockWebRTCPeerConnectionHandlerClient() {}

void MockWebRTCPeerConnectionHandlerClient::didGenerateICECandidateWorker(
    const blink::WebRTCICECandidate& candidate) {
  candidate_sdp_ = candidate.Candidate().Utf8();
  candidate_mline_index_ = candidate.SdpMLineIndex();
  candidate_mid_ = candidate.SdpMid().Utf8();
}

void MockWebRTCPeerConnectionHandlerClient::didAddRemoteStreamWorker(
    const blink::WebMediaStream& stream_descriptor,
    blink::WebVector<std::unique_ptr<blink::WebRTCRtpReceiver>>*
        stream_web_rtp_receivers) {
  remote_steam_ = stream_descriptor;
}

void MockWebRTCPeerConnectionHandlerClient::didRemoveRemoteStreamWorker(
    const blink::WebMediaStream& stream_descriptor) {
  remote_steam_.Reset();
}

}  // namespace content
