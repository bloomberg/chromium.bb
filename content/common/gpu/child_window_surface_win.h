// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CHILD_WINDOW_SURFACE_WIN_H_
#define CONTENT_COMMON_GPU_CHILD_WINDOW_SURFACE_WIN_H_
#include "ui/gl/gl_surface_egl.h"

#include <windows.h>

namespace content {

class GpuChannelManager;

class ChildWindowSurfaceWin : public gfx::NativeViewGLSurfaceEGL {
 public:
  ChildWindowSurfaceWin(GpuChannelManager* manager, HWND parent_window);

  // GLSurface implementation.
  EGLConfig GetConfig() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              bool has_alpha) override;
  bool InitializeNativeWindow() override;
  gfx::SwapResult SwapBuffers() override;
  gfx::SwapResult PostSubBuffer(int x, int y, int width, int height) override;

  void InvalidateWindowRect(const gfx::Rect& rect);

 protected:
  ~ChildWindowSurfaceWin() override;

 private:
  void ClearInvalidContents();

  HWND parent_window_;
  GpuChannelManager* manager_;
  gfx::Rect rect_to_clear_;

  DISALLOW_COPY_AND_ASSIGN(ChildWindowSurfaceWin);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CHILD_WINDOW_SURFACE_WIN_H_
