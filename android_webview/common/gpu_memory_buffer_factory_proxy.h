// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_GPU_MEMORY_BUFFER_FACTORY_PROXY_H_
#define ANDROID_WEBVIEW_COMMON_GPU_MEMORY_BUFFER_FACTORY_PROXY_H_

#include "base/basictypes.h"
#include "ui/gl/gpu_memory_buffer.h"

namespace android_webview {

const gfx::GpuMemoryBuffer::Create& GetGpuMemoryBufferFactoryProxy();
void SetGpuMemoryBufferFactoryProxy(
    const gfx::GpuMemoryBuffer::Create& factory);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_GPU_MEMORY_BUFFER_FACTORY_PROXY_H_
