// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CC_CONTEXT_PROVIDER_MOJO_H_
#define MOJO_CC_CONTEXT_PROVIDER_MOJO_H_

#include "base/macros.h"
#include "cc/output/context_provider.h"
#include "mojo/public/c/gles2/gles2.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

class ContextProviderMojo : public cc::ContextProvider {
 public:
  explicit ContextProviderMojo(ScopedMessagePipeHandle command_buffer_handle);

  // cc::ContextProvider implementation.
  virtual bool BindToCurrentThread() override;
  virtual gpu::gles2::GLES2Interface* ContextGL() override;
  virtual gpu::ContextSupport* ContextSupport() override;
  virtual class GrContext* GrContext() override;
  virtual Capabilities ContextCapabilities() override;
  virtual bool IsContextLost() override;
  virtual void VerifyContexts() override {}
  virtual void DeleteCachedResources() override {}
  virtual bool DestroyedOnMainThread() override;
  virtual void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override {}
  virtual void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      override {}

 protected:
  friend class base::RefCountedThreadSafe<ContextProviderMojo>;
  virtual ~ContextProviderMojo();

 private:
  static void ContextLostThunk(void* closure) {
    static_cast<ContextProviderMojo*>(closure)->ContextLost();
  }
  void ContextLost();

  cc::ContextProvider::Capabilities capabilities_;
  ScopedMessagePipeHandle command_buffer_handle_;
  MojoGLES2Context context_;
  bool context_lost_;

  DISALLOW_COPY_AND_ASSIGN(ContextProviderMojo);
};

}  // namespace mojo

#endif  // MOJO_CC_CONTEXT_PROVIDER_MOJO_H_
