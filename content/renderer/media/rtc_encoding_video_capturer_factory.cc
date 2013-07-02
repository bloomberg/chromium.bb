// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_encoding_video_capturer_factory.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/renderer/media/rtc_encoding_video_capturer.h"
#include "media/base/encoded_bitstream_buffer.h"

namespace content {

RtcEncodingVideoCapturerFactory::RtcEncodingVideoCapturerFactory()
    : encoded_video_source_(NULL) {
}

RtcEncodingVideoCapturerFactory::~RtcEncodingVideoCapturerFactory() {
  DCHECK(encoder_set_.empty());
}

void RtcEncodingVideoCapturerFactory::OnEncodedVideoSourceAdded(
    media::EncodedVideoSource* source) {
  // TODO(hshi): support multiple encoded video sources.
  // For now we only support one instance of encoded video source at a time.
  DCHECK(!encoded_video_source_);
  encoded_video_source_ = source;
  source->RequestCapabilities(base::Bind(
      &RtcEncodingVideoCapturerFactory::OnCapabilitiesAvailable,
      AsWeakPtr()));
}

void RtcEncodingVideoCapturerFactory::OnEncodedVideoSourceRemoved(
    media::EncodedVideoSource* source) {
  encoded_video_source_ = NULL;
}

webrtc::VideoEncoder* RtcEncodingVideoCapturerFactory::CreateVideoEncoder(
    webrtc::VideoCodecType type) {
  for (size_t i = 0; i < codecs_.size(); ++i) {
    if (codecs_[i].type == type) {
      RtcEncodingVideoCapturer* capturer =
          new RtcEncodingVideoCapturer(encoded_video_source_, type);
      encoder_set_.insert(capturer);
      return capturer;
    }
  }
  return NULL;
}

void RtcEncodingVideoCapturerFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  EncoderSet::iterator it = encoder_set_.find(encoder);
  if (it != encoder_set_.end()) {
      delete encoder;
      encoder_set_.erase(it);
  }
}

void RtcEncodingVideoCapturerFactory::AddObserver(
    WebRtcVideoEncoderFactory::Observer* observer) {
  observers_.AddObserver(observer);
}

void RtcEncodingVideoCapturerFactory::RemoveObserver(
    WebRtcVideoEncoderFactory::Observer* observer) {
  observers_.RemoveObserver(observer);
}

const std::vector<cricket::WebRtcVideoEncoderFactory::VideoCodec>&
RtcEncodingVideoCapturerFactory::codecs() const {
  return codecs_;
}

void RtcEncodingVideoCapturerFactory::OnCapabilitiesAvailable(
    const media::VideoEncodingCapabilities& caps) {
  codecs_.clear();

  for (size_t i = 0; i < caps.size(); ++i) {
    webrtc::VideoCodecType webrtc_codec_type = webrtc::kVideoCodecGeneric;
    switch (caps[i].codec_type) {
      case media::kCodecVP8:
        webrtc_codec_type = webrtc::kVideoCodecVP8;
        break;
      default:
        break;
    }
    codecs_.push_back(cricket::WebRtcVideoEncoderFactory::VideoCodec(
        webrtc_codec_type,
        caps[i].codec_name,
        caps[i].max_resolution.width(),
        caps[i].max_resolution.height(),
        caps[i].max_frames_per_second));
  }

  FOR_EACH_OBSERVER(WebRtcVideoEncoderFactory::Observer, observers_,
                    OnCodecsAvailable());
}

}  // namespace content
