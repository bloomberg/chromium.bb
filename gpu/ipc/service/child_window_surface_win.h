// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_CHILD_WINDOW_SURFACE_WIN_H_
#define GPU_IPC_SERVICE_CHILD_WINDOW_SURFACE_WIN_H_

#include "base/memory/weak_ptr.h"
#include "gpu/ipc/service/child_window_win.h"
#include "gpu/ipc/service/image_transport_surface_delegate.h"
#include "ui/gl/gl_surface_egl.h"

#include <windows.h>

namespace gpu {

class ChildWindowSurfaceWin : public gl::NativeViewGLSurfaceEGL {
 public:
  ChildWindowSurfaceWin(base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
                        HWND parent_window);

  // GLSurface implementation.
  EGLConfig GetConfig() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  bool InitializeNativeWindow() override;
  gfx::SwapResult SwapBuffers() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;

 protected:
  ~ChildWindowSurfaceWin() override;

 private:
  ChildWindowWin child_window_;
  bool alpha_;
  bool first_swap_;

  DISALLOW_COPY_AND_ASSIGN(ChildWindowSurfaceWin);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_CHILD_WINDOW_SURFACE_WIN_H_
