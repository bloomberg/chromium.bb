// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/context_provider.h"
#include "mojo/public/gles2/gles2.h"
#include "mojo/public/system/core_cpp.h"

namespace mojo {
namespace examples {

class MojoContextProvider : public cc::ContextProvider {
 public:
  explicit MojoContextProvider(ScopedMessagePipeHandle gl_pipe);

  // cc::ContextProvider implementation.
  virtual bool BindToCurrentThread() OVERRIDE;
  virtual gpu::gles2::GLES2Interface* ContextGL() OVERRIDE;
  virtual gpu::ContextSupport* ContextSupport() OVERRIDE;
  virtual class GrContext* GrContext() OVERRIDE;
  virtual Capabilities ContextCapabilities() OVERRIDE;
  virtual bool IsContextLost() OVERRIDE;
  virtual void VerifyContexts() OVERRIDE {}
  virtual bool DestroyedOnMainThread() OVERRIDE;
  virtual void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) OVERRIDE {}
  virtual void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      OVERRIDE {}

 protected:
  friend class base::RefCountedThreadSafe<MojoContextProvider>;
  virtual ~MojoContextProvider();

 private:
  static void ContextLostThunk(void* closure) {
    static_cast<MojoContextProvider*>(closure)->ContextLost();
  }
  void ContextLost();

  cc::ContextProvider::Capabilities capabilities_;
  ScopedMessagePipeHandle gl_pipe_;
  MojoGLES2Context context_;
};

}  // namespace examples
}  // namespace mojo
