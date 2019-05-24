// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_PROMISE_H_
#define BASE_TASK_PROMISE_PROMISE_H_

#include "base/task/post_task.h"
#include "base/task/promise/finally_executor.h"
#include "base/task/promise/helpers.h"
#include "base/task/promise/no_op_promise_executor.h"
#include "base/task/promise/promise_result.h"
#include "base/task/promise/then_and_catch_executor.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace base {

// Inspired by ES6 promises, Promise<> is a PostTask based callback system for
// asynchronous operations. An operation can resolve (succeed) with a value and
// optionally reject (fail) with a different result. Interested parties can be
// notified using ThenOn() and CatchOn() which schedule callbacks to run as
// appropriate on the specified task runner or task traits. If a promise is
// settled when a ThenOn() / CatchOn() / FinallyOn() statement is added, the
// callback will be posted immediately, otherwise it has to wait.
//
// Promise<> is copyable, moveable and thread safe. Under the hood
// internal::AbstractPromise is refcounted so retaining multiple Promises<> will
// prevent that part of the promise graph from being released.
template <typename ResolveType, typename RejectType = NoReject>
class Promise {
 public:
  Promise() : abstract_promise_(nullptr) {}

  explicit Promise(
      scoped_refptr<internal::AbstractPromise> abstract_promise) noexcept
      : abstract_promise_(std::move(abstract_promise)) {}

  // Constructs an unresolved promise for use by a ManualPromiseResolver<> and
  // TaskRunner::PostPromise.
  Promise(scoped_refptr<TaskRunner> task_runner,
          const Location& location,
          RejectPolicy reject_policy)
      : abstract_promise_(MakeRefCounted<internal::AbstractPromise>(
            std::move(task_runner),
            location,
            nullptr,
            reject_policy,
            internal::AbstractPromise::ConstructWith<
                internal::DependentList::ConstructUnresolved,
                internal::NoOpPromiseExecutor>(),
            /* can_resolve */ !std::is_same<ResolveType, NoResolve>::value,
            /* can_reject */ !std::is_same<RejectType, NoReject>::value)) {}

  NOINLINE ~Promise() = default;

  operator bool() const { return !!abstract_promise_; }

  bool IsCancelledForTesting() const {
    DCHECK(abstract_promise_);
    return abstract_promise_->IsCanceled();
  }

  // A task to execute |on_reject| is posted on |task_runner| as soon as this
  // promise (or an uncaught ancestor) is rejected. A Promise<> for the return
  // value of |on_reject| is returned.
  //
  // The following callback return types have special meanings:
  // 1. PromiseResult<Resolve, Reject> lets the callback resolve, reject or
  //    curry a Promise<Resolve, Reject>
  // 2. Promise<Resolve, Reject> where the result is a curried promise.
  //
  // If a promise has multiple Catches they will be run in order of creation.
  template <typename RejectCb>
  NOINLINE auto CatchOn(scoped_refptr<TaskRunner> task_runner,
                        const Location& from_here,
                        RejectCb&& on_reject) noexcept {
    DCHECK(abstract_promise_);

    // Extract properties from the |on_reject| callback.
    using RejectCallbackTraits = internal::CallbackTraits<RejectCb>;
    using RejectCallbackArgT = typename RejectCallbackTraits::ArgType;

    // Compute the resolve and reject types of the returned Promise.
    using ReturnedPromiseTraits =
        internal::PromiseCombiner<ResolveType,
                                  NoReject,  // We've caught the reject case.
                                  typename RejectCallbackTraits::ResolveType,
                                  typename RejectCallbackTraits::RejectType>;
    using ReturnedPromiseResolveT = typename ReturnedPromiseTraits::ResolveType;
    using ReturnedPromiseRejectT = typename ReturnedPromiseTraits::RejectType;

    static_assert(!std::is_same<NoReject, RejectType>::value,
                  "Can't catch a NoReject promise.");

    // Check we wouldn't need to return Promise<Variant<...>, ...>
    static_assert(ReturnedPromiseTraits::valid,
                  "Ambiguous promise resolve type");
    static_assert(
        internal::IsValidPromiseArg<RejectType, RejectCallbackArgT>::value ||
            std::is_void<RejectCallbackArgT>::value,
        "|on_reject| callback must accept Promise::RejectType or void.");

    return Promise<ReturnedPromiseResolveT, ReturnedPromiseRejectT>(
        MakeRefCounted<internal::AbstractPromise>(
            std::move(task_runner), from_here,
            std::make_unique<internal::AbstractPromise::AdjacencyList>(
                abstract_promise_),
            RejectPolicy::kMustCatchRejection,
            internal::AbstractPromise::ConstructWith<
                internal::DependentList::ConstructUnresolved,
                internal::ThenAndCatchExecutor<
                    OnceClosure,  // Never called.
                    OnceCallback<typename RejectCallbackTraits::SignatureType>,
                    internal::NoCallback, RejectType,
                    Resolved<ReturnedPromiseResolveT>,
                    Rejected<ReturnedPromiseRejectT>>>(),
            OnceClosure(),
            static_cast<
                OnceCallback<typename RejectCallbackTraits::SignatureType>>(
                std::forward<RejectCb>(on_reject))));
  }

