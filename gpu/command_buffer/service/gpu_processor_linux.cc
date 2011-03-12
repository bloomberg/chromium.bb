// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/gfx/gl/gl_context.h"
#include "gpu/command_buffer/service/gpu_processor.h"

using ::base::SharedMemory;

namespace gpu {

bool GPUProcessor::Initialize(gfx::PluginWindowHandle window,
                              const gfx::Size& size,
                              const char* allowed_extensions,
                              const std::vector<int32>& attribs,
                              GPUProcessor* parent,
                              uint32 parent_texture_id) {
  // Get the parent decoder and the GLContext to share IDs with, if any.
  gles2::GLES2Decoder* parent_decoder = NULL;
  gfx::GLContext* parent_context = NULL;
  void* parent_handle = NULL;
  if (parent) {
    parent_decoder = parent->decoder_.get();
    DCHECK(parent_decoder);

    parent_context = parent_decoder->GetGLContext();
    DCHECK(parent_context);
  }

  // Create either a view or pbuffer based GLContext.
  scoped_ptr<gfx::GLContext> context;
  if (window) {
    DCHECK(!parent_handle);

    // TODO(apatrick): support multisampling.
    context.reset(gfx::GLContext::CreateViewGLContext(window, false));
  } else {
    context.reset(gfx::GLContext::CreateOffscreenGLContext(parent_context));
  }

  if (!context.get()) {
    LOG(ERROR) << "GPUProcessor::Initialize failed";
    return false;
  }

  return InitializeCommon(context.release(),
                          size,
                          allowed_extensions,
                          attribs,
                          parent_decoder,
                          parent_texture_id);
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
