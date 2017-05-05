// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_sender.h"

#include "base/logging.h"
#include "third_party/webrtc/base/scoped_ref_ptr.h"

namespace content {

namespace {

inline bool operator==(
    const std::unique_ptr<blink::WebMediaStreamTrack>& web_track,
    const webrtc::MediaStreamTrackInterface* webrtc_track) {
  if (!web_track)
    return !webrtc_track;
  return webrtc_track && web_track->Id() == webrtc_track->id().c_str();
}

}  // namespace

uintptr_t RTCRtpSender::getId(
    const webrtc::RtpSenderInterface* webrtc_rtp_sender) {
  return reinterpret_cast<uintptr_t>(webrtc_rtp_sender);
}

RTCRtpSender::RTCRtpSender(
    webrtc::RtpSenderInterface* webrtc_rtp_sender,
    std::unique_ptr<blink::WebMediaStreamTrack> web_track)
    : webrtc_rtp_sender_(webrtc_rtp_sender), web_track_(std::move(web_track)) {
  DCHECK(webrtc_rtp_sender_);
  DCHECK(web_track_ == webrtc_track());
}

RTCRtpSender::~RTCRtpSender() {}

uintptr_t RTCRtpSender::Id() const {
  return getId(webrtc_rtp_sender_.get());
}

const blink::WebMediaStreamTrack* RTCRtpSender::Track() const {
  DCHECK(web_track_ == webrtc_track());
  return web_track_.get();
}

const webrtc::MediaStreamTrackInterface* RTCRtpSender::webrtc_track() const {
  return webrtc_rtp_sender_->track();
}

}  // namespace content
