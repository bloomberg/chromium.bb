// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_TASK_UTIL_H_
#define CHROME_BROWSER_GOOGLE_APIS_TASK_UTIL_H_

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"

namespace google_apis {

// Runs task on the thread to which |relay_proxy| belongs.
void RunTaskOnThread(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                     const base::Closure& task);

namespace internal {

// Implementation of the composed callback, whose signature is |Sig|.
template<typename Sig> struct ComposedCallback;

// ComposedCallback with no argument.
template<>
struct ComposedCallback<void()> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Closure& callback) {
    runner.Run(callback);
  }
};

// ComposedCallback with one argument.
template<typename T1>
struct ComposedCallback<void(T1)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1)>& callback,
      T1 arg1) {
    runner.Run(base::Bind(callback, arg1));
  }
};

// ComposedCallback with two arguments.
template<typename T1, typename T2>
struct ComposedCallback<void(T1, T2)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1, T2)>& callback,
      T1 arg1, T2 arg2) {
    runner.Run(base::Bind(callback, arg1, arg2));
  }
};

// ComposedCallback with two arguments, and the last one is scoped_ptr.
template<typename T1, typename T2, typename D2>
struct ComposedCallback<void(T1, scoped_ptr<T2, D2>)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1, scoped_ptr<T2, D2>)>& callback,
      T1 arg1, scoped_ptr<T2, D2> arg2) {
    runner.Run(base::Bind(callback, arg1, base::Passed(&arg2)));
  }
};

// ComposedCallback with three arguments.
template<typename T1, typename T2, typename T3>
struct ComposedCallback<void(T1, T2, T3)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1, T2, T3)>& callback,
      T1 arg1, T2 arg2, T3 arg3) {
    runner.Run(base::Bind(callback, arg1, arg2, arg3));
  }
};

// ComposedCallback with three arguments, and the last one is scoped_ptr.
template<typename T1, typename T2, typename T3, typename D3>
struct ComposedCallback<void(T1, T2, scoped_ptr<T3, D3>)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1, T2, scoped_ptr<T3, D3>)>& callback,
      T1 arg1, T2 arg2, scoped_ptr<T3, D3> arg3) {
    runner.Run(base::Bind(callback, arg1, arg2, base::Passed(&arg3)));
  }
};

// ComposedCallback with four arguments.
template<typename T1, typename T2, typename T3, typename T4>
struct ComposedCallback<void(T1, T2, T3, T4)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1, T2, T3, T4)>& callback,
      T1 arg1, T2 arg2, T3 arg3, T4 arg4) {
    runner.Run(base::Bind(callback, arg1, arg2, arg3, arg4));
  }
};

// ComposedCallback with four arguments, and the second one is scoped_ptr.
template<typename T1, typename T2, typename D2, typename T3, typename T4>
struct ComposedCallback<void(T1, scoped_ptr<T2, D2>, T3, T4)> {
  static void Run(
      const base::Callback<void(const base::Closure&)>& runner,
      const base::Callback<void(T1, scoped_ptr<T2, D2>, T3, T4)>& callback,
      T1 arg1, scoped_ptr<T2, D2> arg2, T3 arg3, T4 arg4) {
    runner.Run(base::Bind(callback, arg1, base::Passed(&arg2), arg3, arg4));
  }
};

// GetDefaultValue returns a value constructed by the default constructor.
template<typename T> struct DefaultValueCreator {
  static T GetDefaultValue() { return T(); }
};
template<typename T> struct DefaultValueCreator<const T&> {
  static T GetDefaultValue() { return T(); }
};

// Helper of CreateErrorRunCallback implementation.
// Provides:
// - ResultType; the type of the Callback which should be returned by
//     CreateErrorRunCallback.
// - Run(): a static function which takes the original |callback| and |error|,
//     and runs the |callback|.Run() with the error code and default values
//     for remaining arguments.
template<typename CallbackType> struct CreateErrorRunCallbackHelper;

// CreateErrorRunCallback with two arguments.
template<typename ErrorType, typename P1>
struct CreateErrorRunCallbackHelper<void(ErrorType, P1)> {
  typedef base::Callback<void(ErrorType)> ResultType;
  static void Run(
      const base::Callback<void(ErrorType, P1)>& callback, ErrorType error) {
    callback.Run(error, DefaultValueCreator<P1>::GetDefaultValue());
  }
};

}  // namespace internal

// Returns callback that takes arguments (arg1, arg2, ...), create a closure
// by binding them to |callback|, and runs |runner| with the closure.
// I.e. the returned callback works as follows:
//   runner.Run(Bind(callback, arg1, arg2, ...))
template<typename CallbackType>
CallbackType CreateComposedCallback(
    const base::Callback<void(const base::Closure&)>& runner,
    const CallbackType& callback) {
  DCHECK(!runner.is_null());
  DCHECK(!callback.is_null());
  return base::Bind(
      &internal::ComposedCallback<typename CallbackType::RunType>::Run,
      runner, callback);
}

// Returns callback which runs the given |callback| on the current thread.
template<typename CallbackType>
CallbackType CreateRelayCallback(const CallbackType& callback) {
  return CreateComposedCallback(
      base::Bind(&RunTaskOnThread, base::MessageLoopProxy::current()),
      callback);
}

// Returns a callback with the tail parameter bound to its default value.
// In other words, returned_callback.Run(error) runs callback.Run(error, T()).
template<typename CallbackType>
typename internal::CreateErrorRunCallbackHelper<CallbackType>::ResultType
CreateErrorRunCallback(const base::Callback<CallbackType>& callback) {
  return base::Bind(
      &internal::CreateErrorRunCallbackHelper<CallbackType>::Run, callback);
}

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_TASK_UTIL_H_
