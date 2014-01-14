// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/environment/async_waiter_impl.h"

#include "base/bind.h"
#include "mojo/common/handle_watcher.h"

namespace mojo {
namespace internal {
namespace {

void OnHandleReady(common::HandleWatcher* watcher,
                   MojoAsyncWaitCallback callback,
                   uintptr_t user_data,
                   MojoResult result) {
  delete watcher;
  callback(user_data, result);
}

MojoAsyncWaitID AsyncWait(MojoAsyncWaiter* waiter,
                          MojoHandle handle,
                          MojoWaitFlags flags,
                          MojoDeadline deadline,
                          MojoAsyncWaitCallback callback,
                          uintptr_t user_data) {
  // This instance will be deleted when done or cancelled.
  common::HandleWatcher* watcher = new common::HandleWatcher();
  watcher->Start(Handle(handle), flags, deadline,
                 base::Bind(&OnHandleReady, watcher, callback, user_data));
  return reinterpret_cast<MojoAsyncWaitID>(watcher);
}

void CancelWait(MojoAsyncWaiter* waiter, MojoAsyncWaitID wait_id) {
  delete reinterpret_cast<common::HandleWatcher*>(wait_id);
}

MojoAsyncWaiter s_default_async_waiter = {
  AsyncWait,
  CancelWait
};

}  // namespace

MojoAsyncWaiter* GetDefaultAsyncWaiterImpl() {
  return &s_default_async_waiter;
}

}  // namespace internal
}  // namespace mojo
