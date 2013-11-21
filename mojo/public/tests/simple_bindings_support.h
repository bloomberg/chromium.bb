// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_TESTS_SIMPLE_BINDINGS_SUPPORT_H_
#define MOJO_PUBLIC_TESTS_SIMPLE_BINDINGS_SUPPORT_H_

#include <list>

#include "mojo/public/bindings/lib/bindings_support.h"

namespace mojo {
namespace test {

class SimpleBindingsSupport : public BindingsSupport {
 public:
  SimpleBindingsSupport();
  virtual ~SimpleBindingsSupport();

  virtual AsyncWaitID AsyncWait(const Handle& handle,
                                MojoWaitFlags flags,
                                AsyncWaitCallback* callback) MOJO_OVERRIDE;
  virtual void CancelWait(AsyncWaitID async_wait_id) MOJO_OVERRIDE;

  // This method is called by unit tests to check the status of any handles
  // that we are asynchronously waiting on and to dispatch callbacks for any
  // handles that are ready.
  void Process();

 private:
  bool IsReady(const Handle& handle, MojoWaitFlags flags, MojoResult* result);

  struct Waiter {
    Handle handle;
    MojoWaitFlags flags;
    AsyncWaitCallback* callback;
  };

  typedef std::list<Waiter*> WaiterList;
  WaiterList waiters_;
};

}  // namespace test
}  // namespace mojo

#endif  // MOJO_PUBLIC_TESTS_SIMPLE_BINDINGS_SUPPORT_H_
