// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_decoder_factory.h"

#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "media/filters/gpu_video_decoder_factories.h"

namespace content {

RTCVideoDecoderFactory::RTCVideoDecoderFactory(
    const scoped_refptr<media::GpuVideoDecoderFactories>& gpu_factories)
    : vda_loop_proxy_(gpu_factories->GetMessageLoop()) {
  DVLOG(2) << "RTCVideoDecoderFactory";
  // The decoder cannot be created in CreateVideoDecoder because VDA has to be
  // created on |vda_loop_proxy_|, which can be the child thread. The child
  // thread is blocked when CreateVideoDecoder runs. This supports only one
  // VDA-powered <video> tag at a time.
  // TODO(wuchengli): remove this restriction.
  // |decoder_| can be null if VDA does not support VP8.
  decoder_ = RTCVideoDecoder::Create(gpu_factories);
}

RTCVideoDecoderFactory::~RTCVideoDecoderFactory() {
  DVLOG(2) << "~RTCVideoDecoderFactory";
  if (decoder_) {
    webrtc::VideoDecoder* decoder = decoder_.release();
    if (!vda_loop_proxy_->DeleteSoon(FROM_HERE, decoder))
      delete decoder;
  }
}

webrtc::VideoDecoder* RTCVideoDecoderFactory::CreateVideoDecoder(
    webrtc::VideoCodecType type) {
  DVLOG(2) << "CreateVideoDecoder";
  // Only VP8 is supported.
  if (type == webrtc::kVideoCodecVP8)
    return decoder_.release();
  return NULL;
}

void RTCVideoDecoderFactory::DestroyVideoDecoder(
    webrtc::VideoDecoder* decoder) {
  DVLOG(2) << "DestroyVideoDecoder";
  // Save back the decoder because it is the only one. VideoDecoder::Release
  // should have been called.
  decoder->RegisterDecodeCompleteCallback(NULL);
  decoder_.reset(decoder);
}

}  // namespace content
