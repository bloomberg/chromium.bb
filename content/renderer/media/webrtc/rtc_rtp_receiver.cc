// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_receiver.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/renderer/media/webrtc/rtc_rtp_contributing_source.h"
#include "third_party/webrtc/rtc_base/scoped_ref_ptr.h"

namespace content {

uintptr_t RTCRtpReceiver::getId(
    const webrtc::RtpReceiverInterface* webrtc_rtp_receiver) {
  return reinterpret_cast<uintptr_t>(webrtc_rtp_receiver);
}

RTCRtpReceiver::RTCRtpReceiver(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> webrtc_rtp_receiver,
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_adapter)
    : webrtc_rtp_receiver_(std::move(webrtc_rtp_receiver)),
      track_adapter_(std::move(track_adapter)) {
  DCHECK(webrtc_rtp_receiver_);
  DCHECK(track_adapter_);
  DCHECK_EQ(track_adapter_->webrtc_track(), webrtc_rtp_receiver_->track());
}

RTCRtpReceiver::~RTCRtpReceiver() {}

uintptr_t RTCRtpReceiver::Id() const {
  return getId(webrtc_rtp_receiver_.get());
}

const blink::WebMediaStreamTrack& RTCRtpReceiver::Track() const {
  DCHECK(track_adapter_->webrtc_track() == webrtc_rtp_receiver_->track());
  return track_adapter_->web_track();
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
  DCHECK(track_adapter_->webrtc_track() == webrtc_rtp_receiver_->track());
  DCHECK(track_adapter_->webrtc_track());
  return *track_adapter_->webrtc_track();
}

}  // namespace content
