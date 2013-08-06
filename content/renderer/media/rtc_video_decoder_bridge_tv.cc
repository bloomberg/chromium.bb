// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_decoder_bridge_tv.h"

#include <queue>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/time/time.h"
#include "content/renderer/media/rtc_video_decoder_factory_tv.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "third_party/libjingle/source/talk/base/ratetracker.h"

namespace content {

RTCVideoDecoderBridgeTv::RTCVideoDecoderBridgeTv(
    RTCVideoDecoderFactoryTv* factory)
    : factory_(factory),
      is_initialized_(false),
      first_frame_(true) {}

RTCVideoDecoderBridgeTv::~RTCVideoDecoderBridgeTv() {}

int32_t RTCVideoDecoderBridgeTv::InitDecode(
    const webrtc::VideoCodec* codec_settings,
    int32_t number_of_cores) {
  // We don't support non-VP8 codec, feedback mode, nor double-initialization
  if (codec_settings->codecType != webrtc::kVideoCodecVP8 ||
      codec_settings->codecSpecific.VP8.feedbackModeOn || is_initialized_)
    return WEBRTC_VIDEO_CODEC_ERROR;
  size_ = gfx::Size(codec_settings->width, codec_settings->height);

  is_initialized_ = true;
  first_frame_ = true;
  factory_->InitializeStream(size_);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoDecoderBridgeTv::Decode(
    const webrtc::EncodedImage& input_image,
    bool missing_frames,
    const webrtc::RTPFragmentationHeader* fragmentation,
    const webrtc::CodecSpecificInfo* codec_specific_info,
    int64_t render_time_ms) {
  // Unlike the SW decoder in libvpx, hw decoder can not handle broken frames.
  // Here, we return an error in order to request a key frame.
  if (missing_frames || !input_image._completeFrame)
    return WEBRTC_VIDEO_CODEC_ERROR;

  if (!is_initialized_)
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;

  if (first_frame_) {
    // If the first frame is not a key frame, return an error to request a key
    // frame.
    if (input_image._frameType != webrtc::kKeyFrame)
      return WEBRTC_VIDEO_CODEC_ERROR;

    // Google TV expects timestamp from 0, so we store the initial timestamp as
    // an offset and subtract the value from every timestamps to meet the
    // expectation.
    timestamp_offset_millis_ = render_time_ms;
  }
  first_frame_ = false;
  gfx::Size new_size;
  if (input_image._frameType == webrtc::kKeyFrame &&
      input_image._encodedWidth != 0 && input_image._encodedHeight != 0) {
    // Only a key frame has a meaningful size.
    new_size.SetSize(input_image._encodedWidth, input_image._encodedHeight);
    if (size_ == new_size)
      new_size = gfx::Size();
    else
      size_ = new_size;
  }
  // |input_image_| may be destroyed after this call, so we make a copy of the
  // buffer so that we can queue the buffer asynchronously.
  scoped_refptr<media::DecoderBuffer> buffer =
      media::DecoderBuffer::CopyFrom(input_image._buffer, input_image._length);
  if (render_time_ms != -1) {
    buffer->set_timestamp(base::TimeDelta::FromMilliseconds(
        render_time_ms - timestamp_offset_millis_));
  }

  factory_->QueueBuffer(buffer, new_size);

  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoDecoderBridgeTv::RegisterDecodeCompleteCallback(
    webrtc::DecodedImageCallback* callback) {
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoDecoderBridgeTv::Release() {
  is_initialized_ = false;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t RTCVideoDecoderBridgeTv::Reset() {
  first_frame_ = true;
  return WEBRTC_VIDEO_CODEC_OK;
}

}  // namespace content
