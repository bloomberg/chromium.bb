// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <string>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/platform_file.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/api/serial/serial_port_enumerator.h"
#include "chrome/common/extensions/api/serial.h"

namespace extensions {

namespace {

const int kDefaultBufferSize = 4096;

}

static base::LazyInstance<ProfileKeyedAPIFactory<
    ApiResourceManager<SerialConnection> > >
        g_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
ProfileKeyedAPIFactory<ApiResourceManager<SerialConnection> >*
ApiResourceManager<SerialConnection>::GetFactoryInstance() {
  return &g_factory.Get();
}

SerialConnection::SerialConnection(const std::string& port,
                                   const std::string& owner_extension_id)
    : ApiResource(owner_extension_id),
      port_(port),
      file_(base::kInvalidPlatformFileValue),
      persistent_(false),
      buffer_size_(kDefaultBufferSize),
      receive_timeout_(0),
      send_timeout_(0),
      paused_(false),
      io_handler_(SerialIoHandler::Create()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

SerialConnection::~SerialConnection() {
  DCHECK(open_complete_.is_null());
  io_handler_->CancelRead(api::serial::RECEIVE_ERROR_DISCONNECTED);
  io_handler_->CancelWrite(api::serial::SEND_ERROR_DISCONNECTED);
  Close();
}

bool SerialConnection::IsPersistent() const {
  return persistent();
}

void SerialConnection::set_buffer_size(int buffer_size) {
  buffer_size_ = buffer_size;
}

void SerialConnection::set_receive_timeout(int receive_timeout) {
  receive_timeout_ = receive_timeout;
}

void SerialConnection::set_send_timeout(int send_timeout) {
  send_timeout_ = send_timeout;
}

void SerialConnection::set_paused(bool paused) {
  paused_ = paused;
  if (paused) {
    io_handler_->CancelRead(api::serial::RECEIVE_ERROR_NONE);
  }
}

void SerialConnection::Open(const OpenCompleteCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(open_complete_.is_null());
  open_complete_ = callback;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&SerialConnection::StartOpen, base::Unretained(this)));
}

void SerialConnection::Close() {
  DCHECK(open_complete_.is_null());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (file_ != base::kInvalidPlatformFileValue) {
    base::PlatformFile file = file_;
    file_ = base::kInvalidPlatformFileValue;
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(&SerialConnection::DoClose, file));
  }
}

bool SerialConnection::Receive(const ReceiveCompleteCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!receive_complete_.is_null())
    return false;
  receive_complete_ = callback;
  io_handler_->Read(buffer_size_);
  receive_timeout_task_.reset();
  if (receive_timeout_ > 0) {
    receive_timeout_task_.reset(new TimeoutTask(
        base::Bind(&SerialConnection::OnReceiveTimeout, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(receive_timeout_)));
  }
  return true;
}

bool SerialConnection::Send(const std::string& data,
                            const SendCompleteCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!send_complete_.is_null())
    return false;
  send_complete_ = callback;
  io_handler_->Write(data);
  send_timeout_task_.reset();
  if (send_timeout_ > 0) {
    send_timeout_task_.reset(new TimeoutTask(
        base::Bind(&SerialConnection::OnSendTimeout, AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(send_timeout_)));
  }
  return true;
}

bool SerialConnection::Configure(
    const api::serial::ConnectionOptions& options) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (options.persistent.get())
    set_persistent(*options.persistent);
  if (options.name.get())
    set_name(*options.name);
  if (options.buffer_size.get())
    set_buffer_size(*options.buffer_size);
  if (options.receive_timeout.get())
    set_receive_timeout(*options.receive_timeout);
  if (options.send_timeout.get())
    set_send_timeout(*options.send_timeout);
  bool success = ConfigurePort(options);
  io_handler_->CancelRead(api::serial::RECEIVE_ERROR_NONE);
  return success;
}

void SerialConnection::SetIoHandlerForTest(
    scoped_refptr<SerialIoHandler> handler) {
  io_handler_ = handler;
}

