// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/gles2/gles2_context.h"

#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "mojo/public/gles2/gles2.h"
#include "mojo/public/system/core_cpp.h"

namespace mojo {
namespace gles2 {

namespace {
const size_t kDefaultCommandBufferSize = 1024 * 1024;
const size_t kDefaultStartTransferBufferSize = 1 * 1024 * 1024;
const size_t kDefaultMinTransferBufferSize = 1 * 256 * 1024;
const size_t kDefaultMaxTransferBufferSize = 16 * 1024 * 1024;
}

GLES2Context::GLES2Context(MojoAsyncWaiter* async_waiter,
                           ScopedCommandBufferHandle command_buffer_handle,
                           MojoGLES2ContextLost lost_callback,
                           MojoGLES2DrawAnimationFrame animation_callback,
                           void* closure)
    : command_buffer_(this, async_waiter, command_buffer_handle.Pass()),
      lost_callback_(lost_callback),
      animation_callback_(animation_callback),
      closure_(closure) {}

GLES2Context::~GLES2Context() {}

bool GLES2Context::Initialize() {
  if (!command_buffer_.Initialize())
    return false;
  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(&command_buffer_));
  if (!gles2_helper_->Initialize(kDefaultCommandBufferSize))
    return false;
  gles2_helper_->SetAutomaticFlushes(false);
  transfer_buffer_.reset(new gpu::TransferBuffer(gles2_helper_.get()));
  implementation_.reset(
      new gpu::gles2::GLES2Implementation(gles2_helper_.get(),
                                          NULL,
                                          transfer_buffer_.get(),
                                          true,
                                          &command_buffer_));
  return implementation_->Initialize(kDefaultStartTransferBufferSize,
                                     kDefaultMinTransferBufferSize,
                                     kDefaultMaxTransferBufferSize,
                                     gpu::gles2::GLES2Implementation::kNoLimit);
}

void GLES2Context::RequestAnimationFrames() {
  command_buffer_.RequestAnimationFrames();
}

void GLES2Context::CancelAnimationFrames() {
  command_buffer_.CancelAnimationFrames();
}

void GLES2Context::ContextLost() { lost_callback_(closure_); }

void GLES2Context::DrawAnimationFrame() { animation_callback_(closure_); }

}  // namespace gles2
}  // namespace mojo
