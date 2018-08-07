// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_BARRIER_BUILDER_H_
#define CONTENT_TEST_BARRIER_BUILDER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace content {

// A BarrierBuilder helps create a barrier for a large, complex, or variable
// number of steps.
//
// Example usage:
//
// base::RunLoop loop;
// {
//   BarrierBuilder barrier(loop.QuitClosure());
//   AsyncMethod(barrier.AddClosure());
//   AsyncMethod(barrier.AddClosure());
// }
// loop.Run();
//
// Once all closures returned by AddClosure AND the BarrierBuilder object are
// all destroyed, the |done_closure| is called. The |done_closure| will
// be invoked on the thread that calls the last step closure, or on the thread
// where BarrierBuilder goes out of scope (if that happens after all closures
// are called).
class BarrierBuilder {
 public:
  explicit BarrierBuilder(base::OnceClosure done_closure);
  ~BarrierBuilder();

  // Returns a closure that can be used in the same way that a barrier closure
  // would be used. Furthermore, each callback returned by AddClosure() should
  // eventually be run, or else 'done_closure' will never be run.
  base::OnceClosure AddClosure();

 private:
  class InternalBarrierBuilder;

  scoped_refptr<InternalBarrierBuilder> internal_barrier_;

  DISALLOW_COPY_AND_ASSIGN(BarrierBuilder);
};

}  // namespace content

#endif  // CONTENT_TEST_BARRIER_BUILDER_H_
