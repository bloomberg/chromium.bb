// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_info_collector.h"

#include "base/logging.h"

namespace gpu_info_collector {

bool CollectGraphicsInfo(content::GPUInfo* gpu_info) {
  // can_lose_context must be false to enable accelerated Canvas2D
  gpu_info->can_lose_context = false;
  gpu_info->finalized = true;
  return CollectGraphicsInfoGL(gpu_info);
}

bool CollectPreliminaryGraphicsInfo(content::GPUInfo* gpu_info) {
  return true;
}

bool CollectVideoCardInfo(content::GPUInfo* gpu_info) {
  return true;
}

bool CollectDriverInfoGL(content::GPUInfo* gpu_info) {
  return true;
}

}  // namespace gpu_info_collector
