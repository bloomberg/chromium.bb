// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_PROMISE_HELPERS_H_
#define BASE_TASK_PROMISE_HELPERS_H_

#include <type_traits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/promise/abstract_promise.h"
#include "base/task/promise/promise_result.h"

namespace base {
namespace internal {

// PromiseCallbackTraits computes the resolve and reject types of a Promise
// from the return type of a resolve or reject callback.
//
// Usage example:
//   using Traits = PromiseCallbackTraits<int>;
//
//   Traits::
//       ResolveType is int
//       RejectType is NoReject
//       could_resolve is true
//       could_reject is false
template <typename T>
struct PromiseCallbackTraits {
  using ResolveType = T;
  using RejectType = NoReject;
  static constexpr bool could_resolve = true;
  static constexpr bool could_reject = false;
};

template <typename T>
struct PromiseCallbackTraits<Resolved<T>> {
  using ResolveType = T;
  using RejectType = NoReject;
  static constexpr bool could_resolve = true;
  static constexpr bool could_reject = false;
};

template <typename T>
struct PromiseCallbackTraits<Rejected<T>> {
  using ResolveType = NoResolve;
  using RejectType = T;
  static constexpr bool could_resolve = false;
  static constexpr bool could_reject = true;
};

template <typename Reject>
struct PromiseCallbackTraits<Promise<NoResolve, Reject>> {
  using ResolveType = NoResolve;
  using RejectType = Reject;
  static constexpr bool could_resolve = false;
  static constexpr bool could_reject = true;
};

template <typename Resolve>
struct PromiseCallbackTraits<Promise<Resolve, NoReject>> {
  using ResolveType = Resolve;
  using RejectType = NoReject;
  static constexpr bool could_resolve = true;
  static constexpr bool could_reject = false;
};

template <typename Resolve, typename Reject>
struct PromiseCallbackTraits<Promise<Resolve, Reject>> {
  using ResolveType = Resolve;
  using RejectType = Reject;
  static constexpr bool could_resolve = true;
  static constexpr bool could_reject = true;
};

template <typename Reject>
struct PromiseCallbackTraits<PromiseResult<NoResolve, Reject>> {
  using ResolveType = NoResolve;
  using RejectType = Reject;
  static constexpr bool could_resolve = false;
  static constexpr bool could_reject = true;
};

template <typename Resolve>
struct PromiseCallbackTraits<PromiseResult<Resolve, NoReject>> {
  using ResolveType = Resolve;
  using RejectType = NoReject;
  static constexpr bool could_resolve = true;
  static constexpr bool could_reject = false;
};

template <typename Resolve, typename Reject>
struct PromiseCallbackTraits<PromiseResult<Resolve, Reject>> {
  using ResolveType = Resolve;
  using RejectType = Reject;
  static constexpr bool could_resolve = true;
  static constexpr bool could_reject = true;
};

template <typename T>
struct IsScopedRefPtr {
  static constexpr bool value = false;
};

template <typename T>
struct IsScopedRefPtr<scoped_refptr<T>> {
  static constexpr bool value = true;
};

// UseMoveSemantics determines whether move semantics should be used to
// pass |T| as a function parameter.
//
// Usage example:
//
//   UseMoveSemantics<std::unique_ptr<int>>::value;  // is true
//   UseMoveSemantics<int>::value; // is false
//   UseMoveSemantics<scoped_refptr<Dummy>>::value; // is false
//
// Will give false positives for some copyable types, but that should be
// harmless.
template <typename T,
          bool use_move = !std::is_reference<T>::value &&
                          !std::is_pointer<T>::value &&
                          !std::is_fundamental<std::decay_t<T>>::value &&
                          !IsScopedRefPtr<T>::value>
struct UseMoveSemantics : public std::integral_constant<bool, use_move> {
  static_assert(!std::is_rvalue_reference<T>::value,
                "Promise<T&&> not supported");

