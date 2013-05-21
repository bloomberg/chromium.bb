// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_DRIVER_BUG_LIST_H_
#define GPU_CONFIG_GPU_DRIVER_BUG_LIST_H_

#include <string>

#include "gpu/config/gpu_control_list.h"
#include "gpu/gpu_export.h"

namespace gpu {

class GPU_EXPORT GpuDriverBugList : public GpuControlList {
 public:
  virtual ~GpuDriverBugList();

  static GpuDriverBugList* Create();

 private:
  GpuDriverBugList();

  DISALLOW_COPY_AND_ASSIGN(GpuDriverBugList);
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_DRIVER_BUG_LIST_H_

