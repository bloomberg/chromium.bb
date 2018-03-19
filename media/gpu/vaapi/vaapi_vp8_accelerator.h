// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VP8_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_VP8_ACCELERATOR_H_

#include "base/sequence_checker.h"
#include "media/filters/vp8_parser.h"
#include "media/gpu/vp8_decoder.h"

namespace media {

class VP8Picture;
class VaapiVideoDecodeAccelerator;
class VaapiWrapper;

class VaapiVP8Accelerator : public VP8Decoder::VP8Accelerator {
 public:
  VaapiVP8Accelerator(VaapiVideoDecodeAccelerator* vaapi_dec,
                      scoped_refptr<VaapiWrapper> vaapi_wrapper);
  ~VaapiVP8Accelerator() override;

  // VP8Decoder::VP8Accelerator implementation.
  scoped_refptr<VP8Picture> CreateVP8Picture() override;
  bool SubmitDecode(const scoped_refptr<VP8Picture>& pic,
                    const Vp8FrameHeader* frame_hdr,
                    const scoped_refptr<VP8Picture>& last_frame,
                    const scoped_refptr<VP8Picture>& golden_frame,
                    const scoped_refptr<VP8Picture>& alt_frame) override;
  bool OutputPicture(const scoped_refptr<VP8Picture>& pic) override;

 private:
  const scoped_refptr<VaapiWrapper> vaapi_wrapper_;
  VaapiVideoDecodeAccelerator* vaapi_dec_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VaapiVP8Accelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VP8_ACCELERATOR_H_
