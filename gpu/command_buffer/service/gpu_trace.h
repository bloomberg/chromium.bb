// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GPUTrace class.
#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_TRACE_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_TRACE_H_

#include <string>

#include "base/basictypes.h"

namespace gpu {
namespace gles2 {

class GPUTrace {
 public:
  explicit GPUTrace(const std::string& name);
  ~GPUTrace();

  void EnableStartTrace();
  void EnableEndTrace();

  const char* name();

 private:
  std::string name_;

  DISALLOW_COPY_AND_ASSIGN(GPUTrace);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_TRACE_H_
