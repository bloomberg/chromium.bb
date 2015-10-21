// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/surfaces_context_provider.h"

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
#include "gpu/command_buffer/client/transfer_buffer.h"

namespace mus {

namespace {
const size_t kDefaultCommandBufferSize = 1024 * 1024;
const size_t kDefaultStartTransferBufferSize = 1 * 1024 * 1024;
const size_t kDefaultMinTransferBufferSize = 1 * 256 * 1024;
const size_t kDefaultMaxTransferBufferSize = 16 * 1024 * 1024;
}

SurfacesContextProvider::SurfacesContextProvider(
    SurfacesContextProviderDelegate* delegate,
    gfx::AcceleratedWidget widget,
    const scoped_refptr<GpuState>& state)
    : delegate_(delegate), widget_(widget) {
  capabilities_.gpu.image = true;
  command_buffer_local_.reset(new CommandBufferLocal(this, widget_, state));
}

// This is called when we have an accelerated widget.
bool SurfacesContextProvider::BindToCurrentThread() {
  // SurfacesContextProvider should always live on the same thread as the
  // Window Manager.
  DCHECK(CalledOnValidThread());
  if (!command_buffer_local_->Initialize())
    return false;
  gles2_helper_.reset(new gpu::gles2::GLES2CmdHelper(
      command_buffer_local_->GetCommandBuffer()));
  if (!gles2_helper_->Initialize(kDefaultCommandBufferSize))
    return false;
  gles2_helper_->SetAutomaticFlushes(false);
  transfer_buffer_.reset(new gpu::TransferBuffer(gles2_helper_.get()));
  gpu::Capabilities capabilities = command_buffer_local_->GetCapabilities();
  bool bind_generates_resource =
      !!capabilities.bind_generates_resource_chromium;
  // TODO(piman): Some contexts (such as compositor) want this to be true, so
  // this needs to be a public parameter.
  bool lose_context_when_out_of_memory = false;
  bool support_client_side_arrays = false;
  implementation_.reset(new gpu::gles2::GLES2Implementation(
      gles2_helper_.get(), NULL, transfer_buffer_.get(),
      bind_generates_resource, lose_context_when_out_of_memory,
      support_client_side_arrays, command_buffer_local_.get()));
  return implementation_->Initialize(
      kDefaultStartTransferBufferSize, kDefaultMinTransferBufferSize,
      kDefaultMaxTransferBufferSize, gpu::gles2::GLES2Implementation::kNoLimit);
}

gpu::gles2::GLES2Interface* SurfacesContextProvider::ContextGL() {
  return implementation_.get();
}

gpu::ContextSupport* SurfacesContextProvider::ContextSupport() {
  return implementation_.get();
}

class GrContext* SurfacesContextProvider::GrContext() {
  return NULL;
}

void SurfacesContextProvider::InvalidateGrContext(uint32_t state) {}

cc::ContextProvider::Capabilities
SurfacesContextProvider::ContextCapabilities() {
  return capabilities_;
}

void SurfacesContextProvider::SetupLock() {}

base::Lock* SurfacesContextProvider::GetLock() {
  return &context_lock_;
}

void SurfacesContextProvider::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {
  lost_context_callback_ = lost_context_callback;
}

SurfacesContextProvider::~SurfacesContextProvider() {
  implementation_->Flush();
  implementation_.reset();
  transfer_buffer_.reset();
  gles2_helper_.reset();
  command_buffer_local_.reset();
}

void SurfacesContextProvider::UpdateVSyncParameters(int64_t timebase,
                                                    int64_t interval) {
  if (delegate_)
    delegate_->OnVSyncParametersUpdated(timebase, interval);
}

void SurfacesContextProvider::DidLoseContext() {
  lost_context_callback_.Run();
}

}  // namespace mus
