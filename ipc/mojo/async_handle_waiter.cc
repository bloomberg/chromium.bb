// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/async_handle_waiter.h"

#include "base/atomic_ref_count.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "mojo/edk/embedder/embedder.h"

namespace IPC {
namespace internal {

class AsyncHandleWaiterContextTraits {
 public:
  static void Destruct(const AsyncHandleWaiter::Context* context);
};

// The thread-safe part of |AsyncHandleWaiter|.
// As |AsyncWait()| invokes the given callback from an arbitrary thread,
// |HandleIsReady()| and the bound |this| have to be thread-safe.
class AsyncHandleWaiter::Context
    : public base::RefCountedThreadSafe<AsyncHandleWaiter::Context,
                                        AsyncHandleWaiterContextTraits>,
      public base::MessageLoopForIO::IOObserver {
 public:
  Context(base::WeakPtr<AsyncHandleWaiter> waiter)
      : io_runner_(base::MessageLoopForIO::current()->task_runner()),
        waiter_(waiter),
        last_result_(MOJO_RESULT_INTERNAL),
        io_loop_level_(0),
        should_invoke_callback_(false) {
    base::MessageLoopForIO::current()->AddIOObserver(this);
  }

  void HandleIsReady(MojoResult result) {
    last_result_ = result;

    // If the signaling happens in the IO handler, use |IOObserver| callback
    // to invoke the callback.
    if (IsCalledFromIOHandler()) {
      should_invoke_callback_ = true;
      return;
    }

    io_runner_->PostTask(FROM_HERE,
                         base::Bind(&Context::InvokeWaiterCallback, this));
  }

 private:
  friend void base::DeletePointer<const Context>(const Context* self);
  friend class AsyncHandleWaiterContextTraits;
  friend class base::RefCountedThreadSafe<Context>;

  ~Context() override {
    DCHECK(base::MessageLoopForIO::current()->task_runner() == io_runner_);
    base::MessageLoopForIO::current()->RemoveIOObserver(this);
  }

  bool IsCalledFromIOHandler() const {
    base::MessageLoop* loop = base::MessageLoop::current();
    if (!loop)
      return false;
    if (loop->task_runner() != io_runner_)
      return false;
    return io_loop_level_ > 0;
  }

  // Called from |io_runner_| thus safe to touch |waiter_|.
  void InvokeWaiterCallback() {
    MojoResult result = last_result_;
    last_result_ = MOJO_RESULT_INTERNAL;
    if (waiter_)
      waiter_->InvokeCallback(result);
  }

  // IOObserver implementation:

  void WillProcessIOEvent() override {
    DCHECK(io_loop_level_ != 0 || !should_invoke_callback_);
    DCHECK_GE(io_loop_level_, 0);
    io_loop_level_++;
  }

  void DidProcessIOEvent() override {
    // This object could have been constructed in another's class's
    // DidProcessIOEvent.
    if (io_loop_level_== 0)
      return;

    DCHECK_GE(io_loop_level_, 1);

    // Leaving a nested loop.
    if (io_loop_level_ > 1) {
      io_loop_level_--;
      return;
    }

    // The zero |waiter_| indicates that |this| have lost the owner and can be
    // under destruction. So we cannot wrap it with a |scoped_refptr| anymore.
    if (!waiter_) {
      should_invoke_callback_ = false;
      io_loop_level_--;
      return;
    }

    // We have to protect |this| because |AsyncHandleWaiter| can be
    // deleted during the callback.
    scoped_refptr<Context> protect(this);
    while (should_invoke_callback_) {
      should_invoke_callback_ = false;
      InvokeWaiterCallback();
    }

    io_loop_level_--;
  }

  // Only |io_runner_| is accessed from arbitrary threads.  Others are touched
  // only from the IO thread.
  const scoped_refptr<base::TaskRunner> io_runner_;

  const base::WeakPtr<AsyncHandleWaiter> waiter_;
  MojoResult last_result_;
  int io_loop_level_;
  bool should_invoke_callback_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

AsyncHandleWaiter::AsyncHandleWaiter(base::Callback<void(MojoResult)> callback)
    : callback_(callback),
      weak_factory_(this) {
  context_ = new Context(weak_factory_.GetWeakPtr());
}

AsyncHandleWaiter::~AsyncHandleWaiter() {
}

MojoResult AsyncHandleWaiter::Wait(MojoHandle handle,
                                   MojoHandleSignals signals) {
  return mojo::edk::AsyncWait(handle, signals,
                              base::Bind(&Context::HandleIsReady, context_));
}

void AsyncHandleWaiter::InvokeCallback(MojoResult result) {
  callback_.Run(result);
}

base::MessageLoopForIO::IOObserver* AsyncHandleWaiter::GetIOObserverForTest() {
  return context_.get();
}

base::Callback<void(MojoResult)> AsyncHandleWaiter::GetWaitCallbackForTest() {
  return base::Bind(&Context::HandleIsReady, context_);
}

// static
void AsyncHandleWaiterContextTraits::Destruct(
    const AsyncHandleWaiter::Context* context) {
  context->io_runner_->PostTask(
      FROM_HERE,
      base::Bind(&base::DeletePointer<const AsyncHandleWaiter::Context>,
                 base::Unretained(context)));
}

}  // namespace internal
}  // namespace IPC
