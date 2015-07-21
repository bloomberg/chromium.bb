// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/native_viewport/onscreen_context_provider.h"

#include "base/bind.h"
#include "components/view_manager/gles2/command_buffer_driver.h"
#include "components/view_manager/gles2/command_buffer_impl.h"
#include "components/view_manager/gles2/gpu_state.h"

namespace native_viewport {

OnscreenContextProvider::OnscreenContextProvider(
    const scoped_refptr<gles2::GpuState>& state)
    : command_buffer_impl_(nullptr),
      state_(state),
      widget_(gfx::kNullAcceleratedWidget),
      binding_(this) {
}

OnscreenContextProvider::~OnscreenContextProvider() {
  if (command_buffer_impl_)
    command_buffer_impl_->set_observer(nullptr);
  for (const auto& driver : command_buffers_)
    driver->DestroyWindow();
}

void OnscreenContextProvider::Bind(
    mojo::InterfaceRequest<mojo::ContextProvider> request) {
  binding_.Bind(request.Pass());
}

void OnscreenContextProvider::SetAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;

  if (widget_ == gfx::kNullAcceleratedWidget) {
    if (command_buffer_impl_)
      command_buffer_impl_->DidLoseContext();
    return;
  }
  if (pending_create_callback_.is_null())
    return;

  CreateAndReturnCommandBuffer();
}

void OnscreenContextProvider::Create(
    mojo::ViewportParameterListenerPtr viewport_parameter_listener,
    const CreateCallback& callback) {
  if (!pending_create_callback_.is_null()) {
    DCHECK(!command_buffer_impl_);
    pending_create_callback_.Run(nullptr);
  }
  pending_listener_ = viewport_parameter_listener.Pass();
  pending_create_callback_ = callback;

  if (widget_ != gfx::kNullAcceleratedWidget)
    CreateAndReturnCommandBuffer();
}

void OnscreenContextProvider::OnCommandBufferImplDestroyed() {
  DCHECK(command_buffer_impl_);
  command_buffer_impl_ = nullptr;
}

void OnscreenContextProvider::CreateAndReturnCommandBuffer() {
  mojo::CommandBufferPtr cb;
  scoped_ptr<gles2::CommandBufferDriver> command_buffer_driver(
      new gles2::CommandBufferDriver(
          widget_, state_,
          base::Bind(&OnscreenContextProvider::CommandBufferDestroyed,
                     base::Unretained(this))));
  command_buffers_.insert(command_buffer_driver.get());

  command_buffer_impl_ =
      new gles2::CommandBufferImpl(GetProxy(&cb), pending_listener_.Pass(),
                                   state_, command_buffer_driver.Pass());
  command_buffer_impl_->set_observer(this);
  pending_create_callback_.Run(cb.Pass());
  pending_create_callback_.reset();
}

void OnscreenContextProvider::CommandBufferDestroyed(
    gles2::CommandBufferDriver* command_buffer) {
  command_buffers_.erase(command_buffer);
}

}  // namespace mojo
