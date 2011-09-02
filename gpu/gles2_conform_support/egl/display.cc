// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gles2_conform_support/egl/display.h"

#include <vector>
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/gles2_conform_support/egl/config.h"
#include "gpu/gles2_conform_support/egl/surface.h"

namespace {
const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 512 * 1024;
}

namespace egl {

Display::Display(EGLNativeDisplayType display_id)
    : display_id_(display_id),
      is_initialized_(false),
      transfer_buffer_id_(-1) {
}

Display::~Display() {
  gles2::Terminate();
}

bool Display::Initialize() {
  using gpu::CommandBufferService;
  scoped_ptr<CommandBufferService> command_buffer(new CommandBufferService);
  if (!command_buffer->Initialize(kCommandBufferSize))
    return false;

  using gpu::Buffer;
  int32 transfer_buffer_id =
      command_buffer->CreateTransferBuffer(kTransferBufferSize, -1);
  Buffer transfer_buffer =
      command_buffer->GetTransferBuffer(transfer_buffer_id);
  if (transfer_buffer.ptr == NULL)
    return false;

  using gpu::gles2::GLES2CmdHelper;
  scoped_ptr<GLES2CmdHelper> cmd_helper(
      new GLES2CmdHelper(command_buffer.get()));
  if (!cmd_helper->Initialize(kCommandBufferSize))
    return false;

  gles2::Initialize();

  is_initialized_ = true;
  command_buffer_.reset(command_buffer.release());
  transfer_buffer_id_ = transfer_buffer_id;
  gles2_cmd_helper_.reset(cmd_helper.release());
  return true;
}

bool Display::IsValidConfig(EGLConfig config) {
  return (config != NULL) && (config == config_.get());
}

bool Display::GetConfigs(EGLConfig* configs,
                         EGLint config_size,
                         EGLint* num_config) {
  // TODO(alokp): Find out a way to find all configs. CommandBuffer currently
  // does not support finding or choosing configs.
  *num_config = 1;
  if (configs != NULL) {
    if (config_ == NULL) {
      config_.reset(new Config);
    }
    configs[0] = config_.get();
  }
  return true;
}

bool Display::GetConfigAttrib(EGLConfig config,
                              EGLint attribute,
                              EGLint* value) {
  const egl::Config* cfg = static_cast<egl::Config*>(config);
  return cfg->GetAttrib(attribute, value);
}

bool Display::IsValidNativeWindow(EGLNativeWindowType win) {
#if defined OS_WIN
  return ::IsWindow(win) != FALSE;
#else
  // TODO(alokp): Validate window handle.
  return true;
#endif  // OS_WIN
}

bool Display::IsValidSurface(EGLSurface surface) {
  return (surface != NULL) && (surface == surface_.get());
}

EGLSurface Display::CreateWindowSurface(EGLConfig config,
                                        EGLNativeWindowType win,
                                        const EGLint* attrib_list) {
  if (surface_ != NULL) {
    // We do not support more than one window surface.
    return EGL_NO_SURFACE;
  }

  using gpu::GpuScheduler;
  std::vector<int32> attribs;
  gpu::gles2::ContextGroup::Ref group(new gpu::gles2::ContextGroup(true));
  scoped_ptr<GpuScheduler> gpu_scheduler(
      GpuScheduler::Create(command_buffer_.get(), group.get()));
  if (!gpu_scheduler->Initialize(
      win, gfx::Size(), false, gpu::gles2::DisallowedExtensions(), NULL,
      attribs, NULL))
    return EGL_NO_SURFACE;

  command_buffer_->SetPutOffsetChangeCallback(
      NewCallback(gpu_scheduler.get(), &GpuScheduler::PutChanged));
  gpu_scheduler_.reset(gpu_scheduler.release());
  surface_.reset(new Surface(win));

  return surface_.get();
}

void Display::DestroySurface(EGLSurface surface) {
  DCHECK(IsValidSurface(surface));
  gpu_scheduler_.reset();
  surface_.reset();
}

void Display::SwapBuffers(EGLSurface surface) {
  DCHECK(IsValidSurface(surface));
  context_->SwapBuffers();
}

bool Display::IsValidContext(EGLContext ctx) {
  return (ctx != NULL) && (ctx == context_.get());
}

EGLContext Display::CreateContext(EGLConfig config,
                                  EGLContext share_ctx,
                                  const EGLint* attrib_list) {
  DCHECK(IsValidConfig(config));
  // TODO(alokp): Command buffer does not support shared contexts.
  if (share_ctx != NULL)
    return EGL_NO_CONTEXT;

  DCHECK(command_buffer_ != NULL);
  DCHECK(transfer_buffer_id_ != -1);
  gpu::Buffer buffer = command_buffer_->GetTransferBuffer(transfer_buffer_id_);
  DCHECK(buffer.ptr != NULL);
  bool share_resources = share_ctx != NULL;
  using gpu::gles2::GLES2Implementation;
  context_.reset(new GLES2Implementation(
      gles2_cmd_helper_.get(),
      buffer.size,
      buffer.ptr,
      transfer_buffer_id_,
      share_resources,
      true));

  context_->EnableFeatureCHROMIUM(
      "pepper3d_allow_buffers_on_multiple_targets");
  context_->EnableFeatureCHROMIUM(
      "pepper3d_support_fixed_attribs");

  return context_.get();
}

void Display::DestroyContext(EGLContext ctx) {
  DCHECK(IsValidContext(ctx));
  context_.reset();
}

bool Display::MakeCurrent(EGLSurface draw, EGLSurface read, EGLContext ctx) {
  if (ctx == EGL_NO_CONTEXT) {
    gles2::SetGLContext(NULL);
  } else {
    DCHECK(IsValidSurface(draw));
    DCHECK(IsValidSurface(read));
    DCHECK(IsValidContext(ctx));
    gles2::SetGLContext(context_.get());
  }
  return true;
}

}  // namespace egl
