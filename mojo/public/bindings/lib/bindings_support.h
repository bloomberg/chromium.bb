// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_SUPPORT_H_
#define MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_SUPPORT_H_

#include "mojo/public/system/core_cpp.h"

namespace mojo {

// An embedder of the bindings library MUST implement BindingsSupport and call
// BindingsSupport::Set prior to using the library.
class BindingsSupport {
 public:
  class AsyncWaitCallback {
   public:
    virtual ~AsyncWaitCallback() {}
    virtual void OnHandleReady(MojoResult result) = 0;
  };

  typedef void* AsyncWaitID;

  static void Set(BindingsSupport* support);
  static BindingsSupport* Get();

  // Asynchronously call MojoWait on a background thread, and pass the result
  // of MojoWait to the given AsyncWaitCallback on the current thread.  Returns
  // an AsyncWaitID that can be used with CancelWait to stop waiting. This
  // identifier becomes invalid once the callback runs.
  virtual AsyncWaitID AsyncWait(const Handle& handle,
                                MojoWaitFlags flags,
                                AsyncWaitCallback* callback) = 0;

  // Cancel an existing call to AsyncWait with the given AsyncWaitID. The
  // corresponding AsyncWaitCallback's OnHandleReady method will not be called
  // in this case.
  virtual void CancelWait(AsyncWaitID id) = 0;

 protected:
  virtual ~BindingsSupport() {}
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_SUPPORT_H_
