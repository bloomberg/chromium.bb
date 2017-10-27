// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_sender.h"

#include "base/logging.h"

namespace content {

namespace {

inline bool operator==(
    const std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>&
        track_adapter,
    const webrtc::MediaStreamTrackInterface* webrtc_track) {
  if (!track_adapter) {
    // TODO(hbos): See TODO in |OnRemoved|. Because we can't set a stopped
    // sender's track to null any |webrtc_track| could be correct. When this is
    // fixed this line should be: return !webrtc_track;
    // https://crbug.com/webrtc/7945
    return true;
  }
  return track_adapter->webrtc_track() == webrtc_track;
}

}  // namespace

uintptr_t RTCRtpSender::getId(
    const webrtc::RtpSenderInterface* webrtc_rtp_sender) {
  return reinterpret_cast<uintptr_t>(webrtc_rtp_sender);
}

RTCRtpSender::RTCRtpSender(
    rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_rtp_sender,
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_adapter)
    : RTCRtpSender(
          std::move(webrtc_rtp_sender),
          std::move(track_adapter),
          std::vector<
              std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>()) {}

RTCRtpSender::RTCRtpSender(
    rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_rtp_sender,
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_adapter,
    std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
        stream_adapters)
    : webrtc_rtp_sender_(std::move(webrtc_rtp_sender)),
      track_adapter_(std::move(track_adapter)),
      stream_adapters_(std::move(stream_adapters)) {
  DCHECK(webrtc_rtp_sender_);
  DCHECK(track_adapter_ == webrtc_rtp_sender_->track());
}

RTCRtpSender::~RTCRtpSender() {}

std::unique_ptr<RTCRtpSender> RTCRtpSender::ShallowCopy() const {
  std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
      stream_adapter_copies(stream_adapters_.size());
  for (size_t i = 0; i < stream_adapters_.size(); ++i) {
    stream_adapter_copies[i] = stream_adapters_[i]->Copy();
  }
  return std::make_unique<RTCRtpSender>(webrtc_rtp_sender_,
                                        track_adapter_->Copy(),
                                        std::move(stream_adapter_copies));
}

uintptr_t RTCRtpSender::Id() const {
  return getId(webrtc_rtp_sender_.get());
}

const blink::WebMediaStreamTrack* RTCRtpSender::Track() const {
  DCHECK(track_adapter_ == webrtc_rtp_sender_->track());
  return track_adapter_ ? &track_adapter_->web_track() : nullptr;
}

void RTCRtpSender::OnRemoved() {
  // TODO(hbos): We should do |webrtc_rtp_sender_->SetTrack(null)| but that is
  // not allowed on a stopped sender. https://crbug.com/webrtc/7945
  track_adapter_.reset();
}

webrtc::RtpSenderInterface* RTCRtpSender::webrtc_rtp_sender() {
  return webrtc_rtp_sender_.get();
}

const webrtc::MediaStreamTrackInterface* RTCRtpSender::webrtc_track() const {
  DCHECK(track_adapter_ == webrtc_rtp_sender_->track());
  return track_adapter_ ? track_adapter_->webrtc_track() : nullptr;
}

}  // namespace content