  static constexpr AbstractPromise::Executor::ArgumentPassingType
      argument_passing_type =
          use_move ? AbstractPromise::Executor::ArgumentPassingType::kMove
                   : AbstractPromise::Executor::ArgumentPassingType::kNormal;
};

// CallbackTraits extracts properties relevant to Promises from a callback.
//
// Usage example:
//
//   using Traits = CallbackTraits<
//       base::OnceCallback<PromiseResult<int, std::string>(float)>;
//
//   Traits::
//     ResolveType is int
//     RejectType is std::string
//     ArgType is float
//     ReturnType is PromiseResult<int, std::string>
//     SignatureType is PromiseResult<int, std::string>(float)
//     argument_passing_type is kNormal
template <typename T>
struct CallbackTraits;

template <typename T>
struct CallbackTraits<base::OnceCallback<T()>> {
  using ResolveType = typename internal::PromiseCallbackTraits<T>::ResolveType;
  using RejectType = typename internal::PromiseCallbackTraits<T>::RejectType;
  using ArgType = void;
  using ReturnType = T;
  using SignatureType = T();
  static constexpr AbstractPromise::Executor::ArgumentPassingType
      argument_passing_type =
          AbstractPromise::Executor::ArgumentPassingType::kNormal;
};

template <typename T>
struct CallbackTraits<base::RepeatingCallback<T()>> {
  using ResolveType = typename internal::PromiseCallbackTraits<T>::ResolveType;
  using RejectType = typename internal::PromiseCallbackTraits<T>::RejectType;
  using ArgType = void;
  using ReturnType = T;
  using SignatureType = T();
  static constexpr AbstractPromise::Executor::ArgumentPassingType
      argument_passing_type =
          AbstractPromise::Executor::ArgumentPassingType::kNormal;
};

template <typename T, typename Arg>
struct CallbackTraits<base::OnceCallback<T(Arg)>> {
  using ResolveType = typename internal::PromiseCallbackTraits<T>::ResolveType;
  using RejectType = typename internal::PromiseCallbackTraits<T>::RejectType;
  using ArgType = Arg;
  using ReturnType = T;
  using SignatureType = T(Arg);
  static constexpr AbstractPromise::Executor::ArgumentPassingType
      argument_passing_type = UseMoveSemantics<Arg>::argument_passing_type;
};

template <typename T, typename Arg>
struct CallbackTraits<base::RepeatingCallback<T(Arg)>> {
  using ResolveType = typename internal::PromiseCallbackTraits<T>::ResolveType;
  using RejectType = typename internal::PromiseCallbackTraits<T>::RejectType;
  using ArgType = Arg;
  using ReturnType = T;
  using SignatureType = T(Arg);
  static constexpr AbstractPromise::Executor::ArgumentPassingType
      argument_passing_type = UseMoveSemantics<Arg>::argument_passing_type;
};

// Helper for combining the resolve types of two promises.
template <typename A, typename B>
struct ResolveCombinerHelper {
  using Type = A;
  static constexpr bool valid = false;
};

template <typename A>
struct ResolveCombinerHelper<A, A> {
  using Type = A;
  static constexpr bool valid = true;
};

template <typename B>
struct ResolveCombinerHelper<NoResolve, B> {
  using Type = B;
  static constexpr bool valid = true;
};

template <typename A>
struct ResolveCombinerHelper<A, NoResolve> {
  using Type = A;
  static constexpr bool valid = true;
};

template <>
struct ResolveCombinerHelper<NoResolve, NoResolve> {
  using Type = NoResolve;
  static constexpr bool valid = true;
};

// Helper for combining the reject types of two promises.
template <typename A, typename B>
struct RejectCombinerHelper {
  using Type = A;
  static constexpr bool valid = false;
};

template <typename A>
struct RejectCombinerHelper<A, A> {
  using Type = A;
  static constexpr bool valid = true;
};

template <typename B>
struct RejectCombinerHelper<NoReject, B> {
  using Type = B;
  static constexpr bool valid = true;
};

template <typename A>
struct RejectCombinerHelper<A, NoReject> {
  using Type = A;
  static constexpr bool valid = true;
};

template <>
struct RejectCombinerHelper<NoReject, NoReject> {
  using Type = NoReject;
  static constexpr bool valid = true;
};

// Helper that computes and validates the return type for combining promises.
// Essentially the promise types have to match unless there's NoResolve or
// or NoReject in which case they can be combined.
template <typename ThenReturnResolveT,
          typename ThenReturnRejectT,
          typename CatchReturnResolveT,
          typename CatchReturnRejectT>
struct PromiseCombiner {
  using ResolveHelper =
      ResolveCombinerHelper<ThenReturnResolveT, CatchReturnResolveT>;
  using RejectHelper =
      RejectCombinerHelper<ThenReturnRejectT, CatchReturnRejectT>;
  using ResolveType = typename ResolveHelper::Type;
  using RejectType = typename RejectHelper::Type;
  static constexpr bool valid = ResolveHelper::valid && RejectHelper::valid;
};

// TODO(alexclarke): Specialize |CallbackTraits| for callbacks with more than
// one argument to support Promises::All.

template <typename RejectStorage>
struct EmplaceInnerHelper {
  template <typename Resolve, typename Reject>
  static void Emplace(AbstractPromise* promise,
                      PromiseResult<Resolve, Reject> result) {
    promise->emplace(std::move(result.value()));
  }
};

// TODO(alexclarke): Specialize |EmplaceInnerHelper| where RejectStorage is
// base::Variant to support Promises::All.

template <typename ResolveStorage, typename RejectStorage>
struct EmplaceHelper {
  template <typename Resolve, typename Reject>
  static void Emplace(AbstractPromise* promise,
                      PromiseResult<Resolve, Reject>&& result) {
    static_assert(std::is_same<typename ResolveStorage::Type, Resolve>::value ||
                      std::is_same<NoResolve, Resolve>::value,
                  "Resolve should match ResolveStorage");
    static_assert(std::is_same<typename RejectStorage::Type, Reject>::value ||
                      std::is_same<NoReject, Reject>::value,
                  "Reject should match RejectStorage");
    EmplaceInnerHelper<RejectStorage>::Emplace(promise, std::move(result));
  }

