// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MOJO_ASYNC_HANDLE_WAITER_H_
#define IPC_MOJO_ASYNC_HANDLE_WAITER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "ipc/ipc_export.h"
#include "mojo/public/c/system/types.h"

namespace IPC {
namespace internal {

// |AsyncHandleWaiter| waits on a mojo handle asynchronously and
// invokes the given |callback| through |runner| when it is signaled.
//  * To start waiting, the client must call |AsyncHandleWaiter::Wait()|.
//    The client can call |Wait()| again once it is signaled and
//    the |callback| is invoked.
//  * To cancel waiting, delete the instance.
//
// |AsyncHandleWaiter| must be created, used and deleted only from the IO
// |thread.
class IPC_MOJO_EXPORT AsyncHandleWaiter {
 public:
  class Context;

  explicit AsyncHandleWaiter(base::Callback<void(MojoResult)> callback);
  ~AsyncHandleWaiter();

  MojoResult Wait(MojoHandle handle, MojoHandleSignals signals);

  base::MessageLoopForIO::IOObserver* GetIOObserverForTest();
  base::Callback<void(MojoResult)> GetWaitCallbackForTest();

 private:
  void InvokeCallback(MojoResult result);

  scoped_refptr<Context> context_;
  base::Callback<void(MojoResult)> callback_;
  base::WeakPtrFactory<AsyncHandleWaiter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AsyncHandleWaiter);
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_MOJO_ASYNC_HANDLE_WAITER_H_
