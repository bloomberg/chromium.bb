// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_impl.h"

#include <limits>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "mojo/services/native_viewport/native_viewport.h"
#include "ui/events/event.h"

namespace mojo {
namespace services {

NativeViewportImpl::NativeViewportImpl(shell::Context* context,
                                       ScopedMessagePipeHandle pipe)
    : context_(context),
      client_(pipe.Pass()) {
  client_.SetPeer(this);
}

NativeViewportImpl::~NativeViewportImpl() {
}

void NativeViewportImpl::Open() {
  native_viewport_ = services::NativeViewport::Create(context_, this);
  native_viewport_->Init();
  client_->DidOpen();
}

void NativeViewportImpl::Close() {
  DCHECK(native_viewport_);
  native_viewport_->Close();
}

bool NativeViewportImpl::OnEvent(ui::Event* event) {
  return false;
}

void NativeViewportImpl::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  gfx::Size size = native_viewport_->GetSize();
  gpu::GLInProcessContextAttribs attribs;
  gl_context_.reset(gpu::GLInProcessContext::CreateContext(
      false, widget, size, false, attribs, gfx::PreferDiscreteGpu));
  gl_context_->SetContextLostCallback(base::Bind(
      &NativeViewportImpl::OnGLContextLost, base::Unretained(this)));

  gpu::gles2::GLES2Interface* gl = gl_context_->GetImplementation();
  // TODO(abarth): Instead of drawing green, we want to send the context over
  // pipe_ somehow.
  uint64_t encoded_gl = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(gl));
  client_->DidCreateGLContext(encoded_gl);
}

void NativeViewportImpl::OnGLContextLost() {
}

void NativeViewportImpl::OnResized(const gfx::Size& size) {
}

void NativeViewportImpl::OnDestroyed() {
  base::MessageLoop::current()->Quit();
}

}  // namespace services
}  // namespace mojo
