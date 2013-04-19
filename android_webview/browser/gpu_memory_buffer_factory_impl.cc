// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gpu_memory_buffer_factory_impl.h"

#include "android_webview/browser/gpu_memory_buffer_impl.h"
#include "base/logging.h"
#include "ui/gfx/size.h"

namespace android_webview {

scoped_ptr<gpu::GpuMemoryBuffer> CreateGpuMemoryBuffer(int width, int height) {
  DCHECK(width > 0);
  DCHECK(height > 0);
  scoped_ptr<GpuMemoryBufferImpl> result(new GpuMemoryBufferImpl(
      gfx::Size(width, height)));
  return result.PassAs<gpu::GpuMemoryBuffer>();
}

}  // namespace android_webview
