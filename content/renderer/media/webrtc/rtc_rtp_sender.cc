// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_sender.h"

#include "base/logging.h"
#include "third_party/webrtc/base/scoped_ref_ptr.h"

namespace content {

namespace {

inline bool operator==(
    const std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>&
        track_adapter,
    const webrtc::MediaStreamTrackInterface* webrtc_track) {
  if (!track_adapter)
    return !webrtc_track;
  return track_adapter->webrtc_track() == webrtc_track;
}

}  // namespace

uintptr_t RTCRtpSender::getId(
    const webrtc::RtpSenderInterface* webrtc_rtp_sender) {
  return reinterpret_cast<uintptr_t>(webrtc_rtp_sender);
}

RTCRtpSender::RTCRtpSender(
    scoped_refptr<webrtc::RtpSenderInterface> webrtc_rtp_sender,
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_adapter)
    : webrtc_rtp_sender_(std::move(webrtc_rtp_sender)),
      track_adapter_(std::move(track_adapter)) {
  DCHECK(webrtc_rtp_sender_);
  DCHECK(track_adapter_ == webrtc_rtp_sender_->track());
}

RTCRtpSender::~RTCRtpSender() {}

uintptr_t RTCRtpSender::Id() const {
  return getId(webrtc_rtp_sender_.get());
}

const blink::WebMediaStreamTrack* RTCRtpSender::Track() const {
  DCHECK(track_adapter_ == webrtc_rtp_sender_->track());
  return track_adapter_ ? &track_adapter_->web_track() : nullptr;
}

const webrtc::MediaStreamTrackInterface* RTCRtpSender::webrtc_track() const {
  DCHECK(track_adapter_ == webrtc_rtp_sender_->track());
  return track_adapter_ ? track_adapter_->webrtc_track() : nullptr;
}

}  // namespace content
