// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_CONTEXT_PROVIDER_H_
#define CC_TEST_FAKE_CONTEXT_PROVIDER_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "cc/context_provider.h"

namespace cc {
class TestWebGraphicsContext3D;

class FakeContextProvider : public cc::ContextProvider {
 public:
  typedef base::Callback<scoped_ptr<TestWebGraphicsContext3D>(void)>
    CreateCallback;

  FakeContextProvider();
  explicit FakeContextProvider(const CreateCallback& create_callback);

  virtual bool InitializeOnMainThread() OVERRIDE;
  virtual bool BindToCurrentThread() OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* Context3d() OVERRIDE;
  virtual class GrContext* GrContext() OVERRIDE;
  virtual void VerifyContexts() OVERRIDE;

  bool DestroyedOnMainThread();

 protected:
  virtual ~FakeContextProvider();

  CreateCallback create_callback_;
  scoped_ptr<WebKit::WebGraphicsContext3D> context3d_;

  base::Lock destroyed_lock_;
  bool destroyed_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_CONTEXT_PROVIDER_H_

