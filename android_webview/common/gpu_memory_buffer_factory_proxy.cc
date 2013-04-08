// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/gpu_memory_buffer_factory_proxy.h"

#include "android_webview/common/aw_switches.h"
#include "base/command_line.h"
#include "base/logging.h"

namespace android_webview {

gfx::GpuMemoryBuffer::Create* g_pixel_buffer_factory_ = NULL;

const gfx::GpuMemoryBuffer::Create& GetGpuMemoryBufferFactoryProxy() {
  return *g_pixel_buffer_factory_;
}

void SetGpuMemoryBufferFactoryProxy(
    const gfx::GpuMemoryBuffer::Create& factory) {
  DCHECK(g_pixel_buffer_factory_ == NULL);
  g_pixel_buffer_factory_ =
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kUseZeroCopyBuffers)
      ? new gfx::GpuMemoryBuffer::Create(factory) : NULL;
}

}  // namespace android_webview
