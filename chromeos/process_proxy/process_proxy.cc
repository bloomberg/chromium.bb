// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/process_proxy/process_proxy.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/threading/thread.h"
#include "chromeos/process_proxy/process_output_watcher.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace {

enum PipeEnd {
  PIPE_END_READ,
  PIPE_END_WRITE
};

enum PseudoTerminalFd {
  PT_MASTER_FD,
  PT_SLAVE_FD
};

const int kInvalidFd = -1;

}  // namespace

namespace chromeos {

ProcessProxy::ProcessProxy(): process_launched_(false),
                              callback_set_(false),
                              watcher_started_(false) {
  // Set pipes to initial, invalid value so we can easily know if a pipe was
  // opened by us.
  ClearAllFdPairs();
};

bool ProcessProxy::Open(const std::string& command, pid_t* pid) {
  if (process_launched_)
    return false;

  if (!CreatePseudoTerminalPair(pt_pair_)) {
    return false;
  }

  process_launched_ = LaunchProcess(command, pt_pair_[PT_SLAVE_FD], &pid_);

  if (process_launched_) {
    // We won't need these anymore. These will be used by the launched process.
    CloseFd(&pt_pair_[PT_SLAVE_FD]);
    *pid = pid_;
    LOG(WARNING) << "Process launched: " << pid_;
  } else {
    CloseFdPair(pt_pair_);
  }
  return process_launched_;
}

bool ProcessProxy::StartWatchingOnThread(
    base::Thread* watch_thread,
    const ProcessOutputCallback& callback) {
  DCHECK(process_launched_);
  if (watcher_started_)
    return false;
  if (pipe(shutdown_pipe_))
    return false;

  // We give ProcessOutputWatcher a copy of master to make life easier during
  // tear down.
  // TODO(tbarzic): improve fd managment.
  int master_copy = HANDLE_EINTR(dup(pt_pair_[PT_MASTER_FD]));
  if (master_copy == -1)
    return false;

  callback_set_ = true;
  callback_ = callback;
  callback_runner_ = base::MessageLoopProxy::current();

  // This object will delete itself once watching is stopped.
  // It also takes ownership of the passed fds.
  ProcessOutputWatcher* output_watcher =
      new ProcessOutputWatcher(master_copy,
                               shutdown_pipe_[PIPE_END_READ],
                               base::Bind(&ProcessProxy::OnProcessOutput,
                                          this));

  // Output watcher took ownership of the read end of shutdown pipe.
  shutdown_pipe_[PIPE_END_READ] = -1;

  // |watch| thread is blocked by |output_watcher| from now on.
  watch_thread->message_loop()->PostTask(FROM_HERE,
      base::Bind(&ProcessOutputWatcher::Start,
                 base::Unretained(output_watcher)));
  watcher_started_ = true;
  return true;
}

void ProcessProxy::OnProcessOutput(ProcessOutputType type,
                                   const std::string& output) {
  if (!callback_runner_.get())
    return;

  callback_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ProcessProxy::CallOnProcessOutputCallback,
                 this, type, output));
}

void ProcessProxy::CallOnProcessOutputCallback(ProcessOutputType type,
                                               const std::string& output) {
  // We may receive some output even after Close was called (crosh process does
  // not have to quit instantly, or there may be some trailing data left in
  // output stream fds). In that case owner of the callback may be gone so we
  // don't want to send it anything. |callback_set_| is reset when this gets
  // closed.
  if (callback_set_)
    callback_.Run(type, output);
}

bool ProcessProxy::StopWatching() {
  if (!watcher_started_)
    return true;
  // Signal Watcher that we are done. We use self-pipe trick to unblock watcher.
  // Anything may be written to the pipe.
  const char message[] = "q";
  return base::WriteFileDescriptor(shutdown_pipe_[PIPE_END_WRITE],
                                   message, sizeof(message));
}

void ProcessProxy::Close() {
  if (!process_launched_)
    return;

  process_launched_ = false;
  callback_set_ = false;
  callback_ = ProcessOutputCallback();
  callback_runner_ = NULL;

  base::KillProcess(pid_, 0, true /* wait */);

  // TODO(tbarzic): What if this fails?
  StopWatching();

  CloseAllFdPairs();
}

