// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_GPU_SUPPORT_STUB_H_
#define ASH_COMMON_GPU_SUPPORT_STUB_H_

#include "ash/ash_export.h"
#include "ash/common/gpu_support.h"
#include "base/macros.h"

namespace ash {

// A GPUSupport object that does not depend on src/content.
class ASH_EXPORT GPUSupportStub : public GPUSupport {
 public:
  GPUSupportStub();
  ~GPUSupportStub() override;

 private:
  // Overridden from GPUSupport:
  bool IsPanelFittingDisabled() const override;
  void GetGpuProcessHandles(
      const GetGpuProcessHandlesCallback& callback) const override;

  DISALLOW_COPY_AND_ASSIGN(GPUSupportStub);
};

}  // namespace ash

#endif  // ASH_COMMON_GPU_SUPPORT_STUB_H_
