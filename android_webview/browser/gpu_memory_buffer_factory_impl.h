// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_FACTORY_IMPL_H_
#define ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_FACTORY_IMPL_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gl/gpu_memory_buffer.h"

namespace gfx {
class Size;
}

namespace android_webview {

scoped_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(gfx::Size size);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GPU_MEMORY_BUFFER_FACTORY_IMPL_H_
