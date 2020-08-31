// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SUBMITTABLE_EXECUTOR_H_
#define CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SUBMITTABLE_EXECUTOR_H_

#include <memory>

#include "chromeos/components/nearby/library/callable.h"
#include "chromeos/components/nearby/library/down_cast.h"
#include "chromeos/components/nearby/library/executor.h"
#include "chromeos/components/nearby/library/future.h"
// TODO(kyleqian): Use Ptr once the Nearby library is merged in.
// #include "location/nearby/cpp/platform/ptr.h"
#include "chromeos/components/nearby/library/runnable.h"

namespace location {
namespace nearby {

// Each per-platform concrete implementation is expected to extend from
// SubmittableExecutor and provide an override of its submit() method.
//
// e.g.
// class IOSSubmittableExecutor
//                      : public SubmittableExecutor<IOSSubmittableExecutor> {
//  public:
//   template <typename T>
//   Ptr<Future<T> > submit(Ptr<Callable<T> > callable) {
//     ...
//   }
// }
template <typename ConcreteSubmittableExecutor>
class SubmittableExecutor : public Executor {
 public:
  virtual ~SubmittableExecutor() {}

  template <typename T>
  std::shared_ptr<Future<T>> submit(std::shared_ptr<Callable<T>> callable) {
    return DOWN_CAST<ConcreteSubmittableExecutor*>(this)->submit(callable);
  }

  // https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executor.html#execute-java.lang.Runnable-
  virtual void execute(std::shared_ptr<Runnable> runnable) = 0;
};

}  // namespace nearby
}  // namespace location

#endif  // CHROMEOS_COMPONENTS_NEARBY_LIBRARY_SUBMITTABLE_EXECUTOR_H_
