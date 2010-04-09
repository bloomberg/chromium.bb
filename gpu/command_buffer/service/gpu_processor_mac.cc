// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#if !defined(UNIT_TEST)
    AcceleratedSurface* surface_ptr = &surface_;
#else
    AcceleratedSurface* surface_ptr = NULL;
#endif
    scoped_ptr<ViewGLContext> context(new ViewGLContext(surface_ptr));
    // TODO(apatrick): support multisampling.
    if (!context->Initialize(false)) {
      Destroy();
      return false;
    }
    context_.reset(context.release());
  } else {
    scoped_ptr<PbufferGLContext> context(new PbufferGLContext);
    if (!context->Initialize(parent_context)) {
      Destroy();
      return false;
    }
    context_.reset(context.release());
  }

  return InitializeCommon(size, parent_decoder, parent_texture_id);

  return true;
}

uint64 GPUProcessor::SetWindowSizeForIOSurface(const gfx::Size& size) {
#if !defined(UNIT_TEST)
  return surface_.SetSurfaceSize(size);
#else
  return 0;
#endif
}

TransportDIB::Handle GPUProcessor::SetWindowSizeForTransportDIB(
    const gfx::Size& size) {
#if !defined(UNIT_TEST)
  return surface_.SetTransportDIBSize(size);
#else
  return TransportDIB::DefaultHandleValue();
#endif
}

void GPUProcessor::SetTransportDIBAllocAndFree(
      Callback2<size_t, TransportDIB::Handle*>::Type* allocator,
      Callback1<TransportDIB::Id>::Type* deallocator) {
#if !defined(UNIT_TEST)
  surface_.SetTransportDIBAllocAndFree(allocator, deallocator);
#endif
}

}  // namespace gpu

