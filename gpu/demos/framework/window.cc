// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/framework/window.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/demos/framework/demo.h"
#include "gpu/demos/framework/demo_factory.h"

using gpu::Buffer;
using gpu::CommandBufferService;
using gpu::GpuScheduler;
using gpu::gles2::GLES2CmdHelper;
using gpu::gles2::GLES2Implementation;

namespace {
const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 512 * 1024;
}  // namespace.

namespace gpu {
namespace demos {

Window::Window()
  : window_handle_(NULL),
    demo_(CreateDemo()) {
}

Window::~Window() {
}

bool Window::Init(int width, int height) {
  window_handle_ = CreateNativeWindow(demo_->Title(), width, height);
  if (window_handle_ == NULL)
    return false;
  if (!CreateRenderContext(PluginWindow(window_handle_)))
    return false;

  demo_->Resize(width, height);
  return demo_->InitGL();
}

void Window::OnPaint() {
  demo_->Draw();
  ::gles2::GetGLContext()->SwapBuffers();
}

bool Window::CreateRenderContext(gfx::PluginWindowHandle hwnd) {
  command_buffer_.reset(new CommandBufferService);
  if (!command_buffer_->Initialize(kCommandBufferSize)) {
    return false;
  }

  gpu::gles2::ContextGroup::Ref group(new gpu::gles2::ContextGroup(true));

  decoder_.reset(gpu::gles2::GLES2Decoder::Create(group.get()));
  if (!decoder_.get())
    return false;

  gpu_scheduler_.reset(new gpu::GpuScheduler(command_buffer_.get(),
                                             decoder_.get(),
                                             NULL));

  decoder_->set_engine(gpu_scheduler_.get());

  surface_ = gfx::GLSurface::CreateViewGLSurface(false, hwnd);
  if (!surface_.get())
    return false;

  context_ = gfx::GLContext::CreateGLContext(
      NULL, surface_.get(), gfx::PreferDiscreteGpu);
  if (!context_.get())
    return false;

  std::vector<int32> attribs;
  if (!decoder_->Initialize(surface_.get(),
                            context_.get(),
                            gfx::Size(),
                            gpu::gles2::DisallowedFeatures(),
                            NULL,
                            attribs)) {
    return false;
  }

  command_buffer_->SetPutOffsetChangeCallback(
      base::Bind(&GpuScheduler::PutChanged,
                 base::Unretained(gpu_scheduler_.get())));

  gles2_cmd_helper_.reset(new GLES2CmdHelper(command_buffer_.get()));
  if (!gles2_cmd_helper_->Initialize(kCommandBufferSize))
    return false;

  int32 transfer_buffer_id =
      command_buffer_->CreateTransferBuffer(kTransferBufferSize, -1);
  Buffer transfer_buffer =
      command_buffer_->GetTransferBuffer(transfer_buffer_id);
  if (transfer_buffer.ptr == NULL) return false;

  ::gles2::Initialize();
  ::gles2::SetGLContext(new GLES2Implementation(gles2_cmd_helper_.get(),
                                                transfer_buffer.size,
                                                transfer_buffer.ptr,
                                                transfer_buffer_id,
                                                false,
                                                true));
  return true;
}

}  // namespace demos
}  // namespace gpu
