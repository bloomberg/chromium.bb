// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/nacl_debug_exception_handler_win.h"

#include "base/bind.h"
#include "base/process_util.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"
#include "native_client/src/trusted/service_runtime/win/debug_exception_handler.h"

namespace {

class DebugExceptionHandler : public base::PlatformThread::Delegate {
 public:
  DebugExceptionHandler(base::ProcessHandle nacl_process,
                        const std::string& startup_info,
                        base::MessageLoopProxy* message_loop,
                        const base::Callback<void(bool)>& on_connected)
      : nacl_process_(nacl_process),
        startup_info_(startup_info),
        message_loop_(message_loop),
        on_connected_(on_connected) {
  }

  virtual void ThreadMain() OVERRIDE {
    // In the Windows API, the set of processes being debugged is
    // thread-local, so we have to attach to the process (using
    // DebugActiveProcess()) on the same thread on which
    // NaClDebugExceptionHandlerRun() receives debug events for the
    // process.
    bool attached = false;
    int pid = GetProcessId(nacl_process_);
    if (pid == 0) {
      LOG(ERROR) << "Invalid process handle";
    } else {
      if (!DebugActiveProcess(pid)) {
        LOG(ERROR) << "Failed to connect to the process";
      } else {
        attached = true;
      }
    }
    message_loop_->PostTask(FROM_HERE, base::Bind(on_connected_, attached));

    if (attached) {
      // TODO(mseaborn): Clean up the NaCl side to remove the need for
      // a const_cast here.
      NaClDebugExceptionHandlerRun(
          nacl_process_,
          reinterpret_cast<void*>(const_cast<char*>(startup_info_.data())),
          startup_info_.size());
    }
    delete this;
  }

 private:
  base::win::ScopedHandle nacl_process_;
  std::string startup_info_;
  base::MessageLoopProxy* message_loop_;
  base::Callback<void(bool)> on_connected_;

  DISALLOW_COPY_AND_ASSIGN(DebugExceptionHandler);
};

}  // namespace

void NaClStartDebugExceptionHandlerThread(
    base::ProcessHandle nacl_process,
    const std::string& startup_info,
    base::MessageLoopProxy* message_loop,
    const base::Callback<void(bool)>& on_connected) {
  // The new PlatformThread will take ownership of the
  // DebugExceptionHandler object, which will delete itself on exit.
  DebugExceptionHandler* handler = new DebugExceptionHandler(
      nacl_process, startup_info, message_loop, on_connected);
  if (!base::PlatformThread::CreateNonJoinable(0, handler)) {
    on_connected.Run(false);
    delete handler;
  }
}
