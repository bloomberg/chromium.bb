// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_sender.h"

#include "base/logging.h"

namespace content {

class RTCRtpSender::RTCRtpSenderInternal
    : public base::RefCountedThreadSafe<RTCRtpSender::RTCRtpSenderInternal> {
 public:
  RTCRtpSenderInternal(
      rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref,
      std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
          stream_refs)
      : webrtc_sender_(std::move(webrtc_sender)),
        track_ref_(std::move(track_ref)),
        stream_refs_(std::move(stream_refs)) {
    DCHECK(webrtc_sender_);
  }

  webrtc::RtpSenderInterface* webrtc_sender() const {
    return webrtc_sender_.get();
  }

  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref()
      const {
    return track_ref_ ? track_ref_->Copy() : nullptr;
  }

  std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
  stream_refs() const {
    std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
        stream_ref_copies(stream_refs_.size());
    for (size_t i = 0; i < stream_refs_.size(); ++i)
      stream_ref_copies[i] = stream_refs_[i]->Copy();
    return stream_ref_copies;
  }

  void OnRemoved() {
    // TODO(hbos): We should do |webrtc_sender_->SetTrack(null)| but that is
    // not allowed on a stopped sender. https://crbug.com/webrtc/7945
    track_ref_.reset();
  }

 private:
  friend class base::RefCountedThreadSafe<RTCRtpSenderInternal>;
  virtual ~RTCRtpSenderInternal() {}

  const rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender_;
  // The track adapter is the glue between blink and webrtc layer tracks.
  // Keeping a reference to the adapter ensures it is not disposed, as is
  // required as long as the webrtc layer track is in use by the sender.
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref_;
  // Similarly, reference needs to be kept to the stream adapters of the
  // sender's associated set of streams.
  std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
      stream_refs_;
};

uintptr_t RTCRtpSender::getId(const webrtc::RtpSenderInterface* webrtc_sender) {
  return reinterpret_cast<uintptr_t>(webrtc_sender);
}

RTCRtpSender::RTCRtpSender(
    rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender,
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref)
    : RTCRtpSender(
          std::move(webrtc_sender),
          std::move(track_ref),
          std::vector<
              std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>()) {}

RTCRtpSender::RTCRtpSender(
    rtc::scoped_refptr<webrtc::RtpSenderInterface> webrtc_sender,
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref,
    std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
        stream_refs)
    : internal_(new RTCRtpSenderInternal(std::move(webrtc_sender),
                                         std::move(track_ref),
                                         std::move(stream_refs))) {}

RTCRtpSender::RTCRtpSender(const RTCRtpSender& other)
    : internal_(other.internal_) {}

RTCRtpSender::~RTCRtpSender() {}

RTCRtpSender& RTCRtpSender::operator=(const RTCRtpSender& other) {
  internal_ = other.internal_;
  return *this;
}

std::unique_ptr<RTCRtpSender> RTCRtpSender::ShallowCopy() const {
  return std::make_unique<RTCRtpSender>(*this);
}

uintptr_t RTCRtpSender::Id() const {
  return getId(internal_->webrtc_sender());
}

blink::WebMediaStreamTrack RTCRtpSender::Track() const {
  auto track_ref = internal_->track_ref();
  return track_ref ? track_ref->web_track() : blink::WebMediaStreamTrack();
}

void RTCRtpSender::OnRemoved() {
  internal_->OnRemoved();
}

webrtc::RtpSenderInterface* RTCRtpSender::webrtc_sender() const {
  return internal_->webrtc_sender();
}

const webrtc::MediaStreamTrackInterface* RTCRtpSender::webrtc_track() const {
  auto track_ref = internal_->track_ref();
  return track_ref ? track_ref->webrtc_track() : nullptr;
}

}  // namespace content
