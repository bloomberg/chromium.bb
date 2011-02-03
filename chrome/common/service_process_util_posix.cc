// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util.h"

#include <signal.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/message_pump_libevent.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/multi_process_lock.h"

namespace {

int g_signal_socket = -1;

// Gets the name of the lock file for service process.
FilePath GetServiceProcessLockFilePath() {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  chrome::VersionInfo version_info;
  std::string lock_file_name = version_info.Version() + "Service Process Lock";
  return user_data_dir.Append(lock_file_name);
}

// Attempts to take a lock named |name|. If |waiting| is true then this will
// make multiple attempts to acquire the lock.
// Caller is responsible for ownership of the MultiProcessLock.
MultiProcessLock* TakeNamedLock(const std::string& name, bool waiting) {
  scoped_ptr<MultiProcessLock> lock(MultiProcessLock::Create(name));
  if (lock == NULL) return NULL;
  bool got_lock = false;
  for (int i = 0; i < 10; ++i) {
    if (lock->TryLock()) {
      got_lock = true;
      break;
    }
    if (!waiting) break;
    base::PlatformThread::Sleep(100 * i);
  }
  if (!got_lock) {
    lock.reset();
  }
  return lock.release();
}

MultiProcessLock* TakeServiceRunningLock(bool waiting) {
  std::string lock_name =
      GetServiceProcessScopedName("_service_running");
  return TakeNamedLock(lock_name, waiting);
}

MultiProcessLock* TakeServiceInitializingLock(bool waiting) {
  std::string lock_name =
      GetServiceProcessScopedName("_service_initializing");
  return TakeNamedLock(lock_name, waiting);
}

// Watches for |kShutDownMessage| to be written to the file descriptor it is
// watching. When it reads |kShutDownMessage|, it performs |shutdown_task_|.
// Used here to monitor the socket listening to g_signal_socket.
class ServiceProcessShutdownMonitor
    : public base::MessagePumpLibevent::Watcher {
 public:

  enum {
    kShutDownMessage = 0xdecea5e
  };

  explicit ServiceProcessShutdownMonitor(Task* shutdown_task)
      : shutdown_task_(shutdown_task) {
  }

  virtual ~ServiceProcessShutdownMonitor();

  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int fd);

 private:
  scoped_ptr<Task> shutdown_task_;
};

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
void SigTermHandler(int sig, siginfo_t* info, void* uap) {
  // TODO(dmaclach): add security here to make sure that we are being shut
  //                 down by an appropriate process.
  int message = ServiceProcessShutdownMonitor::kShutDownMessage;
  if (write(g_signal_socket, &message, sizeof(message)) < 0) {
    PLOG(ERROR) << "write";
  }
}

}  // namespace

// See comment for SigTermHandler.
bool ForceServiceProcessShutdown(const std::string& version,
                                 base::ProcessId process_id) {
  if (kill(process_id, SIGTERM) < 0) {
    PLOG(ERROR) << "kill";
    return false;
  }
  return true;
}

bool CheckServiceProcessReady() {
  scoped_ptr<MultiProcessLock> running_lock(TakeServiceRunningLock(false));
  return running_lock.get() == NULL;
}

struct ServiceProcessState::StateData
    : public base::RefCountedThreadSafe<ServiceProcessState::StateData> {
  scoped_ptr<MultiProcessLock> initializing_lock_;
  scoped_ptr<MultiProcessLock> running_lock_;
  scoped_ptr<ServiceProcessShutdownMonitor> shut_down_monitor_;
  base::MessagePumpLibevent::FileDescriptorWatcher watcher_;
  int sockets_[2];
  struct sigaction old_action_;
  bool set_action_;

  // WatchFileDescriptor needs to be set up by the thread that is going
  // to be monitoring it.
  void SignalReady() {
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
      initializing_lock_.reset();
    } else {
      PLOG(ERROR) << "sigaction";
    }
  }
};

bool ServiceProcessState::TakeSingletonLock() {
  CHECK(!state_);
  state_ = new StateData;
  state_->AddRef();
  state_->sockets_[0] = -1;
  state_->sockets_[1] = -1;
  state_->set_action_ = false;
  state_->initializing_lock_.reset(TakeServiceInitializingLock(true));
  return state_->initializing_lock_.get();
}

bool ServiceProcessState::SignalReady(
    base::MessageLoopProxy* message_loop_proxy, Task* shutdown_task) {
  CHECK(state_);
  CHECK_EQ(g_signal_socket, -1);

  state_->running_lock_.reset(TakeServiceRunningLock(true));
  if (state_->running_lock_.get() == NULL) {
    return false;
  }
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

bool ServiceProcessState::AddToAutoRun() {
  NOTIMPLEMENTED();
  return false;
}

bool ServiceProcessState::RemoveFromAutoRun() {
  NOTIMPLEMENTED();
  return false;
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
