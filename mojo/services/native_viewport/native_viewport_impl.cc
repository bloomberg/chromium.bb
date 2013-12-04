// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_impl.h"

#include "base/message_loop/message_loop.h"
#include "mojo/services/gles2/gles2_impl.h"
#include "mojo/services/native_viewport/native_viewport.h"
#include "ui/events/event.h"

namespace mojo {
namespace services {

NativeViewportImpl::NativeViewportImpl(shell::Context* context,
                                       ScopedMessagePipeHandle pipe)
    : context_(context),
      widget_(gfx::kNullAcceleratedWidget),
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
  gles2_.reset();
  DCHECK(native_viewport_);
  native_viewport_->Close();
}

void NativeViewportImpl::CreateGLES2Context(
    ScopedMessagePipeHandle gles2_client) {
  gles2_.reset(new GLES2Impl(gles2_client.Pass()));
  CreateGLES2ContextIfNeeded();
}

void NativeViewportImpl::CreateGLES2ContextIfNeeded() {
  if (widget_ == gfx::kNullAcceleratedWidget || !gles2_)
    return;
  gles2_->CreateContext(widget_, native_viewport_->GetSize());
}

bool NativeViewportImpl::OnEvent(ui::Event* event) {
  return false;
}

void NativeViewportImpl::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;
  CreateGLES2ContextIfNeeded();
}

void NativeViewportImpl::OnResized(const gfx::Size& size) {
}

void NativeViewportImpl::OnDestroyed() {
  base::MessageLoop::current()->Quit();
}

}  // namespace services
}  // namespace mojo
