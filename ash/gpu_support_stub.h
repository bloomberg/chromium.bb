// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GPU_SUPPORT_STUB_H_
#define ASH_GPU_SUPPORT_STUB_H_

#include "ash/ash_export.h"
#include "ash/gpu_support.h"
#include "base/compiler_specific.h"

namespace ash {

// A GPUSupport object that does not depend on src/content.
class ASH_EXPORT GPUSupportStub : public GPUSupport {
 public:
  GPUSupportStub();
  virtual ~GPUSupportStub();

 private:
  // Overridden from GPUSupport:
  virtual bool IsPanelFittingDisabled() const OVERRIDE;
  virtual void DisableGpuWatchdog() OVERRIDE;
  virtual void GetGpuProcessHandles(
      const GetGpuProcessHandlesCallback& callback) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(GPUSupportStub);
};

}  // namespace ash

#endif  // ASH_GPU_SUPPORT_STUB_H_
