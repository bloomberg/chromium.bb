// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_BRIDGE_TV_H_
#define CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_BRIDGE_TV_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"
#include "third_party/webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "ui/gfx/size.h"

namespace content {

class MediaStreamDependencyFactory;
class RTCVideoDecoderFactoryTv;

// An object shared between WebMediaPlayerAndroid and WebRTC Video Engine.
// Note that this class provides the first remote stream.
class CONTENT_EXPORT RTCVideoDecoderBridgeTv
    : NON_EXPORTED_BASE(public webrtc::VideoDecoder) {
 public:
  explicit RTCVideoDecoderBridgeTv(RTCVideoDecoderFactoryTv* factory);
  virtual ~RTCVideoDecoderBridgeTv();

  // webrtc::VideoDecoder implementation.
  virtual int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                             int32_t number_of_cores) OVERRIDE;
  virtual int32_t Decode(
      const webrtc::EncodedImage& input_image,
      bool missing_frames,
      const webrtc::RTPFragmentationHeader* fragmentation,
      const webrtc::CodecSpecificInfo* codec_specific_info,
      int64_t render_time_ms) OVERRIDE;
  virtual int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) OVERRIDE;
  virtual int32_t Release() OVERRIDE;
  virtual int32_t Reset() OVERRIDE;

 private:
  // The factory outlives this object, so weak pointer is fine.
  RTCVideoDecoderFactoryTv* factory_;

  gfx::Size size_;
  bool is_initialized_;
  bool first_frame_;
  int64_t timestamp_offset_millis_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderBridgeTv);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_RTC_VIDEO_DECODER_BRIDGE_TV_H_
