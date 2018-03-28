// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_receiver.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/renderer/media/webrtc/rtc_rtp_contributing_source.h"
#include "third_party/webrtc/rtc_base/scoped_ref_ptr.h"

namespace content {

class RTCRtpReceiver::RTCRtpReceiverInternal
    : public base::RefCountedThreadSafe<
          RTCRtpReceiver::RTCRtpReceiverInternal> {
 public:
  RTCRtpReceiverInternal(
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> webrtc_receiver,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
          track_adapter,
      std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
          stream_adapter_refs)
      : webrtc_receiver_(std::move(webrtc_receiver)),
        track_adapter_(std::move(track_adapter)),
        stream_adapter_refs_(std::move(stream_adapter_refs)) {
    DCHECK(webrtc_receiver_);
    DCHECK(track_adapter_);
  }

  const blink::WebMediaStreamTrack& Track() const {
    return track_adapter_->web_track();
  }

  blink::WebVector<blink::WebMediaStream> Streams() const {
    blink::WebVector<blink::WebMediaStream> web_streams(
        stream_adapter_refs_.size());
    for (size_t i = 0; i < stream_adapter_refs_.size(); ++i)
      web_streams[i] = stream_adapter_refs_[i]->adapter().web_stream();
    return web_streams;
  }

  blink::WebVector<std::unique_ptr<blink::WebRTCRtpContributingSource>>
  GetSources() {
    auto webrtc_sources = webrtc_receiver_->GetSources();
    blink::WebVector<std::unique_ptr<blink::WebRTCRtpContributingSource>>
        sources(webrtc_sources.size());
    for (size_t i = 0; i < webrtc_sources.size(); ++i) {
      sources[i] =
          std::make_unique<RTCRtpContributingSource>(webrtc_sources[i]);
    }
    return sources;
  }

  webrtc::RtpReceiverInterface* webrtc_receiver() const {
    return webrtc_receiver_.get();
  }

  const webrtc::MediaStreamTrackInterface& webrtc_track() const {
    DCHECK(track_adapter_->webrtc_track());
    return *track_adapter_->webrtc_track();
  }

  bool HasStream(const webrtc::MediaStreamInterface* webrtc_stream) const {
    for (const auto& stream_adapter : stream_adapter_refs_) {
      if (webrtc_stream == stream_adapter->adapter().webrtc_stream().get())
        return true;
    }
    return false;
  }

  std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
  StreamAdapterRefs() const {
    std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
        stream_adapter_copies;
    stream_adapter_copies.reserve(stream_adapter_refs_.size());
    for (const auto& stream_adapter : stream_adapter_refs_) {
      stream_adapter_copies.push_back(stream_adapter->Copy());
    }
    return stream_adapter_copies;
  }

 private:
  friend class base::RefCountedThreadSafe<RTCRtpReceiverInternal>;
  ~RTCRtpReceiverInternal() {}

  const rtc::scoped_refptr<webrtc::RtpReceiverInterface> webrtc_receiver_;

  // The track adapter is the glue between blink and webrtc layer tracks.
  // Keeping a reference to the adapter ensures it is not disposed, as is
  // required as long as the webrtc layer track is in use by the receiver.
  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_adapter_;
  // Similarly, references needs to be kept to the stream adapters of streams
  // associated with the receiver.
  std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
      stream_adapter_refs_;
};

uintptr_t RTCRtpReceiver::getId(
    const webrtc::RtpReceiverInterface* webrtc_rtp_receiver) {
  return reinterpret_cast<uintptr_t>(webrtc_rtp_receiver);
}

RTCRtpReceiver::RTCRtpReceiver(
    rtc::scoped_refptr<webrtc::RtpReceiverInterface> webrtc_receiver,
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_adapter,
    std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
        stream_adapter_refs)
    : internal_(new RTCRtpReceiverInternal(std::move(webrtc_receiver),
                                           std::move(track_adapter),
                                           std::move(stream_adapter_refs))) {}

RTCRtpReceiver::RTCRtpReceiver(const RTCRtpReceiver& other)
    : internal_(other.internal_) {}

RTCRtpReceiver::~RTCRtpReceiver() {}

RTCRtpReceiver& RTCRtpReceiver::operator=(const RTCRtpReceiver& other) {
  internal_ = other.internal_;
  return *this;
}

std::unique_ptr<RTCRtpReceiver> RTCRtpReceiver::ShallowCopy() const {
  return std::make_unique<RTCRtpReceiver>(*this);
}

uintptr_t RTCRtpReceiver::Id() const {
  return getId(internal_->webrtc_receiver());
}

const blink::WebMediaStreamTrack& RTCRtpReceiver::Track() const {
  return internal_->Track();
}

blink::WebVector<blink::WebMediaStream> RTCRtpReceiver::Streams() const {
  return internal_->Streams();
}

blink::WebVector<std::unique_ptr<blink::WebRTCRtpContributingSource>>
RTCRtpReceiver::GetSources() {
  return internal_->GetSources();
}

webrtc::RtpReceiverInterface* RTCRtpReceiver::webrtc_receiver() const {
  return internal_->webrtc_receiver();
}

const webrtc::MediaStreamTrackInterface& RTCRtpReceiver::webrtc_track() const {
  return internal_->webrtc_track();
}

bool RTCRtpReceiver::HasStream(
    const webrtc::MediaStreamInterface* webrtc_stream) const {
  return internal_->HasStream(webrtc_stream);
}

std::vector<std::unique_ptr<WebRtcMediaStreamAdapterMap::AdapterRef>>
RTCRtpReceiver::StreamAdapterRefs() const {
  return internal_->StreamAdapterRefs();
}

}  // namespace content
