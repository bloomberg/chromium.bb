// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_FACTORY_H_

#include "base/macros.h"
#include "base/threading/thread.h"
#include "content/common/content_export.h"
#include "third_party/webrtc/api/video_codecs/video_decoder_factory.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"

namespace webrtc {
class VideoDecoder;
}  // namespace webrtc

namespace media {
class GpuVideoAcceleratorFactories;
}  // namespace media

namespace content {

// TODO(wuchengli): add unittest.
class CONTENT_EXPORT RTCVideoDecoderFactory
    : public webrtc::VideoDecoderFactory {
 public:
  explicit RTCVideoDecoderFactory(
      media::GpuVideoAcceleratorFactories* gpu_factories);
  ~RTCVideoDecoderFactory() override;

  // Runs on Chrome_libJingle_WorkerThread. The child thread is blocked while
  // this runs.
  std::unique_ptr<webrtc::VideoDecoder> CreateVideoDecoder(
      const webrtc::SdpVideoFormat& format) override;

  std::vector<webrtc::SdpVideoFormat> GetSupportedFormats() const override;

 private:
  media::GpuVideoAcceleratorFactories* gpu_factories_;
  std::vector<webrtc::SdpVideoFormat> supported_formats_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderFactory);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_FACTORY_H_
