// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_HELPER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_HELPER_H_

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"

namespace sync_file_system {
namespace drive_backend {

namespace internal {

template <typename T>
typename base::enable_if<
    base::internal::IsMoveOnlyType<T>::value,
    base::internal::PassedWrapper<T> >::type
RebindForward(T& t) {
  return base::Passed(&t);
}

template <typename T>
typename base::enable_if<
    !base::internal::IsMoveOnlyType<T>::value,
    T&>::type
RebindForward(T& t) {
  return t;
}

template <typename T>
void RelayToTaskRunner1(base::SequencedTaskRunner* task_runner,
                        const tracked_objects::Location& from_here,
                        const base::Callback<void(T)>& callback,
                        T arg) {
  task_runner->PostTask(
      from_here, base::Bind(callback, RebindForward(arg)));
}

template <typename T1, typename T2>
void RelayToTaskRunner2(base::SequencedTaskRunner* task_runner,
                        const tracked_objects::Location& from_here,
                        const base::Callback<void(T1, T2)>& callback,
                        T1 arg1,
                        T2 arg2) {
  task_runner->PostTask(
      from_here, base::Bind(callback,
                            RebindForward(arg1),
                            RebindForward(arg2)));
}

}  // namespace internal

template <typename T>
base::Callback<void(T)> CreateRelayedCallback(
    const base::Callback<void(T)>& callback) {
  return base::Bind(&internal::RelayToTaskRunner1<T>,
                    base::MessageLoopProxy::current(),
                    FROM_HERE,
                    callback);
}

template <typename T1, typename T2>
base::Callback<void(T1, T2)> CreateRelayedCallback(
    const base::Callback<void(T1, T2)>& callback) {
  return base::Bind(&internal::RelayToTaskRunner2<T1, T2>,
                    base::MessageLoopProxy::current(),
                    FROM_HERE,
                    callback);
}

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_CALLBACK_HELPER_H_
