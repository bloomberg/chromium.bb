// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_TASK_UTIL_H_
#define CHROME_BROWSER_GOOGLE_APIS_TASK_UTIL_H_

#include "base/bind.h"
#include "base/message_loop_proxy.h"

namespace gdata {

// Runs task on the thread to which |relay_proxy| belongs.
void RunTaskOnThread(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                     const base::Closure& task);

// Runs task on UI thread.
void RunTaskOnUIThread(const base::Closure& task);

namespace internal {

// RelayCallback relays arguments for callback running on the given thread.
template<typename CallbackType>
struct RelayCallback;

// RelayCallback for callback with one argument.
template<typename T1>
struct RelayCallback<base::Callback<void(T1)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1)>& callback,
                  T1 arg1) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy, base::Bind(callback, arg1));
  }
};

// RelayCallback for callback with two arguments.
template<typename T1, typename T2>
struct RelayCallback<base::Callback<void(T1, T2)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1, T2)>& callback,
                  T1 arg1,
                  T2 arg2) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy, base::Bind(callback, arg1, arg2));
  }
};

// RelayCallback for callback with two arguments, the last one is scoped_ptr.
template<typename T1, typename T2>
struct RelayCallback<base::Callback<void(T1, scoped_ptr<T2>)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1, scoped_ptr<T2>)>& callback,
                  T1 arg1,
                  scoped_ptr<T2> arg2) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy,
                    base::Bind(callback, arg1, base::Passed(&arg2)));
  }
};

// RelayCallback for callback with three arguments.
template<typename T1, typename T2, typename T3>
struct RelayCallback<base::Callback<void(T1, T2, T3)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1, T2, T3)>& callback,
                  T1 arg1,
                  T2 arg2,
                  T3 arg3) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy, base::Bind(callback, arg1, arg2, arg3));
  }
};

// RelayCallback for callback with three arguments, the last one is scoped_ptr.
template<typename T1, typename T2, typename T3>
struct RelayCallback<base::Callback<void(T1, T2, scoped_ptr<T3>)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1, T2, scoped_ptr<T3>)>& callback,
                  T1 arg1,
                  T2 arg2,
                  scoped_ptr<T3> arg3) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy,
                    base::Bind(callback, arg1, arg2, base::Passed(&arg3)));
  }
};

// RelayCallback for callback with four arguments.
template<typename T1, typename T2, typename T3, typename T4>
struct RelayCallback<base::Callback<void(T1, T2, T3, T4)> > {
  static void Run(scoped_refptr<base::MessageLoopProxy> relay_proxy,
                  const base::Callback<void(T1, T2, T3, T4)>& callback,
                  T1 arg1,
                  T2 arg2,
                  T3 arg3,
                  T4 arg4) {
    if (callback.is_null())
      return;
    RunTaskOnThread(relay_proxy, base::Bind(callback, arg1, arg2, arg3, arg4));
  }
};

}  // namespace internal

// Returns callback which runs the given |callback| on the current thread.
template<typename CallbackType>
CallbackType CreateRelayCallback(const CallbackType& callback) {
  return base::Bind(&internal::RelayCallback<CallbackType>::Run,
                    base::MessageLoopProxy::current(),
                    callback);
}

}  // namespace gdata

#endif  // CHROME_BROWSER_GOOGLE_APIS_TASK_UTIL_H_