  template <typename RejectCb>
  auto CatchOn(const TaskTraits& traits,
               const Location& from_here,
               RejectCb&& on_reject) noexcept {
    return CatchOn(CreateTaskRunnerWithTraits(traits), from_here,
                   std::forward<RejectCb>(on_reject));
  }

  template <typename RejectCb>
  auto CatchOnCurrent(const Location& from_here,
                      RejectCb&& on_reject) noexcept {
    return CatchOn(SequencedTaskRunnerHandle::Get(), from_here,
                   std::forward<RejectCb>(on_reject));
  }

  // A task to execute |on_resolve| is posted on |task_runner| as soon as this
  // promise (or an unhandled ancestor) is resolved. A Promise<> for the return
  // value of |on_resolve| is returned.
  //
  // The following callback return types have special meanings:
  // 1. PromiseResult<Resolve, Reject> lets the callback resolve, reject or
  //    curry a Promise<Resolve, Reject>
  // 2. Promise<Resolve, Reject> where the result is a curried promise.
  //
  // If a promise has multiple Thens they will be run in order of creation.
  template <typename ResolveCb>
  NOINLINE auto ThenOn(scoped_refptr<TaskRunner> task_runner,
                       const Location& from_here,
                       ResolveCb&& on_resolve) noexcept {
    DCHECK(abstract_promise_);

    // Extract properties from the |on_resolve| callback.
    using ResolveCallbackTraits =
        internal::CallbackTraits<std::decay_t<ResolveCb>>;
    using ResolveCallbackArgT = typename ResolveCallbackTraits::ArgType;

    // Compute the resolve and reject types of the returned Promise.
    using ReturnedPromiseTraits =
        internal::PromiseCombiner<NoResolve,  // We've caught the resolve case.
                                  RejectType,
                                  typename ResolveCallbackTraits::ResolveType,
                                  typename ResolveCallbackTraits::RejectType>;
    using ReturnedPromiseResolveT = typename ReturnedPromiseTraits::ResolveType;
    using ReturnedPromiseRejectT = typename ReturnedPromiseTraits::RejectType;

    // Check we wouldn't need to return Promise<..., Variant<...>>
    static_assert(ReturnedPromiseTraits::valid,
                  "Ambiguous promise reject type");

    static_assert(
        internal::IsValidPromiseArg<ResolveType, ResolveCallbackArgT>::value ||
            std::is_void<ResolveCallbackArgT>::value,
        "|on_resolve| callback must accept Promise::ResolveType or void.");

    return Promise<ReturnedPromiseResolveT, ReturnedPromiseRejectT>(
        MakeRefCounted<internal::AbstractPromise>(
            std::move(task_runner), from_here,
            std::make_unique<internal::AbstractPromise::AdjacencyList>(
                abstract_promise_),
            RejectPolicy::kMustCatchRejection,
            internal::AbstractPromise::ConstructWith<
                internal::DependentList::ConstructUnresolved,
                internal::ThenAndCatchExecutor<
                    OnceCallback<typename ResolveCallbackTraits::SignatureType>,
                    OnceClosure, ResolveType, internal::NoCallback,
                    Resolved<ReturnedPromiseResolveT>,
                    Rejected<ReturnedPromiseRejectT>>>(),
            std::forward<ResolveCb>(on_resolve), OnceClosure()));
  }

