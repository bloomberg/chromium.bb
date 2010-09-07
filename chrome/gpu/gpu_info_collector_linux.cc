// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_info_collector.h"

namespace gpu_info_collector {

bool CollectGraphicsInfo(GPUInfo* gpu_info) {
  // TODO(rlp): complete this function
  // TODO(apatrick): this illustrates how can_lose_context will be implemented
  // on this platform in the future.
  // bool can_lose_context =
  //     gfx::GetGLImplementation() == gfx::kGLImplementationEGLGLES2;
  return true;
}

}  // namespace gpu_info_collector
