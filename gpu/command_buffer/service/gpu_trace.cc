// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_trace.h"

namespace gpu {
namespace gles2 {

GPUTrace::GPUTrace(const std::string& name)
    : name_(name) {
}

GPUTrace::~GPUTrace() {
}

void GPUTrace::EnableStartTrace() {
  // TODO(dsinclair): no-op for now.
}

void GPUTrace::EnableEndTrace() {
  // TODO(dsinclair): no-op for now.
}

const char* GPUTrace::name() {
  return name_.c_str();
}

}  // namespace gles2
}  // namespace gpu
