// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_BLACKLIST_H_
#define CONTENT_BROWSER_GPU_GPU_BLACKLIST_H_

#include <string>

#include "content/browser/gpu/gpu_control_list.h"

namespace content {

class CONTENT_EXPORT GpuBlacklist : public GpuControlList {
 public:
  virtual ~GpuBlacklist();

  static GpuBlacklist* Create();

 private:
  GpuBlacklist();

  DISALLOW_COPY_AND_ASSIGN(GpuBlacklist);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_BLACKLIST_H_
