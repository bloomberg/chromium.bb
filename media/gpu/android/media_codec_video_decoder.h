// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MEDIA_CODEC_VIDEO_DECODER_H_
#define MEDIA_GPU_ANDROID_MEDIA_CODEC_VIDEO_DECODER_H_

#include "base/threading/thread_checker.h"
#include "media/base/video_decoder.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// An Android VideoDecoder that delegates to MediaCodec.
class MEDIA_GPU_EXPORT MediaCodecVideoDecoder : public VideoDecoder {
 public:
  MediaCodecVideoDecoder();
  ~MediaCodecVideoDecoder() override;

  // VideoDecoder implementation:
  std::string GetDisplayName() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) override;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) override;
  void Reset(const base::Closure& closure) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaCodecVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_MEDIA_CODEC_VIDEO_DECODER_H_
