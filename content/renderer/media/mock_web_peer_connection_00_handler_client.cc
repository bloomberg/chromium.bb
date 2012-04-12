// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/mock_web_peer_connection_00_handler_client.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebICECandidateDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

namespace WebKit {

MockWebPeerConnection00HandlerClient::
MockWebPeerConnection00HandlerClient()
    : ready_state_(ReadyStateNew),
      more_to_follow_(false) {
}

MockWebPeerConnection00HandlerClient::
~MockWebPeerConnection00HandlerClient() {}

void MockWebPeerConnection00HandlerClient::didGenerateICECandidate(
    const WebICECandidateDescriptor& candidate,
    bool more_to_follow) {
  if (candidate.isNull()) {
    candidate_label_.clear();
    candidate_sdp_.clear();
  } else {
    candidate_label_ = UTF16ToUTF8(candidate.label());
    candidate_sdp_ = UTF16ToUTF8(candidate.candidateLine());
  }
  more_to_follow_ = more_to_follow;
}

void MockWebPeerConnection00HandlerClient::didChangeReadyState(
    ReadyState state) {
  ready_state_ = state;
}

void MockWebPeerConnection00HandlerClient::didChangeICEState(ICEState state) {
  NOTIMPLEMENTED();
}

void MockWebPeerConnection00HandlerClient::didAddRemoteStream(
    const WebMediaStreamDescriptor& stream_descriptor) {
  stream_label_ = UTF16ToUTF8(stream_descriptor.label());
}

void MockWebPeerConnection00HandlerClient::didRemoveRemoteStream(
    const WebMediaStreamDescriptor& stream_descriptor) {
  DCHECK(stream_label_ == UTF16ToUTF8(stream_descriptor.label()));
  stream_label_.clear();
}

}  // namespace WebKit
