// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/process_proxy/process_output_watcher.h"

#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/third_party/icu/icu_utf.h"

namespace {

void InitReadFdSet(int out_fd, int stop_fd, fd_set* set) {
  FD_ZERO(set);
  if (out_fd != -1)
    FD_SET(out_fd, set);
  FD_SET(stop_fd, set);
}

void CloseFd(int* fd) {
  if (*fd >= 0) {
    if (IGNORE_EINTR(close(*fd)) != 0)
      DPLOG(WARNING) << "close fd " << *fd << " failed.";
  }
  *fd = -1;
}

// Gets byte size for a UTF8 character given it's leading byte. The character
// size is encoded as number of leading '1' bits in the character's leading
// byte. If the most significant bit is '0', the character is a valid ASCII
// and it's byte size is 1.
// The method returns 1 if the provided byte is invalid leading byte.
size_t UTF8SizeFromLeadingByte(uint8 leading_byte) {
  size_t byte_count = 0;
  uint8 mask = 1 << 7;
  uint8 error_mask = 1 << (7 - CBU8_MAX_LENGTH);
  while (leading_byte & mask) {
    if (mask & error_mask)
      return 1;
    mask >>= 1;
    ++byte_count;
  }
  return byte_count ? byte_count : 1;
}

}  // namespace

namespace chromeos {

ProcessOutputWatcher::ProcessOutputWatcher(
    int out_fd,
    int stop_fd,
    const ProcessOutputCallback& callback)
    : read_buffer_size_(0),
      out_fd_(out_fd),
      stop_fd_(stop_fd),
      on_read_callback_(callback) {
  VerifyFileDescriptor(out_fd_);
  VerifyFileDescriptor(stop_fd_);
  max_fd_ = std::max(out_fd_, stop_fd_);
  // We want to be sure we will be able to add 0 at the end of the input, so -1.
  read_buffer_capacity_ = arraysize(read_buffer_) - 1;
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
    DCHECK_GE(stop_fd_, 0);
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
  DCHECK_GT(read_buffer_capacity_, read_buffer_size_);
  ssize_t bytes_read =
      HANDLE_EINTR(read(*fd,
                        &read_buffer_[read_buffer_size_],
                        read_buffer_capacity_ - read_buffer_size_));
  if (bytes_read < 0)
    DPLOG(WARNING) << "read from buffer failed";

  if (bytes_read > 0)
    ReportOutput(type, bytes_read);

  // If there is nothing on the output the watched process has exited (slave end
  // of pty is closed).
  if (bytes_read <= 0) {
    // Slave pseudo terminal has been closed, we won't need master fd anymore.
    CloseFd(fd);

    // We have lost contact with the process, so report it.
    on_read_callback_.Run(PROCESS_OUTPUT_TYPE_EXIT, "");
  }
}

size_t ProcessOutputWatcher::OutputSizeWithoutIncompleteUTF8() {
  // Find the last non-trailing character byte. This byte should be used to
  // infer the last UTF8 character length.
  int last_lead_byte = read_buffer_size_ - 1;
  while (true) {
    // If the series of trailing bytes is too long, something's not right.
    // Report the whole output, without waiting for further character bytes.
    if (read_buffer_size_ - last_lead_byte > CBU8_MAX_LENGTH)
      return read_buffer_size_;

    // If there are trailing characters, there must be a leading one in the
    // buffer for a valid UTF8 character. Getting past the buffer begining
    // signals something's wrong, or the buffer is empty. In both cases return
    // the whole current buffer.
    if (last_lead_byte < 0)
      return read_buffer_size_;

    // Found the starting character byte; stop searching.
    if (!CBU8_IS_TRAIL(read_buffer_[last_lead_byte]))
      break;

    --last_lead_byte;
  }

  size_t last_length = UTF8SizeFromLeadingByte(read_buffer_[last_lead_byte]);

  // Note that if |last_length| == 0 or
  // |last_length| + |last_read_byte| < |read_buffer_size_|, the string is
  // invalid UTF8. In that case, send the whole read buffer to the observer
  // immediately, just as if there is no trailing incomplete UTF8 bytes.
  if (!last_length || last_length + last_lead_byte <= read_buffer_size_)
    return read_buffer_size_;

  return last_lead_byte;
}

void ProcessOutputWatcher::ReportOutput(ProcessOutputType type,
                                        size_t new_bytes_count) {
  read_buffer_size_ += new_bytes_count;
  size_t output_to_report = OutputSizeWithoutIncompleteUTF8();

  on_read_callback_.Run(type, std::string(read_buffer_, output_to_report));

  // Move the bytes that were left behind to the beginning of the buffer and
  // update the buffer size accordingly.
  if (output_to_report < read_buffer_size_) {
    for (size_t i = output_to_report; i < read_buffer_size_; ++i) {
      read_buffer_[i - output_to_report] = read_buffer_[i];
    }
  }
  read_buffer_size_ -= output_to_report;
}

void ProcessOutputWatcher::OnStop() {
  delete this;
}

}  // namespace chromeos