  template <typename ResolveCb>
  auto ThenOn(const TaskTraits& traits,
              const Location& from_here,
              ResolveCb&& on_resolve) noexcept {
    return ThenOn(CreateTaskRunnerWithTraits(traits), from_here,
                  std::forward<ResolveCb>(on_resolve));
  }

  template <typename ResolveCb>
  auto ThenOnCurrent(const Location& from_here,
                     ResolveCb&& on_resolve) noexcept {
    return ThenOn(SequencedTaskRunnerHandle::Get(), from_here,
                  std::forward<ResolveCb>(on_resolve));
  }

  // A task to execute |on_reject| is posted on |task_runner| as soon as this
  // promise (or an uncaught ancestor) is rejected. Likewise a task to execute
  // |on_resolve| is posted on |task_runner| as soon as this promise (or an
  // unhandled ancestor) is resolved. A Promise<> for the return value of
  // |on_resolve| or |on_reject| is returned.
  //
  // The following callback return types have special meanings:
  // 1. PromiseResult<Resolve, Reject> lets the callback resolve, reject or
  //    curry a Promise<Resolve, Reject>
  // 2. Promise<Resolve, Reject> where the result is a curried promise.
  //
  // If a promise has multiple Catches/ Thens, they will be run in order of
  // creation.
  //
  // Note if either |on_resolve| or |on_reject| are canceled (due to weak
  // pointer invalidation), then the other must be canceled at the same time as
  // well. This restriction only applies to this form of ThenOn/ThenOnCurrent.
  template <typename ResolveCb, typename RejectCb>
  NOINLINE auto ThenOn(scoped_refptr<TaskRunner> task_runner,
                       const Location& from_here,
                       ResolveCb&& on_resolve,
                       RejectCb&& on_reject) noexcept {
    DCHECK(abstract_promise_);

    // Extract properties from the |on_resolve| and |on_reject| callbacks.
    using ResolveCallbackTraits = internal::CallbackTraits<ResolveCb>;
    using RejectCallbackTraits = internal::CallbackTraits<RejectCb>;
    using ResolveCallbackArgT = typename ResolveCallbackTraits::ArgType;
    using RejectCallbackArgT = typename RejectCallbackTraits::ArgType;

    // Compute the resolve and reject types of the returned Promise.
    using ReturnedPromiseTraits =
        internal::PromiseCombiner<typename ResolveCallbackTraits::ResolveType,
                                  typename ResolveCallbackTraits::RejectType,
                                  typename RejectCallbackTraits::ResolveType,
                                  typename RejectCallbackTraits::RejectType>;
    using ReturnedPromiseResolveT = typename ReturnedPromiseTraits::ResolveType;
    using ReturnedPromiseRejectT = typename ReturnedPromiseTraits::RejectType;

    static_assert(!std::is_same<NoReject, RejectType>::value,
                  "Can't catch a NoReject promise.");

    static_assert(ReturnedPromiseTraits::valid,
                  "|on_resolve| callback and |on_resolve| callback must return "
                  "compatible types.");

    static_assert(
        internal::IsValidPromiseArg<ResolveType, ResolveCallbackArgT>::value ||
            std::is_void<ResolveCallbackArgT>::value,
        "|on_resolve| callback must accept Promise::ResolveType or void.");

    static_assert(
        internal::IsValidPromiseArg<RejectType, RejectCallbackArgT>::value ||
            std::is_void<RejectCallbackArgT>::value,
        "|on_reject| callback must accept Promise::RejectType or void.");

    return Promise<ReturnedPromiseResolveT, ReturnedPromiseRejectT>(
        MakeRefCounted<internal::AbstractPromise>(
            std::move(task_runner), from_here,
            std::make_unique<internal::AbstractPromise::AdjacencyList>(
                abstract_promise_),
            RejectPolicy::kMustCatchRejection,
            internal::AbstractPromise::ConstructWith<
                internal::DependentList::ConstructUnresolved,
                internal::ThenAndCatchExecutor<
                    OnceCallback<typename ResolveCallbackTraits::SignatureType>,
                    OnceCallback<typename RejectCallbackTraits::SignatureType>,
                    ResolveType, RejectType, Resolved<ReturnedPromiseResolveT>,
                    Rejected<ReturnedPromiseRejectT>>>(),
            static_cast<
                OnceCallback<typename ResolveCallbackTraits::SignatureType>>(
                std::forward<ResolveCb>(on_resolve)),
            static_cast<
                OnceCallback<typename RejectCallbackTraits::SignatureType>>(
                std::forward<RejectCb>(on_reject))));
  }

