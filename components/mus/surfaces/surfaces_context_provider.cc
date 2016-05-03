// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/surfaces_context_provider.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "components/mus/gles2/command_buffer_driver.h"
#include "components/mus/gles2/command_buffer_impl.h"
#include "components/mus/gles2/command_buffer_local.h"
#include "components/mus/gles2/gpu_state.h"
#include "components/mus/surfaces/surfaces_context_provider_delegate.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"

namespace mus {

SurfacesContextProvider::SurfacesContextProvider(
    gfx::AcceleratedWidget widget,
    const scoped_refptr<GpuState>& state)
    : delegate_(nullptr), widget_(widget), command_buffer_local_(nullptr) {
  command_buffer_local_ = new CommandBufferLocal(this, widget_, state);
}

void SurfacesContextProvider::SetDelegate(
    SurfacesContextProviderDelegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

// This routine needs to be safe to call more than once.
// This is called when we have an accelerated widget.
bool SurfacesContextProvider::BindToCurrentThread() {
  if (implementation_)
    return true;

  // SurfacesContextProvider should always live on the same thread as the
  // Window Manager.
  DCHECK(CalledOnValidThread());
  if (!command_buffer_local_->Initialize())
    return false;

  constexpr gpu::SharedMemoryLimits default_limits;
  gles2_helper_.reset(
      new gpu::gles2::GLES2CmdHelper(command_buffer_local_));
  if (!gles2_helper_->Initialize(default_limits.command_buffer_size))
    return false;
  gles2_helper_->SetAutomaticFlushes(false);
  transfer_buffer_.reset(new gpu::TransferBuffer(gles2_helper_.get()));
  capabilities_ = command_buffer_local_->GetCapabilities();
  bool bind_generates_resource =
      !!capabilities_.bind_generates_resource_chromium;
  // TODO(piman): Some contexts (such as compositor) want this to be true, so
  // this needs to be a public parameter.
  bool lose_context_when_out_of_memory = false;
  bool support_client_side_arrays = false;
  implementation_.reset(new gpu::gles2::GLES2Implementation(
      gles2_helper_.get(), NULL, transfer_buffer_.get(),
      bind_generates_resource, lose_context_when_out_of_memory,
      support_client_side_arrays, command_buffer_local_));
  return implementation_->Initialize(
      default_limits.start_transfer_buffer_size,
      default_limits.min_transfer_buffer_size,
      default_limits.max_transfer_buffer_size,
      default_limits.mapped_memory_reclaim_limit);
}

gpu::gles2::GLES2Interface* SurfacesContextProvider::ContextGL() {
  DCHECK(implementation_);
  return implementation_.get();
}

gpu::ContextSupport* SurfacesContextProvider::ContextSupport() {
  return implementation_.get();
}

class GrContext* SurfacesContextProvider::GrContext() {
  return NULL;
}

void SurfacesContextProvider::InvalidateGrContext(uint32_t state) {}

gpu::Capabilities SurfacesContextProvider::ContextCapabilities() {
  return capabilities_;
}

void SurfacesContextProvider::SetupLock() {}

base::Lock* SurfacesContextProvider::GetLock() {
  // This context provider is not used on multiple threads.
  NOTREACHED();
  return nullptr;
}

void SurfacesContextProvider::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  implementation_->SetLostContextCallback(lost_context_callback);
}

SurfacesContextProvider::~SurfacesContextProvider() {
  implementation_->Flush();
  implementation_.reset();
  transfer_buffer_.reset();
  gles2_helper_.reset();
  command_buffer_local_->Destroy();
  command_buffer_local_ = nullptr;
}

void SurfacesContextProvider::UpdateVSyncParameters(int64_t timebase,
                                                    int64_t interval) {
  if (delegate_)
    delegate_->OnVSyncParametersUpdated(timebase, interval);
}

void SurfacesContextProvider::GpuCompletedSwapBuffers(gfx::SwapResult result) {
  if (!swap_buffers_completion_callback_.is_null()) {
    swap_buffers_completion_callback_.Run(result);
  }
}

void SurfacesContextProvider::SetSwapBuffersCompletionCallback(
    gfx::GLSurface::SwapCompletionCallback callback) {
  swap_buffers_completion_callback_ = callback;
}

}  // namespace mus
