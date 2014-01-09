// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_UTILITY_BINDINGS_SUPPORT_IMPL_H_
#define MOJO_PUBLIC_UTILITY_BINDINGS_SUPPORT_IMPL_H_

#include "mojo/public/bindings/lib/bindings_support.h"
#include "mojo/public/system/macros.h"

namespace mojo {
namespace internal {

// BindingsSupport implementation that uses RunLoop. Before using this you must
// have created a RunLoop on the current thread.
// You shouldn't create this directly, instead use Environment.
class BindingsSupportImpl : public BindingsSupport {
 public:
  BindingsSupportImpl();
  virtual ~BindingsSupportImpl();

  // Sets up state needed for BindingsSupportImpl. This must be invoked before
  // creating a BindingsSupportImpl.
  static void SetUp();

  // Cleans state created by Setup().
  static void TearDown();

  // BindingsSupport methods:
  virtual Buffer* GetCurrentBuffer() MOJO_OVERRIDE;
  virtual Buffer* SetCurrentBuffer(Buffer* buf) MOJO_OVERRIDE;
  virtual AsyncWaitID AsyncWait(const Handle& handle,
                                MojoWaitFlags flags,
                                AsyncWaitCallback* callback) MOJO_OVERRIDE;
  virtual void CancelWait(AsyncWaitID async_wait_id) MOJO_OVERRIDE;

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(BindingsSupportImpl);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_UTILITY_BINDINGS_SUPPORT_IMPL_H_
