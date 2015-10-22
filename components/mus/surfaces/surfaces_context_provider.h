// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_SURFACES_SURFACES_CONTEXT_PROVIDER_H_
#define COMPONENTS_MUS_SURFACES_SURFACES_CONTEXT_PROVIDER_H_

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/context_provider.h"
#include "components/mus/gles2/command_buffer_local_client.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {

class TransferBuffer;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}

}  // namespace gpu

namespace mus {

class CommandBufferDriver;
class CommandBufferImpl;
class CommandBufferLocal;
class GpuState;
class SurfacesContextProviderDelegate;

class SurfacesContextProvider : public cc::ContextProvider,
                                public CommandBufferLocalClient,
                                public base::NonThreadSafe {
 public:
  SurfacesContextProvider(SurfacesContextProviderDelegate* delegate,
                          gfx::AcceleratedWidget widget,
                          const scoped_refptr<GpuState>& state);

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void InvalidateGrContext(uint32_t state) override;
  Capabilities ContextCapabilities() override;
  void DeleteCachedResources() override {}
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;
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
  gfx::AcceleratedWidget widget_;
  scoped_ptr<CommandBufferLocal> command_buffer_local_;

  base::Lock context_lock_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesContextProvider);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_SURFACES_SURFACES_CONTEXT_PROVIDER_H_
