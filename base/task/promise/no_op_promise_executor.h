// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_NO_OP_PROMISE_EXECUTOR_H_
#define BASE_TASK_PROMISE_NO_OP_PROMISE_EXECUTOR_H_

#include "base/macros.h"
#include "base/task/promise/abstract_promise.h"

namespace base {
namespace internal {

// An Executor that doesn't do anything.
class BASE_EXPORT NoOpPromiseExecutor : public AbstractPromise::Executor {
 public:
  NoOpPromiseExecutor(bool can_resolve, bool can_reject);

  ~NoOpPromiseExecutor() override;

  static scoped_refptr<internal::AbstractPromise> Create(
      Location from_here,
      bool can_resolve,
      bool can_reject,
      RejectPolicy reject_policy);

  PrerequisitePolicy GetPrerequisitePolicy() override;
  bool IsCancelled() const override;

#if DCHECK_IS_ON()
  ArgumentPassingType ResolveArgumentPassingType() const override;
  ArgumentPassingType RejectArgumentPassingType() const override;
  bool CanResolve() const override;
  bool CanReject() const override;
#endif
  void Execute(AbstractPromise* promise) override;

 private:
#if DCHECK_IS_ON()
  bool can_resolve_;
  bool can_reject_;
#endif
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_NO_OP_PROMISE_EXECUTOR_H_
