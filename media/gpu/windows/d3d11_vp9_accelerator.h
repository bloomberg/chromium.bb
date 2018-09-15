// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VP9_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_D3D11_VP9_ACCELERATOR_H_

#include "media/gpu/vp9_decoder.h"

namespace media {

class D3D11VP9Accelerator : public VP9Decoder::VP9Accelerator {
 public:
  D3D11VP9Accelerator();
  ~D3D11VP9Accelerator() override;

  scoped_refptr<VP9Picture> CreateVP9Picture() override;

  bool SubmitDecode(
      const scoped_refptr<VP9Picture>& picture,
      const Vp9SegmentationParams& segmentation_params,
      const Vp9LoopFilterParams& loop_filter_params,
      const std::vector<scoped_refptr<VP9Picture>>& reference_pictures,
      const base::Closure& on_finished_cb) override;

  bool OutputPicture(const scoped_refptr<VP9Picture>& picture) override;

  bool IsFrameContextRequired() const override;

  bool GetFrameContext(const scoped_refptr<VP9Picture>& picture,
                       Vp9FrameContext* frame_context) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(D3D11VP9Accelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VP9_ACCELERATOR_H_
