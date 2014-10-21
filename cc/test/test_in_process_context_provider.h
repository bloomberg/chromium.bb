// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
#define CC_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_

#include "cc/output/context_provider.h"
#include "skia/ext/refptr.h"

class GrContext;

namespace gpu {
class GLInProcessContext;
}

namespace cc {

scoped_ptr<gpu::GLInProcessContext> CreateTestInProcessContext();

class TestInProcessContextProvider : public ContextProvider {
 public:
  TestInProcessContextProvider();

  bool BindToCurrentThread() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  Capabilities ContextCapabilities() override;
  bool IsContextLost() override;
  void VerifyContexts() override;
  void DeleteCachedResources() override;
  bool DestroyedOnMainThread() override;
  void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;
  void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      override;

 protected:
  friend class base::RefCountedThreadSafe<TestInProcessContextProvider>;
  ~TestInProcessContextProvider() override;

 private:
  scoped_ptr<gpu::GLInProcessContext> context_;
  skia::RefPtr<class GrContext> gr_context_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
