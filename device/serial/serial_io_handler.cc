// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_io_handler.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"

namespace device {

SerialIoHandler::SerialIoHandler(
    scoped_refptr<base::MessageLoopProxy> file_thread_message_loop,
    scoped_refptr<base::MessageLoopProxy> ui_thread_message_loop)
    : file_thread_message_loop_(file_thread_message_loop),
      ui_thread_message_loop_(ui_thread_message_loop) {
  options_.bitrate = 9600;
  options_.data_bits = serial::DATA_BITS_EIGHT;
  options_.parity_bit = serial::PARITY_BIT_NO;
  options_.stop_bits = serial::STOP_BITS_ONE;
  options_.cts_flow_control = false;
  options_.has_cts_flow_control = true;
}

SerialIoHandler::~SerialIoHandler() {
  DCHECK(CalledOnValidThread());
  Close();
}

void SerialIoHandler::Open(const std::string& port,
                           const serial::ConnectionOptions& options,
                           const OpenCompleteCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(open_complete_.is_null());
  open_complete_ = callback;
  DCHECK(file_thread_message_loop_.get());
  DCHECK(ui_thread_message_loop_.get());
  MergeConnectionOptions(options);
  RequestAccess(port, file_thread_message_loop_, ui_thread_message_loop_);
}

void SerialIoHandler::RequestAccess(
    const std::string& port,
    scoped_refptr<base::MessageLoopProxy> file_message_loop,
    scoped_refptr<base::MessageLoopProxy> ui_message_loop) {
  OnRequestAccessComplete(port, true /* success */);
}

void SerialIoHandler::OnRequestAccessComplete(const std::string& port,
                                              bool success) {
  DCHECK(CalledOnValidThread());
  if (success) {
    DCHECK(file_thread_message_loop_.get());
    file_thread_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&SerialIoHandler::StartOpen,
                   this,
                   port,
                   base::MessageLoopProxy::current()));
    return;
  } else {
    DCHECK(!open_complete_.is_null());
    OpenCompleteCallback callback = open_complete_;
    open_complete_.Reset();
    callback.Run(false);
    return;
  }
}

void SerialIoHandler::MergeConnectionOptions(
    const serial::ConnectionOptions& options) {
  if (options.bitrate) {
    options_.bitrate = options.bitrate;
  }
  if (options.data_bits != serial::DATA_BITS_NONE) {
    options_.data_bits = options.data_bits;
  }
  if (options.parity_bit != serial::PARITY_BIT_NONE) {
    options_.parity_bit = options.parity_bit;
  }
  if (options.stop_bits != serial::STOP_BITS_NONE) {
    options_.stop_bits = options.stop_bits;
  }
  if (options.has_cts_flow_control) {
    DCHECK(options_.has_cts_flow_control);
    options_.cts_flow_control = options.cts_flow_control;
  }
}

void SerialIoHandler::StartOpen(
    const std::string& port,
    scoped_refptr<base::MessageLoopProxy> io_message_loop) {
  DCHECK(!open_complete_.is_null());
  DCHECK(file_thread_message_loop_->RunsTasksOnCurrentThread());
  DCHECK(!file_.IsValid());
  // It's the responsibility of the API wrapper around SerialIoHandler to
  // validate the supplied path against the set of valid port names, and
  // it is a reasonable assumption that serial port names are ASCII.
  DCHECK(base::IsStringASCII(port));
  base::FilePath path(base::FilePath::FromUTF8Unsafe(MaybeFixUpPortName(port)));
  int flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
              base::File::FLAG_EXCLUSIVE_READ | base::File::FLAG_WRITE |
              base::File::FLAG_EXCLUSIVE_WRITE | base::File::FLAG_ASYNC |
              base::File::FLAG_TERMINAL_DEVICE;
  base::File file(path, flags);
  io_message_loop->PostTask(
      FROM_HERE,
      base::Bind(&SerialIoHandler::FinishOpen, this, Passed(file.Pass())));
}

void SerialIoHandler::FinishOpen(base::File file) {
  DCHECK(CalledOnValidThread());
  DCHECK(!open_complete_.is_null());
  OpenCompleteCallback callback = open_complete_;
  open_complete_.Reset();

  if (!file.IsValid()) {
    LOG(ERROR) << "Failed to open serial port: "
               << base::File::ErrorToString(file.error_details());
    callback.Run(false);
    return;
  }

  file_ = file.Pass();

  bool success = PostOpen() && ConfigurePortImpl();
  if (!success) {
    Close();
  }

  callback.Run(success);
}

bool SerialIoHandler::PostOpen() {
  return true;
}

void SerialIoHandler::Close() {
  if (file_.IsValid()) {
    DCHECK(file_thread_message_loop_.get());
    file_thread_message_loop_->PostTask(
        FROM_HERE, base::Bind(&SerialIoHandler::DoClose, Passed(file_.Pass())));
  }
}

// static
void SerialIoHandler::DoClose(base::File port) {
  // port closed by destructor.
}

void SerialIoHandler::Read(scoped_ptr<WritableBuffer> buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsReadPending());
  pending_read_buffer_ = buffer.Pass();
  read_canceled_ = false;
  AddRef();
  ReadImpl();
}

void SerialIoHandler::Write(scoped_ptr<ReadOnlyBuffer> buffer) {
  DCHECK(CalledOnValidThread());
  DCHECK(!IsWritePending());
  pending_write_buffer_ = buffer.Pass();
  write_canceled_ = false;
  AddRef();
  WriteImpl();
}

void SerialIoHandler::ReadCompleted(int bytes_read,
                                    serial::ReceiveError error) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsReadPending());
  scoped_ptr<WritableBuffer> pending_read_buffer = pending_read_buffer_.Pass();
  if (error == serial::RECEIVE_ERROR_NONE) {
    pending_read_buffer->Done(bytes_read);
  } else {
    pending_read_buffer->DoneWithError(bytes_read, error);
  }
  Release();
}

void SerialIoHandler::WriteCompleted(int bytes_written,
                                     serial::SendError error) {
  DCHECK(CalledOnValidThread());
  DCHECK(IsWritePending());
  scoped_ptr<ReadOnlyBuffer> pending_write_buffer =
      pending_write_buffer_.Pass();
  if (error == serial::SEND_ERROR_NONE) {
    pending_write_buffer->Done(bytes_written);
  } else {
    pending_write_buffer->DoneWithError(bytes_written, error);
  }
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

void SerialIoHandler::CancelRead(serial::ReceiveError reason) {
  DCHECK(CalledOnValidThread());
  if (IsReadPending() && !read_canceled_) {
    read_canceled_ = true;
    read_cancel_reason_ = reason;
    CancelReadImpl();
  }
}

void SerialIoHandler::CancelWrite(serial::SendError reason) {
  DCHECK(CalledOnValidThread());
  if (IsWritePending() && !write_canceled_) {
    write_canceled_ = true;
    write_cancel_reason_ = reason;
    CancelWriteImpl();
  }
}

bool SerialIoHandler::ConfigurePort(const serial::ConnectionOptions& options) {
  MergeConnectionOptions(options);
  return ConfigurePortImpl();
}

void SerialIoHandler::QueueReadCompleted(int bytes_read,
                                         serial::ReceiveError error) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&SerialIoHandler::ReadCompleted, this, bytes_read, error));
}

void SerialIoHandler::QueueWriteCompleted(int bytes_written,
                                          serial::SendError error) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&SerialIoHandler::WriteCompleted, this, bytes_written, error));
}

}  // namespace device
