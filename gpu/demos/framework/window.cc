// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/framework/window.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/demos/framework/demo.h"
#include "gpu/demos/framework/demo_factory.h"

using gpu::Buffer;
using gpu::CommandBufferService;
using gpu::GpuScheduler;
using gpu::gles2::GLES2CmdHelper;
using gpu::gles2::GLES2Implementation;
using gpu::TransferBuffer;

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
  demo_.reset();

  // must free client before service.
  gles2_implementation_.reset();
  transfer_buffer_.reset();
  gles2_cmd_helper_.reset();

  if (decoder_.get()) {
    decoder_->Destroy(true);
  }
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

bool Window::CreateRenderContext(gfx::AcceleratedWidget hwnd) {
  {
    TransferBufferManager* manager = new TransferBufferManager();
    transfer_buffer_manager_.reset(manager);
    EXPECT_TRUE(manager->Initialize());
  }
  command_buffer_.reset(
      new CommandBufferService(transfer_buffer_manager_.get()));
  if (!command_buffer_->Initialize()) {
    return false;
  }

  scoped_refptr<gpu::gles2::ContextGroup> group(new gpu::gles2::ContextGroup(
      NULL, NULL, true));

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

  context_->MakeCurrent(surface_);

  std::vector<int32> attribs;
  if (!decoder_->Initialize(surface_.get(),
                            context_.get(),
                            surface_->IsOffscreen(),
                            gfx::Size(),
                            gpu::gles2::DisallowedFeatures(),
                            NULL,
                            attribs)) {
    return false;
  }

  command_buffer_->SetPutOffsetChangeCallback(
      base::Bind(&GpuScheduler::PutChanged,
                 base::Unretained(gpu_scheduler_.get())));
  command_buffer_->SetGetBufferChangeCallback(
      base::Bind(&GpuScheduler::SetGetBuffer,
                 base::Unretained(gpu_scheduler_.get())));

  gles2_cmd_helper_.reset(new GLES2CmdHelper(command_buffer_.get()));
  if (!gles2_cmd_helper_->Initialize(kCommandBufferSize))
    return false;

  transfer_buffer_.reset(new gpu::TransferBuffer(gles2_cmd_helper_.get()));

  ::gles2::Initialize();
  gles2_implementation_.reset(new GLES2Implementation(
      gles2_cmd_helper_.get(),
      NULL,
      transfer_buffer_.get(),
      false,
      true));

  ::gles2::SetGLContext(gles2_implementation_.get());

  if (!gles2_implementation_->Initialize(
      kTransferBufferSize,
      kTransferBufferSize,
      kTransferBufferSize)) {
    return false;
  }

  return true;
}

}  // namespace demos
}  // namespace gpu
