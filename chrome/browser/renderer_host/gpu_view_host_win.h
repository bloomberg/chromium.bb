// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_GPU_VIEW_HOST_WIN_H_
#define CHROME_BROWSER_RENDERER_HOST_GPU_VIEW_HOST_WIN_H_

#include <windows.h>

#include "base/basictypes.h"

class BackingStore;
class GpuProcessHost;
class RenderWidgetHost;
class RenderWidgetHostViewWin;

namespace gfx {
class Size;
}

// A proxy for the GPU process' window for rendering pages.
class GpuViewHostWin {
 public:
  GpuViewHostWin(RenderWidgetHostViewWin* view, HWND parent);
  ~GpuViewHostWin();

  BackingStore* CreateBackingStore(RenderWidgetHost* widget,
                                   const gfx::Size& size);

 private:
  RenderWidgetHostViewWin* view_;

  GpuProcessHost* process_;
  int32 routing_id_;

  DISALLOW_COPY_AND_ASSIGN(GpuViewHostWin);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_GPU_VIEW_HOST_WIN_H_
