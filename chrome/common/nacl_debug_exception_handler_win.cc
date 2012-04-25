// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/nacl_debug_exception_handler_win.h"

#include "base/process_util.h"
#include "base/threading/platform_thread.h"
#include "native_client/src/trusted/service_runtime/win/debug_exception_handler.h"

namespace {

class DebugExceptionHandler : public base::PlatformThread::Delegate {
 public:
  DebugExceptionHandler(int32 pid,
                        base::MessageLoopProxy* message_loop,
                        const base::Closure& on_connected)
      : pid_(pid), message_loop_(message_loop), on_connected_(on_connected) {
  }

  virtual void ThreadMain() OVERRIDE {
    // In the Windows API, the set of processes being debugged is
    // thread-local, so we have to attach to the process (using
    // DebugActiveProcess()) on the same thread on which
    // NaClDebugLoop() receives debug events for the process.
    BOOL attached = false;
    base::ProcessHandle process_handle = base::kNullProcessHandle;
    if (!base::OpenProcessHandleWithAccess(
             pid_,
             base::kProcessAccessQueryInformation |
             base::kProcessAccessSuspendResume |
             base::kProcessAccessTerminate |
             base::kProcessAccessVMOperation |
             base::kProcessAccessVMRead |
             base::kProcessAccessVMWrite |
             base::kProcessAccessWaitForTermination,
             &process_handle)) {
      LOG(ERROR) << "Failed to get process handle";
    } else {
      attached = DebugActiveProcess(pid_);
      if (!attached) {
        LOG(ERROR) << "Failed to connect to the process";
      }
    }
    // At the moment we do not say in the reply whether attaching as a
    // debugger succeeded.  In the future, when we attach on demand
    // when an exception handler is first registered, we can make the
    // NaCl syscall indicate whether attaching succeeded.
    message_loop_->PostTask(FROM_HERE, on_connected_);

    if (attached) {
      DWORD exit_code;
      NaClDebugLoop(process_handle, &exit_code);
    }
    if (process_handle != base::kNullProcessHandle) {
      base::CloseProcessHandle(process_handle);
    }
    delete this;
  }

 private:
  int32 pid_;
  base::MessageLoopProxy* message_loop_;
  base::Closure on_connected_;

  DISALLOW_COPY_AND_ASSIGN(DebugExceptionHandler);
};

}  // namespace

void NaClStartDebugExceptionHandlerThread(int32 nacl_process_id,
                                          base::MessageLoopProxy* message_loop,
                                          const base::Closure& on_connected) {
  // The new PlatformThread will take ownership of the
  // DebugExceptionHandler object, which will delete itself on exit.
  DebugExceptionHandler* handler = new DebugExceptionHandler(
      nacl_process_id, message_loop, on_connected);
  if (!base::PlatformThread::CreateNonJoinable(0, handler)) {
    on_connected.Run();
    delete handler;
  }
}
