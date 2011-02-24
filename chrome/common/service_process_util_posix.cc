// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util_posix.h"

#include "base/basictypes.h"
#include "base/message_loop_proxy.h"

namespace {
int g_signal_socket = -1;
}

ServiceProcessShutdownMonitor::~ServiceProcessShutdownMonitor() {
}

void ServiceProcessShutdownMonitor::OnFileCanReadWithoutBlocking(int fd) {
  if (shutdown_task_.get()) {
    int buffer;
    int length = read(fd, &buffer, sizeof(buffer));
    if ((length == sizeof(buffer)) && (buffer == kShutDownMessage)) {
      shutdown_task_->Run();
      shutdown_task_.reset();
    } else if (length > 0) {
      LOG(ERROR) << "Unexpected read: " << buffer;
    } else if (length == 0) {
      LOG(ERROR) << "Unexpected fd close";
    } else if (length < 0) {
      PLOG(ERROR) << "read";
    }
  }
}

void ServiceProcessShutdownMonitor::OnFileCanWriteWithoutBlocking(int fd) {
  NOTIMPLEMENTED();
}

// "Forced" Shutdowns on POSIX are done via signals. The magic signal for
// a shutdown is SIGTERM. "write" is a signal safe function. PLOG(ERROR) is
// not, but we don't ever expect it to be called.
static void SigTermHandler(int sig, siginfo_t* info, void* uap) {
  // TODO(dmaclach): add security here to make sure that we are being shut
  //                 down by an appropriate process.
  int message = ServiceProcessShutdownMonitor::kShutDownMessage;
  if (write(g_signal_socket, &message, sizeof(message)) < 0) {
    PLOG(ERROR) << "write";
  }
}

void ServiceProcessState::StateData::SignalReady() {
  CHECK(MessageLoopForIO::current()->WatchFileDescriptor(
      sockets_[0], true, MessageLoopForIO::WATCH_READ,
      &watcher_, shut_down_monitor_.get()));
  g_signal_socket = sockets_[1];

  // Set up signal handler for SIGTERM.
  struct sigaction action;
  action.sa_sigaction = SigTermHandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = SA_SIGINFO;
  if (sigaction(SIGTERM, &action, &old_action_) == 0) {
    // If the old_action is not default, somebody else has installed a
    // a competing handler. Our handler is going to override it so it
    // won't be called. If this occurs it needs to be fixed.
    DCHECK_EQ(old_action_.sa_handler, SIG_DFL);
    set_action_ = true;
#if defined(OS_LINUX)
    initializing_lock_.reset();
#endif  // OS_LINUX
  } else {
    PLOG(ERROR) << "sigaction";
  }
}

bool ServiceProcessState::InitializeState() {
  CHECK(!state_);
  state_ = new StateData;
  state_->AddRef();
  state_->sockets_[0] = -1;
  state_->sockets_[1] = -1;
  state_->set_action_ = false;
  return true;
}

bool ServiceProcessState::SignalReady(
    base::MessageLoopProxy* message_loop_proxy, Task* shutdown_task) {
  CHECK(state_);
  CHECK_EQ(g_signal_socket, -1);

#if defined(OS_LINUX)
  state_->running_lock_.reset(TakeServiceRunningLock(true));
  if (state_->running_lock_.get() == NULL) {
    return false;
  }
#endif // OS_LINUX
  state_->shut_down_monitor_.reset(
      new ServiceProcessShutdownMonitor(shutdown_task));
  if (pipe(state_->sockets_) < 0) {
    PLOG(ERROR) << "pipe";
    return false;
  }
  message_loop_proxy->PostTask(FROM_HERE,
      NewRunnableMethod(state_, &ServiceProcessState::StateData::SignalReady));
  return true;
}

void ServiceProcessState::TearDownState() {
  g_signal_socket = -1;
  if (state_) {
    if (state_->sockets_[0] != -1) {
      close(state_->sockets_[0]);
    }
    if (state_->sockets_[1] != -1) {
      close(state_->sockets_[1]);
    }
    if (state_->set_action_) {
      if (sigaction(SIGTERM, &state_->old_action_, NULL) < 0) {
        PLOG(ERROR) << "sigaction";
      }
    }
    state_->Release();
    state_ = NULL;
  }
}
