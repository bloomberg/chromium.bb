// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_memory_buffer_factory_native_pixmap.h"

#include "build/build_config.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory_test_template.h"

namespace gpu {
namespace {

// On Fuchsia NativePixmap depends on Vulkan, which is not initialized in tests.
// See crbug.com/957700
#if defined(OS_FUCHSIA)
#define MAYBE_GpuMemoryBufferFactoryNativePixmap \
  DISABLED_GpuMemoryBufferFactoryNativePixmap
#else
#define MAYBE_GpuMemoryBufferFactoryNativePixmap \
  GpuMemoryBufferFactoryNativePixmap
#endif
INSTANTIATE_TYPED_TEST_SUITE_P(MAYBE_GpuMemoryBufferFactoryNativePixmap,
                               GpuMemoryBufferFactoryTest,
                               GpuMemoryBufferFactoryNativePixmap);

}  // namespace
}  // namespace gpu
