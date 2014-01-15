// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTENT_SUPPORT_GPU_SUPPORT_IMPL_H_
#define ASH_CONTENT_SUPPORT_GPU_SUPPORT_IMPL_H_

#include "ash/gpu_support.h"

namespace ash {

class GPUSupportImpl : public GPUSupport {
 public:
  GPUSupportImpl();
  virtual ~GPUSupportImpl();

 private:
  // Overridden from GPUSupport:
  virtual bool IsPanelFittingDisabled() const OVERRIDE;
  virtual void DisableGpuWatchdog() OVERRIDE;
  virtual void GetGpuProcessHandles(
      const GetGpuProcessHandlesCallback& callback) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(GPUSupportImpl);
};

}  // namespace ash

#endif  // ASH_CONTENT_SUPPORT_GPU_SUPPORT_IMPL_H_
