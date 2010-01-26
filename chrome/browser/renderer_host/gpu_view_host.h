// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_GPU_VIEW_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_GPU_VIEW_HOST_H_

#include "base/basictypes.h"
#include "chrome/common/gpu_native_window_handle.h"

class BackingStore;
class GpuProcessHost;
class RenderWidgetHost;

namespace gfx {
class Size;
}

// A proxy for the GPU process' window for rendering pages.
class GpuViewHost {
 public:
  GpuViewHost(RenderWidgetHost* widget, GpuNativeWindowHandle parent);
  ~GpuViewHost();

  // Creates a new backing store in the GPU process and returns ownership of
  // the new pointer to the caller.
  BackingStore* CreateBackingStore(const gfx::Size& size);

 private:
  RenderWidgetHost* widget_;

  GpuProcessHost* process_;
  int32 routing_id_;

  DISALLOW_COPY_AND_ASSIGN(GpuViewHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_GPU_VIEW_HOST_H_
