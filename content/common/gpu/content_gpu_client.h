// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CONTENT_GPU_CLIENT_H_
#define CONTENT_COMMON_GPU_CONTENT_GPU_CLIENT_H_
#pragma once

#include "content/common/content_client.h"

struct GPUInfo;

namespace content {

// Embedder API for participating in plugin logic.
class ContentGpuClient {
 public:
  // Sets the data on the gpu to send along with crash reports.
  virtual void SetGpuInfo(const GPUInfo& gpu_info) {}
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CONTENT_GPU_CLIENT_H_
