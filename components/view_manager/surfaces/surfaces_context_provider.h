// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_SURFACES_SURFACES_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIEW_MANAGER_SURFACES_SURFACES_CONTEXT_PROVIDER_H_

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/context_provider.h"
#include "components/view_manager/gles2/command_buffer_local.h"
#include "components/view_manager/gles2/command_buffer_local_client.h"
#include "components/view_manager/gles2/gpu_state.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "third_party/mojo/src/mojo/public/c/gles2/gles2.h"
#include "third_party/mojo/src/mojo/public/cpp/system/core.h"
#include "ui/gfx/native_widget_types.h"

namespace gles2 {
class CommandBufferDriver;
class CommandBufferImpl;
}

namespace surfaces {

class SurfacesContextProviderDelegate;

class SurfacesContextProvider : public cc::ContextProvider,
                                public gles2::CommandBufferLocalClient,
                                public base::NonThreadSafe {
 public:
  SurfacesContextProvider(SurfacesContextProviderDelegate* delegate,
                          gfx::AcceleratedWidget widget,
                          const scoped_refptr<gles2::GpuState>& state);

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void InvalidateGrContext(uint32_t state) override;
  Capabilities ContextCapabilities() override;
  void VerifyContexts() override {}
  void DeleteCachedResources() override {}
  bool DestroyedOnMainThread() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;
  void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      override {}
  void SetupLock() override;
  base::Lock* GetLock() override;

 protected:
  friend class base::RefCountedThreadSafe<SurfacesContextProvider>;
  ~SurfacesContextProvider() override;

 private:
  // CommandBufferLocalClient:
  void UpdateVSyncParameters(int64_t timebase, int64_t interval) override;
  void DidLoseContext() override;

  // From GLES2Context:
  // Initialized in BindToCurrentThread.
  scoped_ptr<gpu::gles2::GLES2CmdHelper> gles2_helper_;
  scoped_ptr<gpu::TransferBuffer> transfer_buffer_;
  scoped_ptr<gpu::gles2::GLES2Implementation> implementation_;

  cc::ContextProvider::Capabilities capabilities_;
  LostContextCallback lost_context_callback_;

  SurfacesContextProviderDelegate* delegate_;
  scoped_refptr<gles2::GpuState> state_;
  gfx::AcceleratedWidget widget_;
  scoped_ptr<gles2::CommandBufferLocal> command_buffer_local_;

  base::Lock context_lock_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesContextProvider);
};

}  // namespace surfaces

#endif  // COMPONENTS_VIEW_MANAGER_SURFACES_SURFACES_CONTEXT_PROVIDER_H_
