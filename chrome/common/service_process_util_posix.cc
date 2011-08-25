// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util_posix.h"

#include "base/basictypes.h"
#include "base/eintr_wrapper.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"

namespace {
int g_signal_socket = -1;
}

ServiceProcessTerminateMonitor::ServiceProcessTerminateMonitor(
    Task* terminate_task)
    : terminate_task_(terminate_task) {
}

ServiceProcessTerminateMonitor::~ServiceProcessTerminateMonitor() {
}

void ServiceProcessTerminateMonitor::OnFileCanReadWithoutBlocking(int fd) {
  if (terminate_task_.get()) {
    int buffer;
    int length = read(fd, &buffer, sizeof(buffer));
    if ((length == sizeof(buffer)) && (buffer == kTerminateMessage)) {
      terminate_task_->Run();
      terminate_task_.reset();
    } else if (length > 0) {
      LOG(ERROR) << "Unexpected read: " << buffer;
    } else if (length == 0) {
      LOG(ERROR) << "Unexpected fd close";
    } else if (length < 0) {
      PLOG(ERROR) << "read";
    }
  }
}

void ServiceProcessTerminateMonitor::OnFileCanWriteWithoutBlocking(int fd) {
  NOTIMPLEMENTED();
}

// "Forced" Shutdowns on POSIX are done via signals. The magic signal for
// a shutdown is SIGTERM. "write" is a signal safe function. PLOG(ERROR) is
// not, but we don't ever expect it to be called.
static void SigTermHandler(int sig, siginfo_t* info, void* uap) {
  // TODO(dmaclach): add security here to make sure that we are being shut
  //                 down by an appropriate process.
  int message = ServiceProcessTerminateMonitor::kTerminateMessage;
  if (write(g_signal_socket, &message, sizeof(message)) < 0) {
    PLOG(ERROR) << "write";
  }
}

ServiceProcessState::StateData::StateData() : set_action_(false) {
  memset(sockets_, -1, sizeof(sockets_));
  memset(&old_action_, 0, sizeof(old_action_));
}

void ServiceProcessState::StateData::SignalReady(base::WaitableEvent* signal,
                                                 bool* success) {
  CHECK_EQ(g_signal_socket, -1);
  CHECK(!signal->IsSignaled());
   *success = MessageLoopForIO::current()->WatchFileDescriptor(
      sockets_[0], true, MessageLoopForIO::WATCH_READ,
      &watcher_, terminate_monitor_.get());
  if (!*success) {
    LOG(ERROR) << "WatchFileDescriptor";
    signal->Signal();
    return;
  }
  g_signal_socket = sockets_[1];

  // Set up signal handler for SIGTERM.
  struct sigaction action;
  action.sa_sigaction = SigTermHandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO;
  *success = sigaction(SIGTERM, &action, &old_action_) == 0;
  if (!*success) {
    PLOG(ERROR) << "sigaction";
    signal->Signal();
    return;
  }

  // If the old_action is not default, somebody else has installed a
  // a competing handler. Our handler is going to override it so it
  // won't be called. If this occurs it needs to be fixed.
  DCHECK_EQ(old_action_.sa_handler, SIG_DFL);
  set_action_ = true;

#if defined(OS_MACOSX)
  *success = WatchExecutable();
  if (!*success) {
    LOG(ERROR) << "WatchExecutable";
    signal->Signal();
    return;
  }
#elif defined(OS_POSIX)
  initializing_lock_.reset();
#endif  // OS_POSIX
  signal->Signal();
}

ServiceProcessState::StateData::~StateData() {
  if (sockets_[0] != -1) {
    if (HANDLE_EINTR(close(sockets_[0]))) {
      PLOG(ERROR) << "close";
    }
  }
  if (sockets_[1] != -1) {
    if (HANDLE_EINTR(close(sockets_[1]))) {
      PLOG(ERROR) << "close";
    }
  }
  if (set_action_) {
    if (sigaction(SIGTERM, &old_action_, NULL) < 0) {
      PLOG(ERROR) << "sigaction";
    }
  }
  g_signal_socket = -1;
}

void ServiceProcessState::CreateState() {
  CHECK(!state_);
  state_ = new StateData;

  // Explicitly adding a reference here (and removing it in TearDownState)
  // because StateData is refcounted on Mac and Linux so that methods can
  // be called on other threads.
  // It is not refcounted on Windows at this time.
  state_->AddRef();
}

bool ServiceProcessState::SignalReady(
    base::MessageLoopProxy* message_loop_proxy, Task* terminate_task) {
  CHECK(state_);

  scoped_ptr<Task> scoped_terminate_task(terminate_task);
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  state_->running_lock_.reset(TakeServiceRunningLock(true));
  if (state_->running_lock_.get() == NULL) {
    return false;
  }
#endif
  state_->terminate_monitor_.reset(
      new ServiceProcessTerminateMonitor(scoped_terminate_task.release()));
  if (pipe(state_->sockets_) < 0) {
    PLOG(ERROR) << "pipe";
    return false;
  }
  base::WaitableEvent signal_ready(true, false);
  bool success = false;

  message_loop_proxy->PostTask(FROM_HERE,
      NewRunnableMethod(state_, &ServiceProcessState::StateData::SignalReady,
                        &signal_ready,
                        &success));
  signal_ready.Wait();
  return success;
}

void ServiceProcessState::TearDownState() {
  if (state_) {
    state_->Release();
    state_ = NULL;
  }
}