bool ProcessProxy::Write(const std::string& text) {
  if (!process_launched_)
    return false;

  // We don't want to write '\0' to the pipe.
  size_t data_size = text.length() * sizeof(*text.c_str());
  int bytes_written =
      base::WriteFileDescriptor(pt_pair_[PT_MASTER_FD],
                                text.c_str(), data_size);
  return (bytes_written == static_cast<int>(data_size));
}

bool ProcessProxy::OnTerminalResize(int width, int height) {
  if (width < 0 || height < 0)
    return false;

  winsize ws;
  // Number of rows.
  ws.ws_row = height;
  // Number of columns.
  ws.ws_col = width;

  return (HANDLE_EINTR(ioctl(pt_pair_[PT_MASTER_FD], TIOCSWINSZ, &ws)) != -1);
}

ProcessProxy::~ProcessProxy() {
  // In case watcher did not started, we may get deleted without calling Close.
  // In that case we have to clean up created pipes. If watcher had been
  // started, there will be a callback with our reference owned by
  // process_output_watcher until Close is called, so we know Close has been
  // called  by now (and pipes have been cleaned).
  if (!watcher_started_)
    CloseAllFdPairs();
}

bool ProcessProxy::CreatePseudoTerminalPair(int *pt_pair) {
  ClearFdPair(pt_pair);

  // Open Master.
  pt_pair[PT_MASTER_FD] = HANDLE_EINTR(posix_openpt(O_RDWR | O_NOCTTY));
  if (pt_pair[PT_MASTER_FD] == -1)
    return false;

  if (grantpt(pt_pair_[PT_MASTER_FD]) != 0 ||
      unlockpt(pt_pair_[PT_MASTER_FD]) != 0) {
    CloseFd(&pt_pair[PT_MASTER_FD]);
    return false;
  }
  char* slave_name = NULL;
  // Per man page, slave_name must not be freed.
  slave_name = ptsname(pt_pair_[PT_MASTER_FD]);
  if (slave_name)
    pt_pair_[PT_SLAVE_FD] = HANDLE_EINTR(open(slave_name, O_RDWR | O_NOCTTY));

  if (pt_pair_[PT_SLAVE_FD] == -1) {
    CloseFdPair(pt_pair);
    return false;
  }

  return true;
}

bool ProcessProxy::LaunchProcess(const std::string& command, int slave_fd,
                                 pid_t* pid) {
  // Redirect crosh  process' output and input so we can read it.
  base::FileHandleMappingVector fds_mapping;
  fds_mapping.push_back(std::make_pair(slave_fd, STDIN_FILENO));
  fds_mapping.push_back(std::make_pair(slave_fd, STDOUT_FILENO));
  fds_mapping.push_back(std::make_pair(slave_fd, STDERR_FILENO));
  base::LaunchOptions options;
  // Do not set NO_NEW_PRIVS on processes if the system is in dev-mode. This
  // permits sudo in the crosh shell when in developer mode.
  options.allow_new_privs = base::CommandLine::ForCurrentProcess()->
      HasSwitch(chromeos::switches::kSystemInDevMode);
  options.fds_to_remap = &fds_mapping;
  options.ctrl_terminal_fd = slave_fd;
  options.environ["TERM"] = "xterm";

  // Launch the process.
  return base::LaunchProcess(CommandLine(base::FilePath(command)), options,
                             pid);
}

void ProcessProxy::CloseAllFdPairs() {
  CloseFdPair(pt_pair_);
  CloseFdPair(shutdown_pipe_);
}

void ProcessProxy::CloseFdPair(int* pipe) {
  CloseFd(&(pipe[PIPE_END_READ]));
  CloseFd(&(pipe[PIPE_END_WRITE]));
}

void ProcessProxy::CloseFd(int* fd) {
  if (*fd != kInvalidFd) {
    if (IGNORE_EINTR(close(*fd)) != 0)
      DPLOG(WARNING) << "close fd failed.";
  }
  *fd = kInvalidFd;
}

void ProcessProxy::ClearAllFdPairs() {
  ClearFdPair(pt_pair_);
  ClearFdPair(shutdown_pipe_);
}

void ProcessProxy::ClearFdPair(int* pipe) {
  pipe[PIPE_END_READ] = kInvalidFd;
  pipe[PIPE_END_WRITE] = kInvalidFd;
}

}  // namespace chromeos
