// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/onscreen_context_provider.h"

#include "mojo/services/gles2/command_buffer_driver.h"
#include "mojo/services/gles2/command_buffer_impl.h"
#include "mojo/services/gles2/gpu_state.h"

namespace native_viewport {

OnscreenContextProvider::OnscreenContextProvider(
    const scoped_refptr<gles2::GpuState>& state)
    : state_(state), widget_(gfx::kNullAcceleratedWidget), binding_(this) {
}

OnscreenContextProvider::~OnscreenContextProvider() {
}

void OnscreenContextProvider::Bind(
    mojo::InterfaceRequest<mojo::ContextProvider> request) {
  binding_.Bind(request.Pass());
}

void OnscreenContextProvider::SetAcceleratedWidget(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;
  if (widget_ != gfx::kNullAcceleratedWidget &&
      !pending_create_callback_.is_null())
    CreateAndReturnCommandBuffer();
}

void OnscreenContextProvider::Create(
    mojo::ViewportParameterListenerPtr viewport_parameter_listener,
    const CreateCallback& callback) {
  if (!pending_create_callback_.is_null())
    pending_create_callback_.Run(nullptr);
  pending_listener_ = viewport_parameter_listener.Pass();
  pending_create_callback_ = callback;

  if (widget_ != gfx::kNullAcceleratedWidget)
    CreateAndReturnCommandBuffer();
}

void OnscreenContextProvider::CreateAndReturnCommandBuffer() {
  mojo::CommandBufferPtr cb;
  new gles2::CommandBufferImpl(
      GetProxy(&cb), pending_listener_.Pass(), state_->control_task_runner(),
      state_->sync_point_manager(),
      make_scoped_ptr(new gles2::CommandBufferDriver(
          widget_, state_->share_group(), state_->mailbox_manager(),
          state_->sync_point_manager())));
  pending_create_callback_.Run(cb.Pass());
  pending_create_callback_.reset();
}

}  // namespace mojo
