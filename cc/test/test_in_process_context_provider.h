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

  virtual bool BindToCurrentThread() override;
  virtual gpu::gles2::GLES2Interface* ContextGL() override;
  virtual gpu::ContextSupport* ContextSupport() override;
  virtual class GrContext* GrContext() override;
  virtual Capabilities ContextCapabilities() override;
  virtual bool IsContextLost() override;
  virtual void VerifyContexts() override;
  virtual void DeleteCachedResources() override;
  virtual bool DestroyedOnMainThread() override;
  virtual void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) override;
  virtual void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      override;

 protected:
  friend class base::RefCountedThreadSafe<TestInProcessContextProvider>;
  virtual ~TestInProcessContextProvider();

 private:
  scoped_ptr<gpu::GLInProcessContext> context_;
  skia::RefPtr<class GrContext> gr_context_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_IN_PROCESS_CONTEXT_PROVIDER_H_
