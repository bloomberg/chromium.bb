// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_vp9_accelerator.h"

namespace media {

D3D11VP9Accelerator::D3D11VP9Accelerator() {
  // Pass
}

D3D11VP9Accelerator::~D3D11VP9Accelerator() {
  // Pass
}

scoped_refptr<VP9Picture> D3D11VP9Accelerator::CreateVP9Picture() {
  return nullptr;
}

bool D3D11VP9Accelerator::SubmitDecode(
    const scoped_refptr<VP9Picture>& picture,
    const Vp9SegmentationParams& segmentation_params,
    const Vp9LoopFilterParams& loop_filter_params,
    const std::vector<scoped_refptr<VP9Picture>>& reference_pictures,
    const base::Closure& on_finished_cb) {
  return false;
}

bool D3D11VP9Accelerator::OutputPicture(
    const scoped_refptr<VP9Picture>& picture) {
  return false;
}

bool D3D11VP9Accelerator::IsFrameContextRequired() const {
  return false;
}

bool D3D11VP9Accelerator::GetFrameContext(
    const scoped_refptr<VP9Picture>& picture,
    Vp9FrameContext* frame_context) {
  return false;
}

}  // namespace media
