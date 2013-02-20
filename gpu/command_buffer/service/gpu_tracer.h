// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the GPUTrace class.
#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_TRACER_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_TRACER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace gpu {
namespace gles2 {

// Traces GPU Commands.
class GPUTracer {
 public:
  static scoped_ptr<GPUTracer> Create();

  GPUTracer() {}
  virtual ~GPUTracer() {}

  // Begin a trace.
  virtual bool Begin(const std::string& name) = 0;

  // End the last started trace.
  virtual bool End() = 0;

  // Retrieve the name of the current open trace.
  // Returns empty string if no current open trace.
  virtual const std::string& CurrentName() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUTracer);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_TRACER_H_
