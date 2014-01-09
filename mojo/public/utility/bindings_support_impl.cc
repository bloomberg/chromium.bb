// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/utility/bindings_support_impl.h"

#include <assert.h>

#include "mojo/public/bindings/lib/buffer.h"
#include "mojo/public/utility/run_loop.h"
#include "mojo/public/utility/run_loop_handler.h"
#include "mojo/public/utility/thread_local.h"

namespace mojo {
namespace internal {
namespace {

ThreadLocalPointer<Buffer>* tls_buffer = NULL;

// RunLoopHandler implementation used for a request to AsyncWait(). There are
// two ways RunLoopHandlerImpl is deleted:
// . when the handle is ready (or errored).
// . when BindingsSupport::CancelWait() is invoked.
class RunLoopHandlerImpl : public RunLoopHandler {
 public:
  RunLoopHandlerImpl(const Handle& handle,
                     BindingsSupport::AsyncWaitCallback* callback)
      : handle_(handle),
        callback_(callback) {}
  virtual ~RunLoopHandlerImpl() {
    RunLoop::current()->RemoveHandler(handle_);
  }

  // RunLoopHandler:
  virtual void OnHandleReady(const Handle& handle) MOJO_OVERRIDE {
    NotifyCallback(MOJO_RESULT_OK);
  }

  virtual void OnHandleError(const Handle& handle,
                             MojoResult result) MOJO_OVERRIDE {
    NotifyCallback(result);
  }

 private:
  void NotifyCallback(MojoResult result) {
    // Delete this to unregister the handle. That way if the callback
    // reregisters everything is ok.
    BindingsSupport::AsyncWaitCallback* callback = callback_;
    delete this;

    callback->OnHandleReady(result);
  }

  const Handle handle_;
  BindingsSupport::AsyncWaitCallback* callback_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(RunLoopHandlerImpl);
};

}  // namespace

BindingsSupportImpl::BindingsSupportImpl() {
}

BindingsSupportImpl::~BindingsSupportImpl() {
}

// static
void BindingsSupportImpl::SetUp() {
  assert(!tls_buffer);
  tls_buffer = new ThreadLocalPointer<Buffer>;
}

// static
void BindingsSupportImpl::TearDown() {
  assert(tls_buffer);
  delete tls_buffer;
  tls_buffer = NULL;
}

Buffer* BindingsSupportImpl::GetCurrentBuffer() {
  return tls_buffer->Get();
}

Buffer* BindingsSupportImpl::SetCurrentBuffer(Buffer* buf) {
  Buffer* old_buf = tls_buffer->Get();
  tls_buffer->Set(buf);
  return old_buf;
}

BindingsSupport::AsyncWaitID BindingsSupportImpl::AsyncWait(
    const Handle& handle,
    MojoWaitFlags flags,
    AsyncWaitCallback* callback) {
  RunLoop* run_loop = RunLoop::current();
  assert(run_loop);
  // |run_loop_handler| is destroyed either when the handle is ready or if
  // CancelWait is invoked.
  RunLoopHandlerImpl* run_loop_handler =
      new RunLoopHandlerImpl(handle, callback);
  run_loop->AddHandler(run_loop_handler, handle, flags,
                       MOJO_DEADLINE_INDEFINITE);
  return run_loop_handler;
}

void BindingsSupportImpl::CancelWait(AsyncWaitID async_wait_id) {
  delete static_cast<RunLoopHandlerImpl*>(async_wait_id);
}

}  // namespace internal
}  // namespace mojo
