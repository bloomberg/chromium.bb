// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_CONTEXT_PROVIDER_H_
#define CC_OUTPUT_CONTEXT_PROVIDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"

class GrContext;
namespace WebKit { class WebGraphicsContext3D; }

namespace cc {

class ContextProvider : public base::RefCountedThreadSafe<ContextProvider> {
 public:
  // Bind the 3d context to the current thread. This should be called before
  // accessing the contexts. Calling it more than once should have no effect.
  // Once this function has been called, the class should only be accessed
  // from the same thread.
  virtual bool BindToCurrentThread() = 0;

  virtual WebKit::WebGraphicsContext3D* Context3d() = 0;
  virtual class GrContext* GrContext() = 0;

  // Ask the provider to check if the contexts are valid or lost. If they are,
  // this should invalidate the provider so that it can be replaced with a new
  // one.
  virtual void VerifyContexts() = 0;

  // A method to be called from the main thread that should return true if
  // the context inside the provider is no longer valid.
  virtual bool DestroyedOnMainThread() = 0;

  // Sets a callback to be called when the context is lost. This should be
  // called from the same thread that the context is bound to. To avoid races,
  // it should be called before BindToCurrentThread(), or VerifyContexts()
  // should be called after setting the callback.
  typedef base::Closure LostContextCallback;
  virtual void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ContextProvider>;
  virtual ~ContextProvider() {}
};

}  // namespace cc

#endif  // CC_OUTPUT_CONTEXT_PROVIDER_H_
