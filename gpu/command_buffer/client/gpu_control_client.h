// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_CLIENT_H_
#define GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_CLIENT_H_

#include <cstdint>

namespace gpu {

class GpuControlClient {
 public:
  virtual void OnGpuControlLostContext() = 0;
  virtual void OnGpuControlErrorMessage(const char* message, int32_t id) = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GPU_CONTROL_CLIENT_H_
