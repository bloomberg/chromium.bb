// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CC_CONTEXT_PROVIDER_MOJO_H_
#define MOJO_CC_CONTEXT_PROVIDER_MOJO_H_

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "cc/output/context_provider.h"
#include "third_party/mojo/src/mojo/public/c/gles2/gles2.h"
#include "third_party/mojo/src/mojo/public/cpp/system/core.h"

namespace mojo {

class ContextProviderMojo : public cc::ContextProvider {
 public:
  explicit ContextProviderMojo(ScopedMessagePipeHandle command_buffer_handle);

  // cc::ContextProvider implementation.
  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  void SetupLock() override;
  base::Lock* GetLock() override;
  Capabilities ContextCapabilities() override;
  bool IsContextLost() override;
  void VerifyContexts() override {}
  void DeleteCachedResources() override {}
  bool DestroyedOnMainThread() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override {}
  void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      override {}

 protected:
  friend class base::RefCountedThreadSafe<ContextProviderMojo>;
  ~ContextProviderMojo() override;

 private:
  static void ContextLostThunk(void* closure) {
    static_cast<ContextProviderMojo*>(closure)->ContextLost();
  }
  void ContextLost();

  cc::ContextProvider::Capabilities capabilities_;
  ScopedMessagePipeHandle command_buffer_handle_;
  MojoGLES2Context context_;
  bool context_lost_;

  base::Lock context_lock_;

  DISALLOW_COPY_AND_ASSIGN(ContextProviderMojo);
};

}  // namespace mojo

#endif  // MOJO_CC_CONTEXT_PROVIDER_MOJO_H_
