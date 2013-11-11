// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_SUPPORT_H_
#define MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_SUPPORT_H_

#include "mojo/public/system/core.h"

namespace mojo {

// An embedder of the bindings library MUST implement BindingsSupport and call
// BindingsSupport::Set prior to using the library.
class BindingsSupport {
 public:
  class AsyncWaitCallback {
   public:
    virtual void OnHandleReady(MojoResult result) = 0;
  };

  // Asynchronously call MojoWait on a background thread, and return the result
  // to the current thread via the given AsyncWaitCallback.
  virtual bool AsyncWait(Handle handle,
                         MojoWaitFlags flags,
                         MojoDeadline deadline,
                         AsyncWaitCallback* callback) = 0;

  // Cancel an existing call to AsyncWait with the given callback. The
  // callback's OnHandleReady method should not be called in this case.
  virtual void CancelWait(AsyncWaitCallback* callback) = 0;

  static void Set(BindingsSupport* support);
  static BindingsSupport* Get();
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_SUPPORT_H_
