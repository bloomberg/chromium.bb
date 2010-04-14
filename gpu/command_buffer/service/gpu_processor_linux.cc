// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gl_context.h"
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
  GLContext* parent_context = NULL;
  if (parent) {
    parent_decoder = parent->decoder_.get();
    DCHECK(parent_decoder);

    parent_context = parent_decoder->GetGLContext();
    DCHECK(parent_context);
  }

  // Create either a view or pbuffer based GLContext.
  if (window) {
    scoped_ptr<ViewGLContext> context(new ViewGLContext(window));
    // TODO(apatrick): support multisampling.
    if (!context->Initialize(false)) {
      Destroy();
      return false;
    }
    context_.reset(context.release());
  } else {
    scoped_ptr<PbufferGLContext> context(new PbufferGLContext());
    if (!context->Initialize(parent_context)) {
      Destroy();
      return false;
    }
    context_.reset(context.release());
  }

  return InitializeCommon(size, parent_decoder, parent_texture_id);

  return true;
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
