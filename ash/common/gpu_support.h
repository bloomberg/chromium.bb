// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_GPU_SUPPORT_H_
#define ASH_COMMON_GPU_SUPPORT_H_

#include <list>

#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/process/process.h"

namespace ash {

// An interface to allow use of content::GpuDataManager to be injected in
// configurations that permit a dependency on content.
class ASH_EXPORT GPUSupport {
 public:
  typedef base::Callback<void(const std::list<base::ProcessHandle>&)>
      GetGpuProcessHandlesCallback;

  virtual ~GPUSupport() {}

  virtual bool IsPanelFittingDisabled() const = 0;

  virtual void GetGpuProcessHandles(
      const GetGpuProcessHandlesCallback& callback) const = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_GPU_SUPPORT_H_
