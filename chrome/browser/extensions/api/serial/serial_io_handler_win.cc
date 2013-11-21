// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_io_handler_win.h"

#include "base/win/windows_version.h"

#include <windows.h>

namespace extensions {

// static
scoped_refptr<SerialIoHandler> SerialIoHandler::Create() {
  return new SerialIoHandlerWin();
}

void SerialIoHandlerWin::InitializeImpl() {
  DCHECK(!comm_context_);
  DCHECK(!read_context_);
  DCHECK(!write_context_);

  base::MessageLoopForIO::current()->RegisterIOHandler(file(), this);

  comm_context_.reset(new base::MessageLoopForIO::IOContext());
  comm_context_->handler = this;
  memset(&comm_context_->overlapped, 0, sizeof(comm_context_->overlapped));

  read_context_.reset(new base::MessageLoopForIO::IOContext());
  read_context_->handler = this;
  memset(&read_context_->overlapped, 0, sizeof(read_context_->overlapped));

  write_context_.reset(new base::MessageLoopForIO::IOContext());
  write_context_->handler = this;
  memset(&write_context_->overlapped, 0, sizeof(write_context_->overlapped));
}

void SerialIoHandlerWin::ReadImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(pending_read_buffer());
  DCHECK_NE(file(), base::kInvalidPlatformFileValue);

  DWORD errors;
  COMSTAT status;
  if (!ClearCommError(file(), &errors, &status) || errors != 0) {
    QueueReadCompleted(0, api::serial::RECEIVE_ERROR_SYSTEM_ERROR);
    return;
  }

  SetCommMask(file(), EV_RXCHAR);

  event_mask_ = 0;
  BOOL ok = ::WaitCommEvent(file(), &event_mask_, &comm_context_->overlapped);
  if (!ok && GetLastError() != ERROR_IO_PENDING) {
    QueueReadCompleted(0, api::serial::RECEIVE_ERROR_SYSTEM_ERROR);
  }
  is_comm_pending_ = true;
}

void SerialIoHandlerWin::WriteImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK(pending_write_buffer());
  DCHECK_NE(file(), base::kInvalidPlatformFileValue);

  BOOL ok = ::WriteFile(file(),
                        pending_write_buffer()->data(),
                        pending_write_buffer_len(), NULL,
                        &write_context_->overlapped);
  if (!ok && GetLastError() != ERROR_IO_PENDING) {
    QueueWriteCompleted(0, api::serial::SEND_ERROR_SYSTEM_ERROR);
  }
}

void SerialIoHandlerWin::CancelReadImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(file(), base::kInvalidPlatformFileValue);
  ::CancelIo(file());
}

void SerialIoHandlerWin::CancelWriteImpl() {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(file(), base::kInvalidPlatformFileValue);
  ::CancelIo(file());
}

SerialIoHandlerWin::SerialIoHandlerWin()
    : event_mask_(0),
      is_comm_pending_(false) {
}

SerialIoHandlerWin::~SerialIoHandlerWin() {
}

void SerialIoHandlerWin::OnIOCompleted(
    base::MessageLoopForIO::IOContext* context,
    DWORD bytes_transferred,
    DWORD error) {
  DCHECK(CalledOnValidThread());
  if (context == comm_context_) {
    if (read_canceled()) {
      ReadCompleted(bytes_transferred, read_cancel_reason());
    } else if (error != ERROR_SUCCESS && error != ERROR_OPERATION_ABORTED) {
      ReadCompleted(0, api::serial::RECEIVE_ERROR_SYSTEM_ERROR);
    } else if (pending_read_buffer()) {
      BOOL ok = ::ReadFile(file(),
                           pending_read_buffer()->data(),
                           pending_read_buffer_len(),
                           NULL,
                           &read_context_->overlapped);
      if (!ok && GetLastError() != ERROR_IO_PENDING) {
        ReadCompleted(0, api::serial::RECEIVE_ERROR_SYSTEM_ERROR);
      }
    }
  } else if (context == read_context_) {
    if (read_canceled()) {
      ReadCompleted(bytes_transferred, read_cancel_reason());
    } else if (error != ERROR_SUCCESS && error != ERROR_OPERATION_ABORTED) {
      ReadCompleted(0, api::serial::RECEIVE_ERROR_SYSTEM_ERROR);
    } else {
      ReadCompleted(
          bytes_transferred,
          error == ERROR_SUCCESS ? api::serial::RECEIVE_ERROR_NONE
                                 : api::serial::RECEIVE_ERROR_SYSTEM_ERROR);
    }
  } else if (context == write_context_) {
    DCHECK(pending_write_buffer());
    if (write_canceled()) {
      WriteCompleted(0, write_cancel_reason());
    } else if (error != ERROR_SUCCESS && error != ERROR_OPERATION_ABORTED) {
      WriteCompleted(0, api::serial::SEND_ERROR_SYSTEM_ERROR);
    } else {
      WriteCompleted(
          bytes_transferred,
          error == ERROR_SUCCESS ? api::serial::SEND_ERROR_NONE
                                 : api::serial::SEND_ERROR_SYSTEM_ERROR);
    }
  } else {
      NOTREACHED() << "Invalid IOContext";
  }
}

}  // namespace extensions
