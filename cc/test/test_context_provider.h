// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_CONTEXT_PROVIDER_H_
#define CC_TEST_TEST_CONTEXT_PROVIDER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/output/context_provider.h"
#include "cc/test/test_context_support.h"
#include "gpu/command_buffer/client/gles2_interface_stub.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace cc {
class TestWebGraphicsContext3D;
class TestGLES2Interface;

class TestContextProvider : public ContextProvider {
 public:
  typedef base::Callback<std::unique_ptr<TestWebGraphicsContext3D>(void)>
      CreateCallback;

  static scoped_refptr<TestContextProvider> Create();
  // Creates a worker context provider that can be used on any thread. This is
  // equivalent to: Create(); BindToCurrentThread().
  static scoped_refptr<TestContextProvider> CreateWorker();
  static scoped_refptr<TestContextProvider> Create(
      std::unique_ptr<TestWebGraphicsContext3D> context);
  static scoped_refptr<TestContextProvider> Create(
      std::unique_ptr<TestWebGraphicsContext3D> context,
      std::unique_ptr<TestContextSupport> support);
  static scoped_refptr<TestContextProvider> Create(
      std::unique_ptr<TestGLES2Interface> gl);

  bool BindToCurrentThread() override;
  void DetachFromThread() override;
  gpu::Capabilities ContextCapabilities() override;
  gpu::gles2::GLES2Interface* ContextGL() override;
  gpu::ContextSupport* ContextSupport() override;
  class GrContext* GrContext() override;
  ContextCacheController* CacheController() override;
  void InvalidateGrContext(uint32_t state) override;
  base::Lock* GetLock() override;
  void SetLostContextCallback(const LostContextCallback& cb) override;

  TestWebGraphicsContext3D* TestContext3d();

  // This returns the TestWebGraphicsContext3D but is valid to call
  // before the context is bound to a thread. This is needed to set up
  // state on the test context before binding. Don't call
  // InitializeOnCurrentThread on the context returned from this method.
  TestWebGraphicsContext3D* UnboundTestContext3d();

  TestContextSupport* support() { return support_.get(); }

 protected:
  explicit TestContextProvider(
      std::unique_ptr<TestContextSupport> support,
      std::unique_ptr<TestGLES2Interface> gl,
      std::unique_ptr<TestWebGraphicsContext3D> context);
  ~TestContextProvider() override;

 private:
  void OnLostContext();

  std::unique_ptr<TestContextSupport> support_;
  std::unique_ptr<TestWebGraphicsContext3D> context3d_;
  std::unique_ptr<TestGLES2Interface> context_gl_;
  sk_sp<class GrContext> gr_context_;
  std::unique_ptr<ContextCacheController> cache_controller_;
  bool bound_ = false;

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  base::Lock context_lock_;

  LostContextCallback lost_context_callback_;

  base::WeakPtrFactory<TestContextProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestContextProvider);
};

}  // namespace cc

#endif  // CC_TEST_TEST_CONTEXT_PROVIDER_H_

