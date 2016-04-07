// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/blink/webgraphicscontext3d_in_process_command_buffer_impl.h"

#include <GLES2/gl2.h>
#include <utility>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <stddef.h>

#include <string>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_implementation.h"

using gpu::gles2::GLES2Implementation;
using gpu::GLInProcessContext;

namespace gpu_blink {

// static
scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
WebGraphicsContext3DInProcessCommandBufferImpl::CreateOffscreenContext(
    const gpu::gles2::ContextCreationAttribHelper& attributes) {
  bool is_offscreen = true;
  return make_scoped_ptr(new WebGraphicsContext3DInProcessCommandBufferImpl(
      scoped_ptr<::gpu::GLInProcessContext>(), attributes, is_offscreen,
      gfx::kNullAcceleratedWidget));
}

scoped_ptr<WebGraphicsContext3DInProcessCommandBufferImpl>
WebGraphicsContext3DInProcessCommandBufferImpl::WrapContext(
    scoped_ptr<::gpu::GLInProcessContext> context,
    const gpu::gles2::ContextCreationAttribHelper& attributes) {
  bool is_offscreen = true;                      // Not used.
  gfx::AcceleratedWidget window = gfx::kNullAcceleratedWidget;  // Not used.
  return make_scoped_ptr(new WebGraphicsContext3DInProcessCommandBufferImpl(
      std::move(context), attributes, is_offscreen, window));
}

WebGraphicsContext3DInProcessCommandBufferImpl::
    WebGraphicsContext3DInProcessCommandBufferImpl(
        scoped_ptr<::gpu::GLInProcessContext> context,
        const gpu::gles2::ContextCreationAttribHelper& attributes,
        bool is_offscreen,
        gfx::AcceleratedWidget window)
    : attributes_(attributes),
      is_offscreen_(is_offscreen),
      window_(window),
      context_(std::move(context)) {}

WebGraphicsContext3DInProcessCommandBufferImpl::
    ~WebGraphicsContext3DInProcessCommandBufferImpl() {
}

size_t WebGraphicsContext3DInProcessCommandBufferImpl::GetMappedMemoryLimit() {
  return context_->GetMappedMemoryLimit();
}

bool WebGraphicsContext3DInProcessCommandBufferImpl::MaybeInitializeGL() {
  if (initialized_)
    return true;

  if (initialize_failed_)
    return false;

  if (!context_) {
    // TODO(kbr): More work will be needed in this implementation to
    // properly support GPU switching. Like in the out-of-process
    // command buffer implementation, all previously created contexts
    // will need to be lost either when the first context requesting the
    // discrete GPU is created, or the last one is destroyed.
    gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;
    context_.reset(GLInProcessContext::Create(
        NULL,                                          /* service */
        NULL,                                          /* surface */
        is_offscreen_, window_, gfx::Size(1, 1), NULL, /* share_context */
        attributes_, gpu_preference,
        ::gpu::GLInProcessContextSharedMemoryLimits(), nullptr, nullptr));
  }

  if (!context_) {
    initialize_failed_ = true;
    return false;
  }

  real_gl_ = context_->GetImplementation();
  SetGLInterface(real_gl_);

  real_gl_->TraceBeginCHROMIUM("WebGraphicsContext3D",
                               "InProcessContext");

  initialized_ = true;
  return true;
}

bool
WebGraphicsContext3DInProcessCommandBufferImpl::InitializeOnCurrentThread() {
  if (!MaybeInitializeGL())
    return false;
  return context_ &&
         context_->GetImplementation()->GetGraphicsResetStatusKHR() ==
             GL_NO_ERROR;
}

void WebGraphicsContext3DInProcessCommandBufferImpl::SetLock(base::Lock* lock) {
  context_->SetLock(lock);
}

::gpu::ContextSupport*
WebGraphicsContext3DInProcessCommandBufferImpl::GetContextSupport() {
  return real_gl_;
}

}  // namespace gpu_blink