  template <typename Resolve, typename Reject>
  static void Emplace(AbstractPromise* promise,
                      Promise<Resolve, Reject>&& result) {
    static_assert(std::is_same<typename ResolveStorage::Type, Resolve>::value ||
                      std::is_same<NoResolve, Resolve>::value,
                  "Resolve should match ResolveStorage");
    static_assert(std::is_same<typename RejectStorage::Type, Reject>::value ||
                      std::is_same<NoReject, Reject>::value,
                  "Reject should match RejectStorage");
    promise->emplace(std::move(result.abstract_promise_));
  }

  template <typename Result>
  static void Emplace(AbstractPromise* promise, Result&& result) {
    static_assert(std::is_same<typename ResolveStorage::Type, Result>::value,
                  "Result should match ResolveStorage");
    promise->emplace(Resolved<Result>{std::forward<Result>(result)});
  }

  template <typename Resolve>
  static void Emplace(AbstractPromise* promise, Resolved<Resolve>&& resolved) {
    static_assert(std::is_same<typename ResolveStorage::Type, Resolve>::value,
                  "Resolve should match ResolveStorage");
    promise->emplace(std::move(resolved));
  }

  template <typename Reject>
  static void Emplace(AbstractPromise* promise, Rejected<Reject>&& rejected) {
    static_assert(std::is_same<typename RejectStorage::Type, Reject>::value,
                  "Reject should match RejectStorage");
    promise->emplace(std::move(rejected));
  }
};

// Helper that decides whether or not to std::move arguments for a callback
// based on the type the callback specifies (i.e. we don't need to move if the
// callback requests a reference).
template <typename CbArg, typename ArgStorageType>
class ArgMoveSemanticsHelper {
 public:
  static CbArg Get(AbstractPromise* arg) {
    return GetImpl(arg, UseMoveSemantics<CbArg>());
  }

 private:
  static CbArg GetImpl(AbstractPromise* arg, std::true_type should_move) {
    return arg->TakeInnerValue<ArgStorageType>();
  }

