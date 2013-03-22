// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DRIVER_BUG_LIST_H_
#define CONTENT_BROWSER_GPU_GPU_DRIVER_BUG_LIST_H_

#include <string>

#include "content/browser/gpu/gpu_control_list.h"

namespace content {

class CONTENT_EXPORT GpuDriverBugList : public GpuControlList {
 public:
  virtual ~GpuDriverBugList();

  static GpuDriverBugList* Create();

 private:
  GpuDriverBugList();

  DISALLOW_COPY_AND_ASSIGN(GpuDriverBugList);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_DRIVER_BUG_LIST_H_

