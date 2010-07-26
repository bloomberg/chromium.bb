// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_GPU_VIEW_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_GPU_VIEW_HOST_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/common/gpu_native_window_handle.h"

class BackingStore;
class GpuProcessHostUIShim;
class RenderWidgetHost;
class VideoLayer;

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

  // Creates a new video layer in the GPU process and returns ownership of the
  // new pointer to the caller.
  VideoLayer* CreateVideoLayer(const gfx::Size& size);

  // Notification that the RenderWidgetHost has been asked to paint the window.
  // Depending on the backing store, the GPU backing store may have to repaint
  // at this time. On Linux this is needed because the GPU process paints
  // directly into the RWH's X window.
  void OnWindowPainted();

 private:
  RenderWidgetHost* widget_;

  GpuProcessHostUIShim* process_shim_;
  int32 routing_id_;

  DISALLOW_COPY_AND_ASSIGN(GpuViewHost);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_GPU_VIEW_HOST_H_
