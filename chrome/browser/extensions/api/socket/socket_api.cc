// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket_api.h"

#include "base/bind.h"
#include "chrome/browser/extensions/api/api_resource_controller.h"
#include "chrome/browser/extensions/api/socket/socket.h"
#include "chrome/browser/extensions/api/socket/tcp_socket.h"
#include "chrome/browser/extensions/api/socket/udp_socket.h"
#include "chrome/browser/extensions/extension_service.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"

namespace extensions {

const char kAddressKey[] = "address";
const char kPortKey[] = "port";
const char kBytesWrittenKey[] = "bytesWritten";
const char kDataKey[] = "data";
const char kResultCodeKey[] = "resultCode";
const char kSocketIdKey[] = "socketId";
const char kTCPOption[] = "tcp";
const char kUDPOption[] = "udp";

const char kSocketNotFoundError[] = "Socket not found";
const char kSocketTypeInvalidError[] = "Socket type is not supported";

void SocketExtensionFunction::Work() {
}

bool SocketExtensionFunction::Respond() {
  return error_.empty();
}

SocketCreateFunction::SocketCreateFunction()
    : socket_type_(kSocketTypeInvalid),
      src_id_(-1),
      event_notifier_(NULL) {
}

SocketCreateFunction::~SocketCreateFunction() {}

bool SocketCreateFunction::Prepare() {
  std::string socket_type_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &socket_type_string));
  if (socket_type_string == kTCPOption) {
    socket_type_ = kSocketTypeTCP;
  } else if (socket_type_string == kUDPOption) {
    socket_type_ = kSocketTypeUDP;
  } else {
    error_ = kSocketTypeInvalidError;
    return false;
  }

  src_id_ = ExtractSrcId(1);
  event_notifier_ = CreateEventNotifier(src_id_);

  return true;
}

void SocketCreateFunction::Work() {
  Socket* socket = NULL;
  if (socket_type_ == kSocketTypeTCP) {
    socket = new TCPSocket(event_notifier_);
  } else if (socket_type_== kSocketTypeUDP) {
    socket = new UDPSocket(event_notifier_);
  }
  DCHECK(socket);

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kSocketIdKey, controller()->AddAPIResource(socket));
  result_.reset(result);
}

bool SocketDestroyFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketDestroyFunction::Work() {
  if (!controller()->RemoveAPIResource(socket_id_))
    error_ = kSocketNotFoundError;
}

bool SocketConnectFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &address_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(2, &port_));
  return true;
}

void SocketConnectFunction::AsyncWorkStart() {
  Socket* socket = controller()->GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1);
    return;
  }

  socket->Connect(address_, port_,
      base::Bind(&SocketConnectFunction::OnCompleted, this));
}

void SocketConnectFunction::OnCompleted(int result) {
  result_.reset(Value::CreateIntegerValue(result));
  AsyncWorkCompleted();
}

bool SocketDisconnectFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketDisconnectFunction::Work() {
  Socket* socket = controller()->GetSocket(socket_id_);
  if (socket)
    socket->Disconnect();
  else
    error_ = kSocketNotFoundError;
  result_.reset(Value::CreateNullValue());
}

bool SocketBindFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &address_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(2, &port_));
  return true;
}

void SocketBindFunction::Work() {
  int result = -1;
  Socket* socket = controller()->GetSocket(socket_id_);
  if (socket)
    result = socket->Bind(address_, port_);
  else
    error_ = kSocketNotFoundError;

  result_.reset(Value::CreateIntegerValue(result));
}

SocketReadFunction::SocketReadFunction()
  : params_(NULL) {
}

SocketReadFunction::~SocketReadFunction() {}

bool SocketReadFunction::Prepare() {
  params_ = api::experimental_socket::Read::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketReadFunction::AsyncWorkStart() {
  Socket* socket = controller()->GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1, NULL);
    return;
  }

  socket->Read(params_->buffer_size.get() ? *params_->buffer_size.get() : 4096,
      base::Bind(&SocketReadFunction::OnCompleted, this));
}

