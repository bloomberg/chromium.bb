// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/gles2/gles2_context.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "mojo/public/c/gles2/gles2.h"
#include "mojo/public/cpp/system/core.h"

namespace gles2 {

GLES2Context::GLES2Context(const std::vector<int32_t>& attribs,
                           mojo::ScopedMessagePipeHandle command_buffer_handle,
                           MojoGLES2ContextLost lost_callback,
                           void* closure)
    : command_buffer_(attribs, std::move(command_buffer_handle)),
      lost_callback_(lost_callback),
      closure_(closure) {}

GLES2Context::~GLES2Context() {}

bool GLES2Context::Initialize() {
  if (!command_buffer_.Initialize())
    return false;

  constexpr gpu::SharedMemoryLimits default_limits;
  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(&command_buffer_));
  if (!gles2_helper_->Initialize(default_limits.command_buffer_size))
    return false;
  gles2_helper_->SetAutomaticFlushes(false);
  transfer_buffer_.reset(new gpu::TransferBuffer(gles2_helper_.get()));
  gpu::Capabilities capabilities = command_buffer_.GetCapabilities();
  bool bind_generates_resource =
      !!capabilities.bind_generates_resource_chromium;
  // TODO(piman): Some contexts (such as compositor) want this to be true, so
  // this needs to be a public parameter.
  bool lose_context_when_out_of_memory = false;
  bool support_client_side_arrays = false;
  implementation_.reset(
      new gpu::gles2::GLES2Implementation(gles2_helper_.get(),
                                          NULL,
                                          transfer_buffer_.get(),
                                          bind_generates_resource,
                                          lose_context_when_out_of_memory,
                                          support_client_side_arrays,
                                          &command_buffer_));
  if (!implementation_->Initialize(default_limits.start_transfer_buffer_size,
                                   default_limits.min_transfer_buffer_size,
                                   default_limits.max_transfer_buffer_size,
                                   default_limits.mapped_memory_reclaim_limit))
    return false;
  implementation_->SetLostContextCallback(
      base::Bind(&GLES2Context::OnLostContext, base::Unretained(this)));
  return true;
}

void GLES2Context::OnLostContext() {
  lost_callback_(closure_);
}

}  // namespace gles2