  template <typename ResolveCb, typename RejectCb>
  auto ThenOn(const TaskTraits& traits,
              const Location& from_here,
              ResolveCb&& on_resolve,
              RejectCb&& on_reject) noexcept {
    return ThenOn(CreateTaskRunnerWithTraits(traits), from_here,
                  std::forward<ResolveCb>(on_resolve),
                  std::forward<RejectCb>(on_reject));
  }

  template <typename ResolveCb, typename RejectCb>
  auto ThenOnCurrent(const Location& from_here,
                     ResolveCb&& on_resolve,
                     RejectCb&& on_reject) noexcept {
    return ThenOn(SequencedTaskRunnerHandle::Get(), from_here,
                  std::forward<ResolveCb>(on_resolve),
                  std::forward<RejectCb>(on_reject));
  }

  // A task to execute |finally_callback| on |task_runner| is posted after the
  // parent promise is resolved or rejected. |finally_callback| is not executed
  // if the parent promise is cancelled. Unlike the finally() in Javascript
  // promises, this doesn't return a Promise that is resolved or rejected with
  // the parent's value if |finally_callback| returns void. (We could support
  // this if needed it but it seems unlikely to be used).
  template <typename FinallyCb>
  NOINLINE auto FinallyOn(scoped_refptr<TaskRunner> task_runner,
                          const Location& from_here,
                          FinallyCb&& finally_callback) noexcept {
    DCHECK(abstract_promise_);

    // Extract properties from |finally_callback| callback.
    using CallbackTraits = internal::CallbackTraits<FinallyCb>;
    using ReturnedPromiseResolveT = typename CallbackTraits::ResolveType;
    using ReturnedPromiseRejectT = typename CallbackTraits::RejectType;

    using CallbackArgT = typename CallbackTraits::ArgType;
    static_assert(std::is_void<CallbackArgT>::value,
                  "|finally_callback| callback must have no arguments");

    return Promise<ReturnedPromiseResolveT, ReturnedPromiseRejectT>(
        MakeRefCounted<internal::AbstractPromise>(
            std::move(task_runner), from_here,
            std::make_unique<internal::AbstractPromise::AdjacencyList>(
                abstract_promise_),
            RejectPolicy::kMustCatchRejection,
            internal::AbstractPromise::ConstructWith<
                internal::DependentList::ConstructUnresolved,
                internal::FinallyExecutor<
                    OnceCallback<typename CallbackTraits::ReturnType()>,
                    Resolved<ReturnedPromiseResolveT>,
                    Rejected<ReturnedPromiseRejectT>>>(),
            std::forward<FinallyCb>(finally_callback)));
  }

  template <typename FinallyCb>
  auto FinallyOn(const TaskTraits& traits,
                 const Location& from_here,
                 FinallyCb&& finally_callback) noexcept {
    return FinallyOn(CreateTaskRunnerWithTraits(traits), from_here,
                     std::move(finally_callback));
  }

  template <typename FinallyCb>
  auto FinallyOnCurrent(const Location& from_here,
                        FinallyCb&& finally_callback) noexcept {
    return FinallyOn(SequencedTaskRunnerHandle::Get(), from_here,
                     std::move(finally_callback));
  }

  template <typename... Args>
  NOINLINE static Promise<ResolveType, RejectType> CreateResolved(
      const Location& from_here,
      Args&&... args) noexcept {
    scoped_refptr<internal::AbstractPromise> promise(
        MakeRefCounted<internal::AbstractPromise>(
            nullptr, from_here, nullptr, RejectPolicy::kMustCatchRejection,
            internal::AbstractPromise::ConstructWith<
                internal::DependentList::ConstructResolved,
                internal::NoOpPromiseExecutor>(),
            /* can_resolve */ true,
            /* can_reject */ false));
    promise->emplace(Resolved<ResolveType>{std::forward<Args>(args)...});
    return Promise<ResolveType, RejectType>(std::move(promise));
  }

