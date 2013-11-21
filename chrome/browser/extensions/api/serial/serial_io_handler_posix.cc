// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_io_handler_posix.h"

#include "base/posix/eintr_wrapper.h"

namespace extensions {

// static
scoped_refptr<SerialIoHandler> SerialIoHandler::Create() {
  return new SerialIoHandlerPosix();
}

void SerialIoHandlerPosix::ReadImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(pending_read_buffer());
  DCHECK_NE(file(), base::kInvalidPlatformFileValue);

  EnsureWatchingReads();
}

void SerialIoHandlerPosix::WriteImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(pending_write_buffer());
  DCHECK_NE(file(), base::kInvalidPlatformFileValue);

  EnsureWatchingWrites();
}

void SerialIoHandlerPosix::CancelReadImpl() {
  DCHECK(CalledOnValidThread());
  is_watching_reads_ = false;
  file_read_watcher_.StopWatchingFileDescriptor();
}

void SerialIoHandlerPosix::CancelWriteImpl() {
  DCHECK(CalledOnValidThread());
  is_watching_writes_ = false;
  file_write_watcher_.StopWatchingFileDescriptor();
}

SerialIoHandlerPosix::SerialIoHandlerPosix()
    : is_watching_reads_(false),
      is_watching_writes_(false) {
}

SerialIoHandlerPosix::~SerialIoHandlerPosix() {}

void SerialIoHandlerPosix::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(fd, file());

  if (pending_read_buffer()) {
    int bytes_read = HANDLE_EINTR(read(file(),
                                       pending_read_buffer()->data(),
                                       pending_read_buffer_len()));
    if (bytes_read < 0) {
      if (errno == ENXIO) {
        ReadCompleted(0, api::serial::RECEIVE_ERROR_DEVICE_LOST);
      } else {
        ReadCompleted(0, api::serial::RECEIVE_ERROR_SYSTEM_ERROR);
      }
    } else if (bytes_read == 0) {
      ReadCompleted(0, api::serial::RECEIVE_ERROR_DEVICE_LOST);
    } else {
      ReadCompleted(bytes_read, api::serial::RECEIVE_ERROR_NONE);
    }
  } else {
    // Stop watching the fd if we get notifications with no pending
    // reads or writes to avoid starving the message loop.
    is_watching_reads_ = false;
    file_read_watcher_.StopWatchingFileDescriptor();
  }
}

void SerialIoHandlerPosix::OnFileCanWriteWithoutBlocking(int fd) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(fd, file());

  if (pending_write_buffer()) {
    int bytes_written = HANDLE_EINTR(write(file(),
                                           pending_write_buffer()->data(),
                                           pending_write_buffer_len()));
    if (bytes_written < 0) {
      WriteCompleted(0, api::serial::SEND_ERROR_SYSTEM_ERROR);
    } else {
      WriteCompleted(bytes_written, api::serial::SEND_ERROR_NONE);
    }
  } else {
    // Stop watching the fd if we get notifications with no pending
    // writes to avoid starving the message loop.
    is_watching_writes_ = false;
    file_write_watcher_.StopWatchingFileDescriptor();
  }
}

void SerialIoHandlerPosix::EnsureWatchingReads() {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(file(), base::kInvalidPlatformFileValue);
  if (!is_watching_reads_) {
    is_watching_reads_ = base::MessageLoopForIO::current()->WatchFileDescriptor(
        file(), true, base::MessageLoopForIO::WATCH_READ,
        &file_read_watcher_, this);
  }
}

void SerialIoHandlerPosix::EnsureWatchingWrites() {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(file(), base::kInvalidPlatformFileValue);
  if (!is_watching_writes_) {
    is_watching_writes_ =
        base::MessageLoopForIO::current()->WatchFileDescriptor(
            file(), true, base::MessageLoopForIO::WATCH_WRITE,
            &file_write_watcher_, this);
  }
}

}  // namespace extensions
