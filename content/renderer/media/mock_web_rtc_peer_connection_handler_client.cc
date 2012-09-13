// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebICECandidateDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

namespace WebKit {

MockWebRTCPeerConnectionHandlerClient::
MockWebRTCPeerConnectionHandlerClient()
    : renegotiate_(false),
      ready_state_(ReadyStateNew),
      ice_state_(ICEStateNew),
      candidate_mline_index_(-1) {
}

MockWebRTCPeerConnectionHandlerClient::
~MockWebRTCPeerConnectionHandlerClient() {}

void MockWebRTCPeerConnectionHandlerClient::negotiationNeeded() {
  renegotiate_ = true;
}

void MockWebRTCPeerConnectionHandlerClient::didGenerateICECandidate(
    const WebRTCICECandidate& candidate) {
  if (!candidate.isNull()) {
    candidate_sdp_ = UTF16ToUTF8(candidate.candidate());
    candidate_mline_index_ = candidate.sdpMLineIndex();
    candidate_mid_ = UTF16ToUTF8(candidate.sdpMid());
  } else {
    candidate_sdp_ = "";
    candidate_mline_index_ = -1;
    candidate_mid_ = "";
  }
}

void MockWebRTCPeerConnectionHandlerClient::didChangeReadyState(
    ReadyState state) {
  ready_state_ = state;
}

void MockWebRTCPeerConnectionHandlerClient::didChangeICEState(ICEState state) {
  ice_state_ = state;
}

void MockWebRTCPeerConnectionHandlerClient::didAddRemoteStream(
    const WebMediaStreamDescriptor& stream_descriptor) {
  stream_label_ = UTF16ToUTF8(stream_descriptor.label());
}

void MockWebRTCPeerConnectionHandlerClient::didRemoveRemoteStream(
    const WebMediaStreamDescriptor& stream_descriptor) {
  DCHECK(stream_label_ == UTF16ToUTF8(stream_descriptor.label()));
  stream_label_.clear();
}

}  // namespace WebKit
