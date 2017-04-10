// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_receiver.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/renderer/media/webrtc/rtc_rtp_contributing_source.h"
#include "third_party/webrtc/base/scoped_ref_ptr.h"

namespace content {

namespace {

inline bool operator==(const blink::WebMediaStreamTrack& web_track,
                       const webrtc::MediaStreamTrackInterface& webrtc_track) {
  return !web_track.IsNull() && web_track.Id() == webrtc_track.id().c_str();
}

}  // namespace

uintptr_t RTCRtpReceiver::getId(
    const webrtc::RtpReceiverInterface* webrtc_rtp_receiver) {
  return reinterpret_cast<uintptr_t>(webrtc_rtp_receiver);
}

RTCRtpReceiver::RTCRtpReceiver(
    webrtc::RtpReceiverInterface* webrtc_rtp_receiver,
    const blink::WebMediaStreamTrack& web_track)
    : webrtc_rtp_receiver_(webrtc_rtp_receiver), web_track_(web_track) {
  DCHECK(webrtc_rtp_receiver_);
  DCHECK(!web_track_.IsNull());
  DCHECK(web_track_ == webrtc_track());
}

RTCRtpReceiver::~RTCRtpReceiver() {}

uintptr_t RTCRtpReceiver::Id() const {
  return getId(webrtc_rtp_receiver_.get());
}

const blink::WebMediaStreamTrack& RTCRtpReceiver::Track() const {
  DCHECK(web_track_ == webrtc_track());
  return web_track_;
}

blink::WebVector<std::unique_ptr<blink::WebRTCRtpContributingSource>>
RTCRtpReceiver::GetSources() {
  auto webrtc_sources = webrtc_rtp_receiver_->GetSources();
  blink::WebVector<std::unique_ptr<blink::WebRTCRtpContributingSource>> sources(
      webrtc_sources.size());
  for (size_t i = 0; i < webrtc_sources.size(); ++i) {
    sources[i] = base::MakeUnique<RTCRtpContributingSource>(webrtc_sources[i]);
  }
  return sources;
}

const webrtc::MediaStreamTrackInterface& RTCRtpReceiver::webrtc_track() const {
  const webrtc::MediaStreamTrackInterface* webrtc_track =
      webrtc_rtp_receiver_->track();
  DCHECK(webrtc_track);
  DCHECK(web_track_ == *webrtc_track);
  return *webrtc_track;
}

}  // namespace content
