// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_VIDEO_CODEC_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_VIDEO_CODEC_FACTORY_H_

#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"

namespace content {

std::unique_ptr<webrtc::VideoEncoderFactory> CreateWebrtcVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories);
std::unique_ptr<webrtc::VideoDecoderFactory> CreateWebrtcVideoDecoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_VIDEO_CODEC_FACTORY_H_
