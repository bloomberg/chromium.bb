// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_FACTORY_H_
#define GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_FACTORY_H_

#include "gles2_impl_export.h"
#include "gpu/command_buffer/client/gpu_memory_buffer.h"

namespace gpu {

// Getter and setter for a GpuMemoryBuffer factory for the current process.
// Currently it is only used for Android Webview where both browser and
// renderer are within the same process.

// It is not valid to call this method before the setter is called.
GLES2_IMPL_EXPORT const GpuMemoryBuffer::Creator&
    GetProcessDefaultGpuMemoryBufferFactory();

// It is illegal to call the setter more than once.
GLES2_IMPL_EXPORT void SetProcessDefaultGpuMemoryBufferFactory(
    const GpuMemoryBuffer::Creator& factory);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_FACTORY_H_