  template <typename... Args>
  NOINLINE static Promise<ResolveType, RejectType> CreateRejected(
      const Location& from_here,
      Args&&... args) noexcept {
    scoped_refptr<internal::AbstractPromise> promise(
        MakeRefCounted<internal::AbstractPromise>(
            nullptr, from_here, nullptr, RejectPolicy::kMustCatchRejection,
            internal::AbstractPromise::ConstructWith<
                internal::DependentList::ConstructRejected,
                internal::NoOpPromiseExecutor>(),
            /* can_resolve */ false,
            /* can_reject */ true));
    promise->emplace(Rejected<RejectType>{std::forward<Args>(args)...});
    return Promise<ResolveType, RejectType>(std::move(promise));
  }

  using ResolveT = ResolveType;
  using RejectT = RejectType;

 private:
  template <typename A, typename B>
  friend class Promise;

  template <typename A, typename B>
  friend class PromiseResult;

  template <typename RejectStorage, typename ResultStorage>
  friend struct internal::EmplaceHelper;

  template <typename A, typename B>
  friend class ManualPromiseResolver;

  scoped_refptr<internal::AbstractPromise> abstract_promise_;
};

// Used for manually resolving and rejecting a Promise. This is for
// compatibility with old code and will eventually be removed.
template <typename ResolveType, typename RejectType = NoReject>
class ManualPromiseResolver {
 public:
  using ResolveHelper = std::conditional_t<
      std::is_void<ResolveType>::value,
      internal::PromiseCallbackHelper<void>,
      internal::PromiseCallbackHelper<ResolveType, ResolveType>>;

  using RejectHelper = std::conditional_t<
      std::is_void<RejectType>::value,
      internal::PromiseCallbackHelper<void>,
      internal::PromiseCallbackHelper<RejectType, RejectType>>;

  ManualPromiseResolver(
      const Location& from_here,
      RejectPolicy reject_policy = RejectPolicy::kMustCatchRejection)
      : promise_(SequencedTaskRunnerHandle::Get(), from_here, reject_policy) {}

  ~ManualPromiseResolver() {
    // If the promise wasn't resolved or rejected, then cancel it to make sure
    // we don't leak memory.
    if (!promise_.abstract_promise_->IsSettled())
      promise_.abstract_promise_->OnCanceled();
  }

  template <typename... Args>
  void Resolve(Args&&... arg) noexcept {
    DCHECK(!promise_.abstract_promise_->IsResolved());
    DCHECK(!promise_.abstract_promise_->IsRejected());
    static_assert(!std::is_same<NoResolve, ResolveType>::value,
                  "Can't resolve a NoResolve promise.");
    promise_.abstract_promise_->emplace(
        Resolved<ResolveType>{std::forward<Args>(arg)...});
    promise_.abstract_promise_->OnResolved();
  }

  template <typename... Args>
  void Reject(Args&&... arg) noexcept {
    DCHECK(!promise_.abstract_promise_->IsResolved());
    DCHECK(!promise_.abstract_promise_->IsRejected());
    static_assert(!std::is_same<NoReject, RejectType>::value,
                  "Can't reject a NoReject promise.");
    promise_.abstract_promise_->emplace(
        Rejected<RejectType>{std::forward<Args>(arg)...});
    promise_.abstract_promise_->OnRejected();
  }

  typename ResolveHelper::Callback GetResolveCallback() {
    return ResolveHelper::GetResolveCallback(promise_.abstract_promise_);
  }

  typename ResolveHelper::RepeatingCallback GetRepeatingResolveCallback() {
    return ResolveHelper::GetRepeatingResolveCallback(
        promise_.abstract_promise_);
  }

  typename RejectHelper::Callback GetRejectCallback() {
    static_assert(!std::is_same<NoReject, RejectType>::value,
                  "Can't reject a NoReject promise.");
    return RejectHelper::GetRejectCallback(promise_.abstract_promise_);
  }

  typename RejectHelper::RepeatingCallback GetRepeatingRejectCallback() {
    static_assert(!std::is_same<NoReject, RejectType>::value,
                  "Can't reject a NoReject promise.");
    return RejectHelper::GetRepeatingRejectCallback(promise_.abstract_promise_);
  }

  Promise<ResolveType, RejectType>& promise() { return promise_; }

 private:
  Promise<ResolveType, RejectType> promise_;
};

}  // namespace base

#endif  // BASE_TASK_PROMISE_PROMISE_H_
