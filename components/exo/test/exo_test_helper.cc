// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_helper.h"

#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"

namespace exo {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// ExoTestHelper, public:

ExoTestHelper::ExoTestHelper() {}

ExoTestHelper::~ExoTestHelper() {}

std::unique_ptr<gfx::GpuMemoryBuffer> ExoTestHelper::CreateGpuMemoryBuffer(
    const gfx::Size& size) {
  return aura::Env::GetInstance()
      ->context_factory()
      ->GetGpuMemoryBufferManager()
      ->AllocateGpuMemoryBuffer(size, gfx::BufferFormat::RGBA_8888,
                                gfx::BufferUsage::GPU_READ,
                                gpu::kNullSurfaceHandle);
}

}  // namespace test
}  // namespace exo
