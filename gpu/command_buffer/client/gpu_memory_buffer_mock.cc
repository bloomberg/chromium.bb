// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gpu_memory_buffer_mock.h"

namespace gpu {

GpuMemoryBufferMock::GpuMemoryBufferMock(int width, int height) {
}

GpuMemoryBufferMock::~GpuMemoryBufferMock() {
  Die();
}

}  // namespace gpu
