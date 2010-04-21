// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "gfx/gl/gl_context.h"
#include "gpu/command_buffer/service/gpu_processor.h"

using ::base::SharedMemory;

namespace gpu {

bool GPUProcessor::Initialize(gfx::PluginWindowHandle window,
                              const gfx::Size& size,
                              GPUProcessor* parent,
                              uint32 parent_texture_id) {
  // Cannot reinitialize.
  if (context_.get())
    return false;

  // Get the parent decoder and the GLContext to share IDs with, if any.
  gles2::GLES2Decoder* parent_decoder = NULL;
  gfx::GLContext* parent_context = NULL;
  void* parent_handle = NULL;
  if (parent) {
    parent_decoder = parent->decoder_.get();
    DCHECK(parent_decoder);

    parent_context = parent_decoder->GetGLContext();
    DCHECK(parent_context);

    parent_handle = parent_context->GetHandle();
    DCHECK(parent_handle);
  }

  // Create either a view or pbuffer based GLContext.
  if (window) {
    DCHECK(!parent_handle);

    // TODO(apatrick): support multisampling.
    context_.reset(gfx::GLContext::CreateViewGLContext(window, false));
  } else {
    context_.reset(gfx::GLContext::CreateOffscreenGLContext(parent_handle));
  }

  if (!context_.get())
    return false;

  return InitializeCommon(size, parent_decoder, parent_texture_id);
}

void GPUProcessor::Destroy() {
  DestroyCommon();
}

void GPUProcessor::WillSwapBuffers() {
  if (wrapped_swap_buffers_callback_.get()) {
    wrapped_swap_buffers_callback_->Run();
  }
}

}  // namespace gpu
