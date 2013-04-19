// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_FACTORY_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_FACTORY_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gpu_memory_buffer.h"

namespace android_webview {

scoped_ptr<gpu::GpuMemoryBuffer> CreateGpuMemoryBuffer(int width, int height);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_FACTORY_IMPL_H_
