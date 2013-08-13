// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_decoder_factory.h"

#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/renderer_gpu_video_accelerator_factories.h"
#include "content/renderer/media/rtc_video_decoder.h"

namespace content {

RTCVideoDecoderFactory::RTCVideoDecoderFactory(
    const scoped_refptr<RendererGpuVideoAcceleratorFactories>& gpu_factories)
    : gpu_factories_(gpu_factories) {
  DVLOG(2) << "RTCVideoDecoderFactory";
}

RTCVideoDecoderFactory::~RTCVideoDecoderFactory() {
  DVLOG(2) << "~RTCVideoDecoderFactory";
}

webrtc::VideoDecoder* RTCVideoDecoderFactory::CreateVideoDecoder(
    webrtc::VideoCodecType type) {
  DVLOG(2) << "CreateVideoDecoder";
  // GpuVideoAcceleratorFactories is not thread safe. It cannot be shared
  // by different decoders. This method runs on Chrome_libJingle_WorkerThread
  // and the child thread is blocked while this runs. We cannot create new gpu
  // factories here. Clone one instead.
  scoped_ptr<RTCVideoDecoder> decoder =
      RTCVideoDecoder::Create(type, gpu_factories_->Clone());
  return decoder.release();
}

void RTCVideoDecoderFactory::DestroyVideoDecoder(
    webrtc::VideoDecoder* decoder) {
  DVLOG(2) << "DestroyVideoDecoder";
  gpu_factories_->GetMessageLoop()->DeleteSoon(FROM_HERE, decoder);
}

}  // namespace content
