// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/framework/window.h"

#include "base/callback.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gpu_processor.h"
#include "gpu/demos/framework/demo.h"
#include "gpu/demos/framework/demo_factory.h"

using gpu::Buffer;
using gpu::CommandBufferService;
using gpu::GPUProcessor;
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

  demo_->InitWindowSize(width, height);
  return demo_->InitGL();
}

void Window::OnPaint() {
  demo_->Draw();
  ::gles2::GetGLContext()->SwapBuffers();
}

// TODO(apatrick): It looks like all the resources allocated here leak. We
// should fix that if we want to use this Window class for anything beyond this
// simple use case.
bool Window::CreateRenderContext(gfx::PluginWindowHandle hwnd) {
  scoped_ptr<CommandBufferService> command_buffer(new CommandBufferService);
  if (!command_buffer->Initialize(kCommandBufferSize)) {
    return false;
  }

  GPUProcessor* gpu_processor(
      new GPUProcessor(command_buffer.get(), NULL));
  if (!gpu_processor->Initialize(hwnd, gfx::Size(), NULL, std::vector<int32>(),
                                 NULL, 0)) {
    return false;
  }

  command_buffer->SetPutOffsetChangeCallback(
      NewCallback(gpu_processor, &GPUProcessor::ProcessCommands));

  GLES2CmdHelper* helper = new GLES2CmdHelper(command_buffer.get());
  if (!helper->Initialize(kCommandBufferSize)) {
    // TODO(alokp): cleanup.
    return false;
  }

  int32 transfer_buffer_id =
      command_buffer->CreateTransferBuffer(kTransferBufferSize);
  Buffer transfer_buffer =
      command_buffer->GetTransferBuffer(transfer_buffer_id);
  if (transfer_buffer.ptr == NULL) return false;

  ::gles2::Initialize();
  ::gles2::SetGLContext(new GLES2Implementation(helper,
                                                transfer_buffer.size,
                                                transfer_buffer.ptr,
                                                transfer_buffer_id,
                                                false));
  return command_buffer.release() != NULL;
}

}  // namespace demos
}  // namespace gpu
