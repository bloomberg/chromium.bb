// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_CONTENT_GPU_CLIENT_H_
#define CHROME_COMMON_CHROME_CONTENT_GPU_CLIENT_H_
#pragma once

#include "content/common/gpu/content_gpu_client.h"

namespace chrome {

class ChromeContentGpuClient : public content::ContentGpuClient {
 public:
  virtual void SetGpuInfo(const GPUInfo& gpu_info);
};

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONTENT_GPU_CLIENT_H_
