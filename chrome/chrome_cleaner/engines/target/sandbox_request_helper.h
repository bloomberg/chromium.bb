// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOX_REQUEST_HELPER_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOX_REQUEST_HELPER_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/run_loop.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/chrome_cleaner/engines/common/sandbox_error_code.h"

namespace chrome_cleaner {

// A basic struct to return the state of the Mojo request, including any
// specific error messages.
struct MojoCallStatus {
  enum State {
    MOJO_CALL_MADE,
    MOJO_CALL_ERROR,
  } state;

  SandboxErrorCode error_code;

  static MojoCallStatus Success();

  static MojoCallStatus Failure(SandboxErrorCode error_code);
};

// These functions are included in the header so SyncSandboxRequest can access
// them, they shouldn't be used by anyone else.
namespace internal {

void SaveMojoCallStatus(base::OnceClosure quit_closure,
                        MojoCallStatus* status_out,
                        MojoCallStatus status);

// Returns a wrapper that executes |closure| on the given |task_runner|.
base::OnceClosure ClosureForTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner,
    base::OnceClosure closure);

}  // namespace internal

// This template function is inlined in the header because otherwise we would
// have to introduce a templated class, and then the callsites would no longer
// be able to implicit call the right function, but would have to explicitly
// create the class.
template <typename ProxyType,
          typename RequestCallback,
          typename ResponseCallback>
MojoCallStatus SyncSandboxRequest(ProxyType* proxy,
                                  RequestCallback request,
                                  ResponseCallback response) {
  if (!proxy) {
    return MojoCallStatus::Failure(SandboxErrorCode::INTERNAL_ERROR);
  }

  // If there is not already a MessageLoop on this thread (eg. when called on a
  // thread created by a 3rd-party engine), create one.
  std::unique_ptr<base::MessageLoop> local_loop;
  if (!base::MessageLoopCurrent::IsSet())
    local_loop = std::make_unique<base::MessageLoop>();

  // Start a local RunLoop to receive the results from the request. It will
  // execute until either the response callback or the early response callback
  // causes it to quit, as follows:
  //
  // 1. If an error occurs that prevents the asynchronous Mojo call from being
  // made, the early response callback will quit the loop. The response
  // callback will not be called.
  //
  // 2. Otherwise as soon as the asynchronous Mojo call is made, the early
  // response callback will be invoked with MOJO_CALL_MADE, and the loop will
  // continue.
  //
  // 3. When the asynchronous Mojo call returns a response, the response
  // callback will be called and will quit the loop.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  MojoCallStatus call_status;
  auto early_response_callback = base::BindOnce(
      &internal::SaveMojoCallStatus, run_loop.QuitClosure(), &call_status);

  // The response callback will be executed on the Mojo IO thread, so it will
  // need to hop back to |message_loop|'s task runner to quit the run loop.
  auto response_callback = base::BindOnce(
      std::move(response),
      internal::ClosureForTaskRunner(base::ThreadTaskRunnerHandle::Get(),
                                     run_loop.QuitClosure()));

  // Invoke the asynchronous Mojo call on the Mojo IO thread.
  base::PostTaskAndReplyWithResult(
      proxy->task_runner().get(), FROM_HERE,
      base::BindOnce(std::move(request), std::move(response_callback)),
      std::move(early_response_callback));
  run_loop.Run();
  return call_status;
}

template <typename ProxyType,
          typename RequestCallback,
          typename ResponseCallback>
MojoCallStatus SyncSandboxRequest(scoped_refptr<ProxyType> proxy,
                                  RequestCallback request,
                                  ResponseCallback response) {
  return SyncSandboxRequest(proxy.get(), std::move(request),
                            std::move(response));
}

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_SANDBOX_REQUEST_HELPER_H_
