// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/async_handle_waiter.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder.h"

namespace IPC {
namespace internal {

// The thread-safe part of |AsyncHandleWaiter|.
// As |AsyncWait()| invokes the given callback from an arbitrary thread,
// |HandleIsReady()| and the bound |this| have to be thread-safe.
class AsyncHandleWaiter::Context
    : public base::RefCountedThreadSafe<AsyncHandleWaiter::Context> {
 public:
  Context(scoped_refptr<base::TaskRunner> runner,
          base::WeakPtr<AsyncHandleWaiter> waiter)
      : waiter_(waiter), runner_(runner), last_result_(MOJO_RESULT_INTERNAL) {}

  void HandleIsReady(MojoResult result) {
    last_result_ = result;
    runner_->PostTask(FROM_HERE,
                      base::Bind(&Context::InvokeWaiterCallback, this));
  }

 private:
  friend class base::RefCountedThreadSafe<Context>;
  virtual ~Context() {}

  // Called from |runner_| thus safe to touch |waiter_|.
  void InvokeWaiterCallback() {
    MojoResult result = last_result_;
    last_result_ = MOJO_RESULT_INTERNAL;
    if (waiter_)
      waiter_->InvokeCallback(result);
  }

  // A weak reference. Note that |waiter_| must be touched only from the
  // original thread.
  const base::WeakPtr<AsyncHandleWaiter> waiter_;
  const scoped_refptr<base::TaskRunner> runner_;
  MojoResult last_result_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

AsyncHandleWaiter::AsyncHandleWaiter(base::Callback<void(MojoResult)> callback)
    : weak_factory_(this),
      context_(new Context(base::MessageLoop::current()->task_runner(),
                           weak_factory_.GetWeakPtr())),
      callback_(callback) {
}

AsyncHandleWaiter::~AsyncHandleWaiter() {
}

MojoResult AsyncHandleWaiter::Wait(MojoHandle handle,
                                   MojoHandleSignals signals) {
  return mojo::embedder::AsyncWait(
      handle, signals, base::Bind(&Context::HandleIsReady, context_));
}

void AsyncHandleWaiter::InvokeCallback(MojoResult result) {
  callback_.Run(result);
}

}  // namespace internal
}  // namespace IPC
