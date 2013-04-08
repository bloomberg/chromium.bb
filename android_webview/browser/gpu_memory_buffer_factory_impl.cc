// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gpu_memory_buffer_factory_impl.h"

#include "android_webview/browser/gpu_memory_buffer_impl.h"

namespace android_webview {

scoped_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(gfx::Size size) {
  scoped_ptr<GpuMemoryBufferImpl> result(new GpuMemoryBufferImpl(size));
  return result.PassAs<gfx::GpuMemoryBuffer>();
}

}  // namespace android_webview
