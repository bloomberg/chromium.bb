// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_DEMOS_FRAMEWORK_WINDOW_H_
#define GPU_DEMOS_FRAMEWORK_WINDOW_H_

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace gpu {

class TransferBuffer;

namespace gles2 {

class GLES2Implementation;

}  // namespace gles2

namespace demos {

class Demo;

// Acts as a framework for standalone demos. It creates a window and delegates
// all events to demo to perform rendering and other tasks.
class Window {
 public:
  Window();
  virtual ~Window();

  // Initializes and creates a window with given dimensions.
  bool Init(int width, int height);

  // Enters the event processing loop.
  void MainLoop();

  void OnPaint();

 private:
  // Creates and shows a native window with the given title and dimensions.
  gfx::NativeWindow CreateNativeWindow(const wchar_t* title,
                                       int width, int height);
  // Converts native window handle to NPAPI plugin window handle.
  gfx::AcceleratedWidget PluginWindow(gfx::NativeWindow hwnd);
  // Creates an OpenGL ES 2.0 rendering context for the given window.
  bool CreateRenderContext(gfx::AcceleratedWidget hwnd);

  gfx::NativeWindow window_handle_;
  scoped_ptr<Demo> demo_;

  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::GpuScheduler> gpu_scheduler_;
  scoped_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_ptr<gpu::gles2::GLES2CmdHelper> gles2_cmd_helper_;
  scoped_ptr<gpu::TransferBuffer> transfer_buffer_;
  scoped_ptr<gpu::gles2::GLES2Implementation> gles2_implementation_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace demos
}  // namespace gpu
#endif  // GPU_DEMOS_FRAMEWORK_WINDOW_H_