  static CbArg GetImpl(AbstractPromise* arg, std::false_type should_move) {
    return unique_any_cast<ArgStorageType>(&arg->value())->value;
  }
};

// Helper for running a promise callback and storing the result if any.
//
// Callback = signature of the callback to execute,
// ArgStorageType = type of the callback parameter (pr void if none)
// ResolveStorage = type to use for resolve, usually Resolved<T>.
// RejectStorage = type to use for reject, usually Rejected<T>.
// TODO(alexclarke): Add support for Rejected<Variant<...>>.
template <typename Callback,
          typename ArgStorageType,
          typename ResolveStorage,
          typename RejectStorage>
struct RunHelper;

// Run helper for callbacks with a single argument.
template <typename CbResult,
          typename CbArg,
          typename ArgStorageType,
          typename ResolveStorage,
          typename RejectStorage>
struct RunHelper<OnceCallback<CbResult(CbArg)>,
                 ArgStorageType,
                 ResolveStorage,
                 RejectStorage> {
  using Callback = OnceCallback<CbResult(CbArg)>;

  static void Run(Callback executor,
                  AbstractPromise* arg,
                  AbstractPromise* result) {
    EmplaceHelper<ResolveStorage, RejectStorage>::Emplace(
        result, std::move(executor).Run(
                    ArgMoveSemanticsHelper<CbArg, ArgStorageType>::Get(arg)));
  }
};

// Run helper for callbacks with a single argument and void return value.
template <typename CbArg,
          typename ArgStorageType,
          typename ResolveStorage,
          typename RejectStorage>
struct RunHelper<OnceCallback<void(CbArg)>,
                 ArgStorageType,
                 ResolveStorage,
                 RejectStorage> {
  using Callback = OnceCallback<void(CbArg)>;

  static void Run(Callback executor,
                  AbstractPromise* arg,
                  AbstractPromise* result) {
    static_assert(std::is_void<typename ResolveStorage::Type>::value, "");
    std::move(executor).Run(
        ArgMoveSemanticsHelper<CbArg, ArgStorageType>::Get(arg));
    result->emplace(Resolved<void>());
  }
};

// Run helper for callbacks with no arguments.
template <typename CbResult,
          typename ArgStorageType,
          typename ResolveStorage,
          typename RejectStorage>
struct RunHelper<OnceCallback<CbResult()>,
                 ArgStorageType,
                 ResolveStorage,
                 RejectStorage> {
  using Callback = OnceCallback<CbResult()>;

  static void Run(Callback executor,
                  AbstractPromise* arg,
                  AbstractPromise* result) {
    EmplaceHelper<ResolveStorage, RejectStorage>::Emplace(
        result, std::move(executor).Run());
  }
};

// Run helper for callbacks with no arguments and void return type.
template <typename ArgStorageType,
          typename ResolveStorage,
          typename RejectStorage>
struct RunHelper<OnceCallback<void()>,
                 ArgStorageType,
                 ResolveStorage,
                 RejectStorage> {
  static void Run(OnceCallback<void()> executor,
                  AbstractPromise* arg,
                  AbstractPromise* result) {
    static_assert(std::is_void<typename ResolveStorage::Type>::value, "");
    std::move(executor).Run();
    result->emplace(Resolved<void>());
  }
};

// TODO(alexclarke): Specialize RunHelper for callbacks unpacked from a tuple.

// Used by ManualPromiseResolver<> to generate callbacks.
template <typename T, typename... Args>
class PromiseCallbackHelper {
 public:
  using Callback = base::OnceCallback<void(Args&&...)>;
  using RepeatingCallback = base::RepeatingCallback<void(Args&&...)>;

  static Callback GetResolveCallback(scoped_refptr<AbstractPromise>& promise) {
    return base::BindOnce(
        [](scoped_refptr<AbstractPromise> promise, Args&&... args) {
          promise->emplace(Resolved<T>{std::forward<Args>(args)...});
          promise->OnResolved();
        },
        promise);
  }

  static RepeatingCallback GetRepeatingResolveCallback(
      scoped_refptr<AbstractPromise>& promise) {
    return base::BindRepeating(
        [](scoped_refptr<AbstractPromise> promise, Args&&... args) {
          promise->emplace(Resolved<T>{std::forward<Args>(args)...});
          promise->OnResolved();
        },
        promise);
  }

  static Callback GetRejectCallback(scoped_refptr<AbstractPromise>& promise) {
    return base::BindOnce(
        [](scoped_refptr<AbstractPromise> promise, Args&&... args) {
          promise->emplace(Rejected<T>{std::forward<Args>(args)...});
          promise->OnRejected();
        },
        promise);
  }

  static RepeatingCallback GetRepeatingRejectCallback(
      scoped_refptr<AbstractPromise>& promise) {
    return base::BindRepeating(
        [](scoped_refptr<AbstractPromise> promise, Args&&... args) {
          promise->emplace(Rejected<T>{std::forward<Args>(args)...});
          promise->OnRejected();
        },
        promise);
  }
};

// Validates that the argument type |CallbackArgType| of a resolve or
// reject callback is compatible with the resolve or reject type
// |PromiseType| of this Promise.
template <typename PromiseType, typename CallbackArgType>
struct IsValidPromiseArg {
  static constexpr bool value =
      std::is_same<PromiseType, std::decay_t<CallbackArgType>>::value;
};

template <typename PromiseType, typename CallbackArgType>
struct IsValidPromiseArg<PromiseType&, CallbackArgType> {
  static constexpr bool value =
      std::is_same<PromiseType&, CallbackArgType>::value;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_PROMISE_HELPERS_H_
