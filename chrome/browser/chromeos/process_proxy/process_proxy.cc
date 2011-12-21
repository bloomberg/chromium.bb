// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/process_proxy/process_proxy.h"

#include <cstdio>
#include <fcntl.h>
#include <signal.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/file_util.h"
#include "base/process_util.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/process_proxy/process_output_watcher.h"
#include "content/public/browser/browser_thread.h"

namespace {

enum PipeEnd {
  PIPE_END_READ,
  PIPE_END_WRITE
};

const int kInvalidFd = -1;

}  // namespace

ProcessProxy::ProcessProxy(): process_launched_(false),
                              callback_set_(false),
                              watcher_started_(false) {
  // Set pipes to initial, invalid value so we can easily know if a pipe was
  // opened by us.
  ClearAllPipes();
};

bool ProcessProxy::Open(const std::string& command, pid_t* pid) {
  if (process_launched_)
    return false;

  if (HANDLE_EINTR(pipe(out_pipe_)) || HANDLE_EINTR(pipe(err_pipe_)) ||
      HANDLE_EINTR(pipe(in_pipe_))) {
    CloseAllPipes();
    return false;
  }

  process_launched_ = LaunchProcess(command,
                                    in_pipe_[PIPE_END_READ],
                                    out_pipe_[PIPE_END_WRITE],
                                    err_pipe_[PIPE_END_WRITE],
                                    &pid_);

  if (process_launched_) {
    // We won't need these anymore. These will be used by the launched process.
    CloseFd(&(in_pipe_[PIPE_END_READ]));
    CloseFd(&(out_pipe_[PIPE_END_WRITE]));
    CloseFd(&(err_pipe_[PIPE_END_WRITE]));

    *pid = pid_;
    LOG(WARNING) << "Process launched: " << pid_;
  } else {
    CloseAllPipes();
  }
  return process_launched_;
}

bool ProcessProxy::StartWatchingOnThread(base::Thread* watch_thread,
     const ProcessOutputCallback& callback) {
  DCHECK(process_launched_);
  if (watcher_started_)
    return false;
  if (pipe(shutdown_pipe_))
    return false;

  callback_set_ = true;
  callback_ = callback;

  // This object will delete itself once watching is stopped.
  // It also takes ownership of the passed fds.
  ProcessOutputWatcher* output_watcher =
      new ProcessOutputWatcher(out_pipe_[PIPE_END_READ],
                               err_pipe_[PIPE_END_READ],
                               shutdown_pipe_[PIPE_END_READ],
                               base::Bind(&ProcessProxy::OnProcessOutput,
                                          this));

  // |watch| thread is blocked by |output_watcher| from now on.
  watch_thread->message_loop()->PostTask(FROM_HERE,
      base::Bind(&ProcessOutputWatcher::Start,
                 base::Unretained(output_watcher)));
  watcher_started_ = true;
  return true;
}

void ProcessProxy::OnProcessOutput(ProcessOutputType type,
                                   const std::string& output) {
  // We have to check if callback is set on FILE thread..
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE)) {
    content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
        base::Bind(&ProcessProxy::OnProcessOutput, this, type, output));
    return;
  }

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
  return file_util::WriteFileDescriptor(shutdown_pipe_[PIPE_END_WRITE],
                                        message, sizeof(message));
}

void ProcessProxy::Close() {
  if (!process_launched_)
    return;

  process_launched_ = false;
  callback_set_ = false;

  // Wait to ensure process dies before we call StopWatching and close read
  // end of the pipe the process writes to.
  base::KillProcess(pid_, 0, true /* wait */);

  // TODO(tbarzic): What if this fails?
  StopWatching();

  // Close all fds owned by us that may still be opened. If wather had been
  // started, it took ownership of some fds.
  if (!watcher_started_) {
    CloseAllPipes();
  } else {
    CloseUsedWriteFds();
  }
}

bool ProcessProxy::Write(const std::string& text) {
  if (!process_launched_)
    return false;

  // We don't want to write '\0' to the pipe.
  size_t data_size = text.length() * sizeof(*text.c_str());
  int bytes_written =
      file_util::WriteFileDescriptor(in_pipe_[PIPE_END_WRITE],
                                     text.c_str(), data_size);
  return (bytes_written == static_cast<int>(data_size));
}

ProcessProxy::~ProcessProxy() {
  // In case watcher did not started, we may get deleted without calling Close.
  // In that case we have to clean up created pipes. If watcher had been
  // started, there will be a callback with our reference owned by
  // process_output_watcher until Close is called, so we know Close has been
  // called  by now (and pipes have been cleaned).
  if (!watcher_started_)
    CloseAllPipes();
}

bool ProcessProxy::LaunchProcess(const std::string& command,
                                 int in_fd, int out_fd, int err_fd,
                                 pid_t *pid) {
  // Redirect crosh  process' output and input so we can read it.
  base::file_handle_mapping_vector fds_mapping;
  fds_mapping.push_back(std::make_pair(in_fd, STDIN_FILENO));
  fds_mapping.push_back(std::make_pair(out_fd, STDOUT_FILENO));
  fds_mapping.push_back(std::make_pair(err_fd, STDERR_FILENO));
  base::LaunchOptions options;
  options.fds_to_remap = &fds_mapping;

  // Launch the process.
  return base::LaunchProcess(CommandLine(FilePath(command)), options, pid);
}

void ProcessProxy::CloseAllPipes() {
  ClosePipe(in_pipe_);
  ClosePipe(out_pipe_);
  ClosePipe(err_pipe_);
  ClosePipe(shutdown_pipe_);
}

void ProcessProxy::ClosePipe(int* pipe) {
  CloseFd(&(pipe[PIPE_END_READ]));
  CloseFd(&(pipe[PIPE_END_WRITE]));
}

void ProcessProxy::CloseUsedWriteFds() {
  CloseFd(&(in_pipe_[PIPE_END_WRITE]));
  CloseFd(&(shutdown_pipe_[PIPE_END_WRITE]));
}

void ProcessProxy::CloseFd(int* fd) {
  if (*fd != kInvalidFd) {
    if (HANDLE_EINTR(close(*fd)) != 0)
      DPLOG(WARNING) << "close fd failed.";
  }
  *fd = kInvalidFd;
}

void ProcessProxy::ClearAllPipes() {
  ClearPipe(in_pipe_);
  ClearPipe(out_pipe_);
  ClearPipe(err_pipe_);
  ClearPipe(shutdown_pipe_);
}

void ProcessProxy::ClearPipe(int* pipe) {
  pipe[PIPE_END_READ] = kInvalidFd;
  pipe[PIPE_END_WRITE] = kInvalidFd;
}
