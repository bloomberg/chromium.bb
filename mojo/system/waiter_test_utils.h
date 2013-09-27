// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_WAITER_TEST_UTILS_H_
#define MOJO_SYSTEM_WAITER_TEST_UTILS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/system/core.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/waiter.h"

namespace mojo {
namespace system {
namespace test {

// This is a very simple thread that has a |Waiter|, on which it waits
// indefinitely (and records the result). It will create and initialize the
// |Waiter| on creation, but the caller must start the thread with |Start()|. It
// will join the thread on destruction.
//
// One usually uses it like:
//
//    MojoResult result;
//    {
//      WaiterList waiter_list;
//      test::SimpleWaiterThread thread(&result);
//      waiter_list.AddWaiter(thread.waiter(), ...);
//      thread.Start();
//      ... some stuff to wake the waiter ...
//      waiter_list.RemoveWaiter(thread.waiter());
//    }  // Join |thread|.
//    EXPECT_EQ(..., result);
//
// There's a bit of unrealism in its use: In this sort of usage, calls such as
// |Waiter::Init()|, |AddWaiter()|, and |RemoveWaiter()| are done in the main
// (test) thread, not the waiter thread (as would actually happen in real code).
// (We accept this unrealism for simplicity, since |WaiterList| is
// thread-unsafe so making it more realistic would require adding nontrivial
// synchronization machinery.)
class SimpleWaiterThread : public base::SimpleThread {
 public:
  // For the duration of the lifetime of this object, |*result| belongs to it
  // (in the sense that it will write to it whenever it wants).
  explicit SimpleWaiterThread(MojoResult* result);
  virtual ~SimpleWaiterThread();  // Joins the thread.

  Waiter* waiter() { return &waiter_; }

 private:
  virtual void Run() OVERRIDE;

  MojoResult* const result_;
  Waiter waiter_;

  DISALLOW_COPY_AND_ASSIGN(SimpleWaiterThread);
};

// This is a more complex and realistic thread that has a |Waiter|, on which it
// waits for the given deadline (with the given flags). Unlike
// |SimpleWaiterThread|, it requires the machinery of |Dispatcher|.
class WaiterThread : public base::SimpleThread {
 public:
  // Note: |*did_wait_out| and |*result| belong to this object while it's alive.
  WaiterThread(scoped_refptr<Dispatcher> dispatcher,
               MojoWaitFlags wait_flags,
               MojoDeadline deadline,
               MojoResult success_result,
               bool* did_wait_out,
               MojoResult* result_out);
  virtual ~WaiterThread();

 private:
  virtual void Run() OVERRIDE;

  const scoped_refptr<Dispatcher> dispatcher_;
  const MojoWaitFlags wait_flags_;
  const MojoDeadline deadline_;
  const MojoResult success_result_;
  bool* const did_wait_out_;
  MojoResult* const result_out_;

  Waiter waiter_;

  DISALLOW_COPY_AND_ASSIGN(WaiterThread);
};

}  // namespace test
}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_WAITER_TEST_UTILS_H_
