// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/process_proxy/process_output_watcher.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace {

void InitReadFdSet(int out_fd, int stop_fd, fd_set* set) {
  FD_ZERO(set);
  if (out_fd != -1)
    FD_SET(out_fd, set);
  FD_SET(stop_fd, set);
}

void CloseFd(int* fd) {
  if (*fd >= 0) {
    if (HANDLE_EINTR(close(*fd)) != 0)
      DPLOG(WARNING) << "close fd " << *fd << " failed.";
  }
  *fd = -1;
}

}  // namespace

namespace chromeos {

ProcessOutputWatcher::ProcessOutputWatcher(int out_fd, int stop_fd,
    const ProcessOutputCallback& callback)
    : out_fd_(out_fd),
      stop_fd_(stop_fd),
      on_read_callback_(callback)  {
  VerifyFileDescriptor(out_fd_);
  VerifyFileDescriptor(stop_fd_);
  max_fd_ = std::max(out_fd_, stop_fd_);
  // We want to be sure we will be able to add 0 at the end of the input, so -1.
  read_buffer_size_ = arraysize(read_buffer_) - 1;
}

void ProcessOutputWatcher::Start() {
  WatchProcessOutput();
  OnStop();
}

ProcessOutputWatcher::~ProcessOutputWatcher() {
  CloseFd(&out_fd_);
  CloseFd(&stop_fd_);
}

void ProcessOutputWatcher::WatchProcessOutput() {
  while (true) {
    // This has to be reset with every watch cycle.
    fd_set rfds;
    DCHECK(stop_fd_ >= 0);
    InitReadFdSet(out_fd_, stop_fd_, &rfds);

    int select_result =
        HANDLE_EINTR(select(max_fd_ + 1, &rfds, NULL, NULL, NULL));

    if (select_result < 0) {
      DPLOG(WARNING) << "select failed";
      return;
    }

    // Check if we were stopped.
    if (FD_ISSET(stop_fd_, &rfds)) {
      return;
    }

    if (out_fd_ != -1 && FD_ISSET(out_fd_, &rfds)) {
      ReadFromFd(PROCESS_OUTPUT_TYPE_OUT, &out_fd_);
    }
  }
}

void ProcessOutputWatcher::VerifyFileDescriptor(int fd) {
  CHECK_LE(0, fd);
  CHECK_GT(FD_SETSIZE, fd);
}

void ProcessOutputWatcher::ReadFromFd(ProcessOutputType type, int* fd) {
  // We don't want to necessary read pipe until it is empty so we don't starve
  // other streams in case data is written faster than we read it. If there is
  // more than read_buffer_size_ bytes in pipe, it will be read in the next
  // iteration.
  ssize_t bytes_read = HANDLE_EINTR(read(*fd, read_buffer_, read_buffer_size_));
  if (bytes_read < 0)
    DPLOG(WARNING) << "read from buffer failed";

  if (bytes_read > 0) {
    on_read_callback_.Run(type, std::string(read_buffer_, bytes_read));
  }

  // If there is nothing on the output the watched process has exited (slave end
  // of pty is closed).
  if (bytes_read <= 0) {
    // Slave pseudo terminal has been closed, we won't need master fd anymore.
    CloseFd(fd);

    // We have lost contact with the process, so report it.
    on_read_callback_.Run(PROCESS_OUTPUT_TYPE_EXIT, "");
  }
}

void ProcessOutputWatcher::OnStop() {
  delete this;
}

}  // namespace chromeos
