// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_center.h"

#include <string>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "third_party/libjingle/source/talk/app/webrtc/jsep.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebICECandidateDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamCenterClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSourcesRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSessionDescriptionDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

namespace content {

MediaStreamCenter::MediaStreamCenter(
    WebKit::WebMediaStreamCenterClient* client)
    : client_(client) {
}

void MediaStreamCenter::queryMediaStreamSources(
    const WebKit::WebMediaStreamSourcesRequest& request) {
  WebKit::WebVector<WebKit::WebMediaStreamSource> audioSources, videoSources;
  request.didCompleteQuery(audioSources, videoSources);
}

void MediaStreamCenter::didEnableMediaStreamTrack(
    const WebKit::WebMediaStreamDescriptor& stream,
    const WebKit::WebMediaStreamComponent& component) {
}

void MediaStreamCenter::didDisableMediaStreamTrack(
    const WebKit::WebMediaStreamDescriptor& stream,
    const WebKit::WebMediaStreamComponent& component) {
}

void MediaStreamCenter::didStopLocalMediaStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
}

void MediaStreamCenter::didConstructMediaStream(
    const WebKit::WebMediaStreamDescriptor& stream) {
}

WebKit::WebString MediaStreamCenter::constructSDP(
    const WebKit::WebICECandidateDescriptor& candidate) {
  webrtc::IceCandidateInterface* native_candidate =
      webrtc::CreateIceCandidate(UTF16ToUTF8(candidate.label()),
                                 UTF16ToUTF8(candidate.candidateLine()));
  std::string sdp;
  if (!native_candidate->ToString(&sdp))
    LOG(ERROR) << "Could not create SDP string";
  return UTF8ToUTF16(sdp);
}

WebKit::WebString MediaStreamCenter::constructSDP(
    const WebKit::WebSessionDescriptionDescriptor& description) {
  webrtc::SessionDescriptionInterface* native_desc =
      webrtc::CreateSessionDescription(UTF16ToUTF8(description.initialSDP()));
  if (!native_desc)
    return WebKit::WebString();

  for (size_t i = 0; i < description.numberOfAddedCandidates(); ++i) {
    WebKit::WebICECandidateDescriptor candidate = description.candidate(i);
    webrtc::IceCandidateInterface* native_candidate =
        webrtc::CreateIceCandidate(UTF16ToUTF8(candidate.label()),
                                   UTF16ToUTF8(candidate.candidateLine()));
    native_desc->AddCandidate(native_candidate);
  }

  std::string sdp;
  if (!native_desc->ToString(&sdp))
    LOG(ERROR) << "Could not create SDP string";
  return UTF8ToUTF16(sdp);
}

}  // namespace content
