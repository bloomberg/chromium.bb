// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_GPU_SUPPORT_IMPL_H_
#define ASH_CONTENT_GPU_SUPPORT_IMPL_H_

#include "ash/common/gpu_support.h"
#include "ash/content/ash_with_content_export.h"
#include "base/macros.h"

namespace ash {

// Support for a real GPU, which relies on access to src/content.
class ASH_WITH_CONTENT_EXPORT GPUSupportImpl : public GPUSupport {
 public:
  GPUSupportImpl();
  ~GPUSupportImpl() override;

 private:
  // Overridden from GPUSupport:
  bool IsPanelFittingDisabled() const override;
  void GetGpuProcessHandles(
      const GetGpuProcessHandlesCallback& callback) const override;

  DISALLOW_COPY_AND_ASSIGN(GPUSupportImpl);
};

}  // namespace ash

#endif  // ASH_CONTENT_GPU_SUPPORT_IMPL_H_
