// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_CONTEXT_PROVIDER_H_
#define COMPONENTS_MUS_PUBLIC_CPP_CONTEXT_PROVIDER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "cc/output/context_provider.h"
#include "mojo/public/c/gles2/gles2_types.h"
#include "mojo/public/cpp/system/core.h"

namespace mus {

class ContextProvider : public cc::ContextProvider {
 public:
  explicit ContextProvider(mojo::ScopedMessagePipeHandle command_buffer_handle);

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void InvalidateGrContext(uint32_t state) override;
  void SetupLock() override;
  base::Lock* GetLock() override;
  gpu::Capabilities ContextCapabilities() override;
  void DeleteCachedResources() override {}
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override {}

 protected:
  friend class base::RefCountedThreadSafe<ContextProvider>;
  ~ContextProvider() override;

 private:
  static void ContextLostThunk(void* closure) {
    static_cast<ContextProvider*>(closure)->ContextLost();
  }
  void ContextLost();

  mojo::ScopedMessagePipeHandle command_buffer_handle_;
  MojoGLES2Context context_;
  std::unique_ptr<gpu::gles2::GLES2Interface> context_gl_;

  base::Lock context_lock_;

  DISALLOW_COPY_AND_ASSIGN(ContextProvider);
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_CONTEXT_PROVIDER_H_
