// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_io_handler.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"

namespace extensions {

SerialIoHandler::SerialIoHandler()
    : file_(base::kInvalidPlatformFileValue),
      pending_read_buffer_len_(0),
      pending_write_buffer_len_(0) {
}

SerialIoHandler::~SerialIoHandler() {
  DCHECK(CalledOnValidThread());
}

void SerialIoHandler::Initialize(base::PlatformFile file,
                                 const ReadCompleteCallback& read_callback,
                                 const WriteCompleteCallback& write_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(file_, base::kInvalidPlatformFileValue);

  file_ = file;
  read_complete_ = read_callback;
  write_complete_ = write_callback;

  InitializeImpl();
}

void SerialIoHandler::Read(int max_bytes) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsReadPending());
  pending_read_buffer_ = new net::IOBuffer(max_bytes);
  pending_read_buffer_len_ = max_bytes;
  read_canceled_ = false;
  AddRef();
  ReadImpl();
}

void SerialIoHandler::Write(const std::string& data) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsWritePending());
  pending_write_buffer_ = new net::IOBuffer(data.length());
  pending_write_buffer_len_ = data.length();
  memcpy(pending_write_buffer_->data(), data.data(), pending_write_buffer_len_);
  write_canceled_ = false;
  AddRef();
  WriteImpl();
}

void SerialIoHandler::ReadCompleted(int bytes_read,
                                    api::serial::ReceiveError error) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsReadPending());
  read_complete_.Run(std::string(pending_read_buffer_->data(), bytes_read),
                     error);
  pending_read_buffer_ = NULL;
  pending_read_buffer_len_ = 0;
  Release();
}

void SerialIoHandler::WriteCompleted(int bytes_written,
                                     api::serial::SendError error) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsWritePending());
  write_complete_.Run(bytes_written, error);
  pending_write_buffer_ = NULL;
  pending_write_buffer_len_ = 0;
  Release();
}

bool SerialIoHandler::IsReadPending() const {
  DCHECK(CalledOnValidThread());
  return pending_read_buffer_ != NULL;
}

bool SerialIoHandler::IsWritePending() const {
  DCHECK(CalledOnValidThread());
  return pending_write_buffer_ != NULL;
}

void SerialIoHandler::CancelRead(api::serial::ReceiveError reason) {
  DCHECK(CalledOnValidThread());
  if (IsReadPending()) {
    read_canceled_ = true;
    read_cancel_reason_ = reason;
    CancelReadImpl();
  }
}

void SerialIoHandler::CancelWrite(api::serial::SendError reason) {
  DCHECK(CalledOnValidThread());
  if (IsWritePending()) {
    write_canceled_ = true;
    write_cancel_reason_ = reason;
    CancelWriteImpl();
  }
}

void SerialIoHandler::QueueReadCompleted(int bytes_read,
                                         api::serial::ReceiveError error) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&SerialIoHandler::ReadCompleted, this,
                            bytes_read, error));
}

void SerialIoHandler::QueueWriteCompleted(int bytes_written,
                                          api::serial::SendError error) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&SerialIoHandler::WriteCompleted, this,
                            bytes_written, error));
}

}  // namespace extensions