bool SerialConnection::GetInfo(api::serial::ConnectionInfo* info) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  info->paused = paused_;
  info->persistent = persistent_;
  info->name = name_;
  info->buffer_size = buffer_size_;
  info->receive_timeout = receive_timeout_;
  info->send_timeout = send_timeout_;
  return GetPortInfo(info);
}

void SerialConnection::StartOpen() {
  DCHECK(!open_complete_.is_null());
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK_EQ(file_, base::kInvalidPlatformFileValue);
  const SerialPortEnumerator::StringSet name_set(
    SerialPortEnumerator::GenerateValidSerialPortNames());
  base::PlatformFile file = base::kInvalidPlatformFileValue;
  if (SerialPortEnumerator::DoesPortExist(name_set, port_)) {
    // It's the responsibility of the API wrapper around SerialConnection to
    // validate the supplied path against the set of valid port names, and
    // it is a reasonable assumption that serial port names are ASCII.
    DCHECK(IsStringASCII(port_));
    base::FilePath path(
        base::FilePath::FromUTF8Unsafe(MaybeFixUpPortName(port_)));
    int flags = base::PLATFORM_FILE_OPEN |
                base::PLATFORM_FILE_READ |
                base::PLATFORM_FILE_EXCLUSIVE_READ |
                base::PLATFORM_FILE_WRITE |
                base::PLATFORM_FILE_EXCLUSIVE_WRITE |
                base::PLATFORM_FILE_ASYNC |
                base::PLATFORM_FILE_TERMINAL_DEVICE;
    file = base::CreatePlatformFile(path, flags, NULL, NULL);
  }
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SerialConnection::FinishOpen, base::Unretained(this), file));
}

void SerialConnection::FinishOpen(base::PlatformFile file) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!open_complete_.is_null());
  DCHECK_EQ(file_, base::kInvalidPlatformFileValue);
  OpenCompleteCallback callback = open_complete_;
  open_complete_.Reset();

  if (file == base::kInvalidPlatformFileValue) {
    callback.Run(false);
    return;
  }

  file_ = file;
  io_handler_->Initialize(
      file_,
      base::Bind(&SerialConnection::OnAsyncReadComplete, AsWeakPtr()),
      base::Bind(&SerialConnection::OnAsyncWriteComplete, AsWeakPtr()));

  bool success = PostOpen();
  if (!success) {
    Close();
  }

  callback.Run(success);
}

// static
void SerialConnection::DoClose(base::PlatformFile port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (port != base::kInvalidPlatformFileValue) {
    base::ClosePlatformFile(port);
  }
}

void SerialConnection::OnReceiveTimeout() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  io_handler_->CancelRead(api::serial::RECEIVE_ERROR_TIMEOUT);
}

void SerialConnection::OnSendTimeout() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  io_handler_->CancelWrite(api::serial::SEND_ERROR_TIMEOUT);
}

void SerialConnection::OnAsyncReadComplete(const std::string& data,
                                           api::serial::ReceiveError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!receive_complete_.is_null());
  ReceiveCompleteCallback callback = receive_complete_;
  receive_complete_.Reset();
  receive_timeout_task_.reset();
  callback.Run(data, error);
}

void SerialConnection::OnAsyncWriteComplete(int bytes_sent,
                                            api::serial::SendError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!send_complete_.is_null());
  SendCompleteCallback callback = send_complete_;
  send_complete_.Reset();
  send_timeout_task_.reset();
  callback.Run(bytes_sent, error);
}

SerialConnection::TimeoutTask::TimeoutTask(const base::Closure& closure,
                                           const base::TimeDelta& delay)
    : weak_factory_(this),
      closure_(closure),
      delay_(delay) {
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&TimeoutTask::Run, weak_factory_.GetWeakPtr()),
      delay_);
}

SerialConnection::TimeoutTask::~TimeoutTask() {}

void SerialConnection::TimeoutTask::Run() const {
  closure_.Run();
}

}  // namespace extensions
