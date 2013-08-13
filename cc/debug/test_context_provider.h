// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_TEST_CONTEXT_PROVIDER_H_
#define CC_DEBUG_TEST_CONTEXT_PROVIDER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/base/cc_export.h"
#include "cc/output/context_provider.h"

namespace WebKit { class WebGraphicsContext3D; }

namespace cc {
class TestWebGraphicsContext3D;

class CC_EXPORT TestContextProvider
    : public NON_EXPORTED_BASE(cc::ContextProvider) {
 public:
  typedef base::Callback<scoped_ptr<TestWebGraphicsContext3D>(void)>
    CreateCallback;

  static scoped_refptr<TestContextProvider> Create();
  static scoped_refptr<TestContextProvider> Create(
      const CreateCallback& create_callback);

  virtual bool BindToCurrentThread() OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE;
  virtual class GrContext* GrContext() OVERRIDE;
  virtual void VerifyContexts() OVERRIDE;
  virtual bool DestroyedOnMainThread() OVERRIDE;
  virtual void SetLostContextCallback(const LostContextCallback& cb) OVERRIDE;

  TestWebGraphicsContext3D* TestContext3d() { return context3d_.get(); }

 protected:
  TestContextProvider();
  virtual ~TestContextProvider();

  bool InitializeOnMainThread(const CreateCallback& create_callback);

  scoped_ptr<TestWebGraphicsContext3D> context3d_;
  bool bound_;

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  base::Lock destroyed_lock_;
  bool destroyed_;
};

}  // namespace cc

#endif  // CC_DEBUG_TEST_CONTEXT_PROVIDER_H_

