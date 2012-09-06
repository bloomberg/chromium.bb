// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_GLES2_CONFORM_SUPPORT_EGL_DISPLAY_H_
#define GPU_GLES2_CONFORM_SUPPORT_EGL_DISPLAY_H_

#include <EGL/egl.h>

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace gpu {
class CommandBufferService;
class GpuScheduler;
class TransferBuffer;
class TransferBufferManagerInterface;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace egl {

class Config;
class Surface;

class Display {
 public:
  explicit Display(EGLNativeDisplayType display_id);
  virtual ~Display();

  void SetCreateOffscreen(int width, int height) {
    create_offscreen_ = true;
    create_offscreen_width_ = width;
    create_offscreen_height_ = height;
  }

  bool is_initialized() const { return is_initialized_; }
  bool Initialize();

  // Config routines.
  bool IsValidConfig(EGLConfig config);
  bool ChooseConfigs(
      EGLConfig* configs, EGLint config_size, EGLint* num_config);
  bool GetConfigs(EGLConfig* configs, EGLint config_size, EGLint* num_config);
  bool GetConfigAttrib(EGLConfig config, EGLint attribute, EGLint* value);

  // Surface routines.
  bool IsValidNativeWindow(EGLNativeWindowType win);
  bool IsValidSurface(EGLSurface surface);
  EGLSurface CreateWindowSurface(EGLConfig config,
                                 EGLNativeWindowType win,
                                 const EGLint* attrib_list);
  void DestroySurface(EGLSurface surface);
  void SwapBuffers(EGLSurface surface);

  // Context routines.
  bool IsValidContext(EGLContext ctx);
  EGLContext CreateContext(EGLConfig config,
                           EGLContext share_ctx,
                           const EGLint* attrib_list);
  void DestroyContext(EGLContext ctx);
  bool MakeCurrent(EGLSurface draw, EGLSurface read, EGLContext ctx);

 private:
  EGLNativeDisplayType display_id_;

  bool is_initialized_;
  bool create_offscreen_;
  int create_offscreen_width_;
  int create_offscreen_height_;

  scoped_ptr<gpu::TransferBufferManagerInterface> transfer_buffer_manager_;
  scoped_ptr<gpu::CommandBufferService> command_buffer_;
  scoped_ptr<gpu::GpuScheduler> gpu_scheduler_;
  scoped_ptr<gpu::gles2::GLES2Decoder> decoder_;
  scoped_refptr<gfx::GLContext> gl_context_;
  scoped_refptr<gfx::GLSurface> gl_surface_;
  scoped_ptr<gpu::gles2::GLES2CmdHelper> gles2_cmd_helper_;
  scoped_ptr<gpu::TransferBuffer> transfer_buffer_;

  // TODO(alokp): Support more than one config, surface, and context.
  scoped_ptr<Config> config_;
  scoped_ptr<Surface> surface_;
  scoped_ptr<gpu::gles2::GLES2Implementation> context_;

  DISALLOW_COPY_AND_ASSIGN(Display);
};

}  // namespace egl

#endif  // GPU_GLES2_CONFORM_SUPPORT_EGL_DISPLAY_H_
