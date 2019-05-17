// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_THEN_AND_CATCH_EXECUTOR_H_
#define BASE_TASK_PROMISE_THEN_AND_CATCH_EXECUTOR_H_

#include <type_traits>

#include "base/callback.h"
#include "base/task/promise/abstract_promise.h"
#include "base/task/promise/helpers.h"

namespace base {
namespace internal {

// Exists to reduce template bloat.
class BASE_EXPORT BaseThenAndCatchExecutor : public AbstractPromise::Executor {
 public:
  BaseThenAndCatchExecutor(CallbackBase&& resolve_callback,
                           CallbackBase&& reject_callback);

  ~BaseThenAndCatchExecutor() override;

  // AbstractPromise::Executor:
  bool IsCancelled() const override;
  PrerequisitePolicy GetPrerequisitePolicy() override;
  void Execute(AbstractPromise* promise) override;

 protected:
  virtual void ExecuteThen(AbstractPromise* prerequisite,
                           AbstractPromise* promise) = 0;

  virtual void ExecuteCatch(AbstractPromise* prerequisite,
                            AbstractPromise* promise) = 0;

  // If |executor| is null then the value of |arg| is moved or copied into
  // |result| and true is returned. Otherwise false is returned.
  static bool ProcessNullCallback(const CallbackBase& executor,
                                  AbstractPromise* arg,
                                  AbstractPromise* result);

  CallbackBase resolve_callback_;
  CallbackBase reject_callback_;
};

// Tag signals no callback which is used to eliminate dead code.
struct NoCallback {};

struct CouldResolveOrReject {};
struct CanOnlyResolve {};
struct CanOnlyReject {};

template <bool can_resolve, bool can_reject>
struct CheckResultHelper;

template <>
struct CheckResultHelper<true, false> {
  using TagType = CanOnlyResolve;
};

template <>
struct CheckResultHelper<true, true> {
  using TagType = CouldResolveOrReject;
};

template <>
struct CheckResultHelper<false, true> {
  using TagType = CanOnlyReject;
};

template <typename ResolveOnceCallback,
          typename RejectOnceCallback,
          typename ArgResolve,
          typename ArgReject,
          typename ResolveStorage,
          typename RejectStorage>
class ThenAndCatchExecutor : public BaseThenAndCatchExecutor {
 public:
  using ResolveReturnT =
      typename CallbackTraits<ResolveOnceCallback>::ReturnType;
  using RejectReturnT = typename CallbackTraits<RejectOnceCallback>::ReturnType;
  using PrerequisiteCouldResolve =
      std::integral_constant<bool,
                             !std::is_same<ArgResolve, NoCallback>::value>;
  using PrerequisiteCouldReject =
      std::integral_constant<bool, !std::is_same<ArgReject, NoCallback>::value>;

  ThenAndCatchExecutor(ResolveOnceCallback&& resolve_callback,
                       RejectOnceCallback&& reject_callback)
      : BaseThenAndCatchExecutor(std::move(resolve_callback),
                                 std::move(reject_callback)) {
    static_assert(sizeof(CallbackBase) == sizeof(ResolveOnceCallback),
                  "We assume it's possible to cast from CallbackBase to "
                  "ResolveOnceCallback");
    static_assert(sizeof(CallbackBase) == sizeof(RejectOnceCallback),
                  "We assume it's possible to cast from CallbackBase to "
                  "RejectOnceCallback");
  }

  ~ThenAndCatchExecutor() override = default;

  void ExecuteThen(AbstractPromise* prerequisite,
                   AbstractPromise* promise) override {
    ExecuteThenInternal(prerequisite, promise, PrerequisiteCouldResolve());
  }

  void ExecuteCatch(AbstractPromise* prerequisite,
                    AbstractPromise* promise) override {
    ExecuteCatchInternal(prerequisite, promise, PrerequisiteCouldReject());
  }

#if DCHECK_IS_ON()
  ArgumentPassingType ResolveArgumentPassingType() const override {
    return resolve_callback_.is_null()
               ? AbstractPromise::Executor::ArgumentPassingType::kNoCallback
               : CallbackTraits<ResolveOnceCallback>::argument_passing_type;
  }

  ArgumentPassingType RejectArgumentPassingType() const override {
    return reject_callback_.is_null()
               ? AbstractPromise::Executor::ArgumentPassingType::kNoCallback
               : CallbackTraits<RejectOnceCallback>::argument_passing_type;
  }

  bool CanResolve() const override {
    return (!resolve_callback_.is_null() &&
            PromiseCallbackTraits<ResolveReturnT>::could_resolve) ||
           (!reject_callback_.is_null() &&
            PromiseCallbackTraits<RejectReturnT>::could_resolve);
  }

  bool CanReject() const override {
    return (!resolve_callback_.is_null() &&
            PromiseCallbackTraits<ResolveReturnT>::could_reject) ||
           (!reject_callback_.is_null() &&
            PromiseCallbackTraits<RejectReturnT>::could_reject);
  }
#endif

 private:
  void ExecuteThenInternal(AbstractPromise* prerequisite,
                           AbstractPromise* promise,
                           std::true_type can_resolve) {
    ResolveOnceCallback* resolve_callback =
        static_cast<ResolveOnceCallback*>(&resolve_callback_);
    RunHelper<ResolveOnceCallback, Resolved<ArgResolve>, ResolveStorage,
              RejectStorage>::Run(std::move(*resolve_callback), prerequisite,
                                  promise);

    using CheckResultTagType = typename CheckResultHelper<
        PromiseCallbackTraits<ResolveReturnT>::could_resolve,
        PromiseCallbackTraits<ResolveReturnT>::could_reject>::TagType;

    CheckResultType(promise, CheckResultTagType());
  }

  void ExecuteThenInternal(AbstractPromise* prerequisite,
                           AbstractPromise* promise,
                           std::false_type can_resolve) {
    // |prerequisite| can't resolve so don't generate dead code.
  }

  void ExecuteCatchInternal(AbstractPromise* prerequisite,
                            AbstractPromise* promise,
                            std::true_type can_reject) {
    RejectOnceCallback* reject_callback =
        static_cast<RejectOnceCallback*>(&reject_callback_);
    RunHelper<RejectOnceCallback, Rejected<ArgReject>, ResolveStorage,
              RejectStorage>::Run(std::move(*reject_callback), prerequisite,
                                  promise);

    using CheckResultTagType = typename CheckResultHelper<
        PromiseCallbackTraits<RejectReturnT>::could_resolve,
        PromiseCallbackTraits<RejectReturnT>::could_reject>::TagType;

    CheckResultType(promise, CheckResultTagType());
  }

  void ExecuteCatchInternal(AbstractPromise* prerequisite,
                            AbstractPromise* promise,
                            std::false_type can_reject) {
    // |prerequisite| can't reject so don't generate dead code.
  }

  void CheckResultType(AbstractPromise* promise, CouldResolveOrReject) {
    if (promise->IsResolvedWithPromise() ||
        promise->value().type() == TypeId::From<ResolveStorage>()) {
      promise->OnResolved();
    } else {
      DCHECK_EQ(promise->value().type(), TypeId::From<RejectStorage>())
          << " See " << promise->from_here().ToString();
      promise->OnRejected();
    }
  }

  void CheckResultType(AbstractPromise* promise, CanOnlyResolve) {
    promise->OnResolved();
  }

  void CheckResultType(AbstractPromise* promise, CanOnlyReject) {
    promise->OnRejected();
  }
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_THEN_AND_CATCH_EXECUTOR_H_