void SocketReadFunction::OnCompleted(int bytes_read,
                                     scoped_refptr<net::IOBuffer> io_buffer) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kResultCodeKey, bytes_read);
  if (bytes_read > 0) {
    result->Set(kDataKey,
        base::BinaryValue::CreateWithCopiedBuffer(io_buffer->data(),
            bytes_read));
  } else {
    // BinaryValue does not support NULL buffer. Workaround it with new char[1].
    // http://crbug.com/127630
    result->Set(kDataKey, base::BinaryValue::Create(new char[1], 0));
  }
  result_.reset(result);

  AsyncWorkCompleted();
}

SocketWriteFunction::SocketWriteFunction()
    : socket_id_(0),
      io_buffer_(NULL),
      io_buffer_size_(0) {
}

SocketWriteFunction::~SocketWriteFunction() {}

bool SocketWriteFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  base::BinaryValue *data = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBinary(1, &data));

  io_buffer_size_ = data->GetSize();
  io_buffer_ = new net::WrappedIOBuffer(data->GetBuffer());
  return true;
}

void SocketWriteFunction::AsyncWorkStart() {
  Socket* socket = controller()->GetSocket(socket_id_);

  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1);
    return;
  }

  socket->Write(io_buffer_, io_buffer_size_,
      base::Bind(&SocketWriteFunction::OnCompleted, this));
}

void SocketWriteFunction::OnCompleted(int bytes_written) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  result_.reset(result);

  AsyncWorkCompleted();
}

SocketRecvFromFunction::SocketRecvFromFunction()
  : params_(NULL) {
}

SocketRecvFromFunction::~SocketRecvFromFunction() {}

bool SocketRecvFromFunction::Prepare() {
  params_ = api::experimental_socket::RecvFrom::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketRecvFromFunction::AsyncWorkStart() {
  Socket* socket = controller()->GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1, NULL, std::string(), 0);
    return;
  }

  socket->RecvFrom(params_->buffer_size.get() ? *params_->buffer_size : 4096,
      base::Bind(&SocketRecvFromFunction::OnCompleted, this));
}

void SocketRecvFromFunction::OnCompleted(int bytes_read,
                                         scoped_refptr<net::IOBuffer> io_buffer,
                                         const std::string& address,
                                         int port) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kResultCodeKey, bytes_read);
  if (bytes_read > 0) {
    result->Set(kDataKey,
        base::BinaryValue::CreateWithCopiedBuffer(io_buffer->data(),
            bytes_read));
  } else {
    // BinaryValue does not support NULL buffer. Workaround it with new char[1].
    // http://crbug.com/127630
    result->Set(kDataKey, base::BinaryValue::Create(new char[1], 0));
  }
  result->SetString(kAddressKey, address);
  result->SetInteger(kPortKey, port);
  result_.reset(result);

  AsyncWorkCompleted();
}

SocketSendToFunction::SocketSendToFunction()
    : socket_id_(0),
      io_buffer_(NULL),
      io_buffer_size_(0) {
}

SocketSendToFunction::~SocketSendToFunction() {}

bool SocketSendToFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  base::BinaryValue *data = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetBinary(1, &data));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &address_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(3, &port_));

  io_buffer_size_ = data->GetSize();
  io_buffer_ = new net::WrappedIOBuffer(data->GetBuffer());
  return true;
}

void SocketSendToFunction::AsyncWorkStart() {
  Socket* socket = controller()->GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1);
    return;
  }

  socket->SendTo(io_buffer_, io_buffer_size_, address_, port_,
      base::Bind(&SocketSendToFunction::OnCompleted, this));
}

void SocketSendToFunction::OnCompleted(int bytes_written) {
  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  result_.reset(result);

  AsyncWorkCompleted();
}

}  // namespace extensions
