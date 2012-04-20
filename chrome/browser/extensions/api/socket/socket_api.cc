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

namespace extensions {

const char kBytesWrittenKey[] = "bytesWritten";
const char kDataKey[] = "data";
const char kSocketIdKey[] = "socketId";
const char kTCPOption[] = "tcp";
const char kUDPOption[] = "udp";

const char kSocketNotFoundError[] = "Socket not found";

SocketCreateFunction::SocketCreateFunction()
    : src_id_(-1), event_notifier_(NULL) {
}

bool SocketCreateFunction::Prepare() {
  std::string socket_type_string;
  size_t argument_position = 0;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(argument_position++,
                                               &socket_type_string));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(argument_position++,
                                               &address_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(argument_position++,
                                                &port_));

  if (socket_type_string == kTCPOption)
    socket_type_ = kSocketTypeTCP;
  else if (socket_type_string == kUDPOption)
    socket_type_ = kSocketTypeUDP;
  else
    return false;

  src_id_ = ExtractSrcId(argument_position);
  event_notifier_ = CreateEventNotifier(src_id_);

  return true;
}

void SocketCreateFunction::Work() {
  Socket* socket = NULL;
  if (socket_type_ == kSocketTypeTCP) {
    socket = new TCPSocket(address_, port_, event_notifier_);
  } else {
    socket = new UDPSocket(address_, port_, event_notifier_);
  }
  DCHECK(socket);
  DCHECK(socket->IsValid());

  DictionaryValue* result = new DictionaryValue();

  result->SetInteger(kSocketIdKey, controller()->AddAPIResource(socket));
  result_.reset(result);
}

bool SocketCreateFunction::Respond() {
  return true;
}

bool SocketDestroyFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketDestroyFunction::Work() {
  controller()->RemoveAPIResource(socket_id_);
}

bool SocketDestroyFunction::Respond() {
  return true;
}

bool SocketConnectFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketConnectFunction::Work() {
  int result = -1;
  Socket* socket = controller()->GetSocket(socket_id_);
  if (socket)
    result = socket->Connect();
  else
    error_ = kSocketNotFoundError;
  result_.reset(Value::CreateIntegerValue(result));
}

bool SocketConnectFunction::Respond() {
  return true;
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

bool SocketDisconnectFunction::Respond() {
  return true;
}

bool SocketReadFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketReadFunction::Work() {
  // TODO(miket): this is an arbitrary number. Can we come up with one that
  // makes sense?
  const int buffer_len = 2048;
  scoped_refptr<net::IOBuffer> io_buffer(new net::IOBuffer(buffer_len));
  Socket* socket = controller()->GetSocket(socket_id_);
  int bytes_read = -1;
  if (socket)
    bytes_read = socket->Read(io_buffer, buffer_len);

  // TODO(miket): the buffer-to-array functionality appears twice, once here
  // and once in socket.cc. When serial etc. is converted over, it'll appear
  // there, too. What's a good single place for it to live? Keep in mind that
  // this is short-term code, to be replaced with ArrayBuffer code.
  DictionaryValue* result = new DictionaryValue();
  ListValue* data_value = new ListValue();
  if (bytes_read > 0) {
    size_t bytes_size = static_cast<size_t>(bytes_read);
    const char* io_buffer_start = io_buffer->data();
    for (size_t i = 0; i < bytes_size; ++i) {
      data_value->Set(i, Value::CreateIntegerValue(io_buffer_start[i]));
    }
  }
  result->Set(kDataKey, data_value);
  result_.reset(result);
}

bool SocketReadFunction::Respond() {
  return true;
}

SocketWriteFunction::SocketWriteFunction()
    : socket_id_(0),
      io_buffer_(NULL) {
}

SocketWriteFunction::~SocketWriteFunction() {
}

bool SocketWriteFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  base::ListValue* data_list_value = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetList(1, &data_list_value));

  size_t size = data_list_value->GetSize();
  if (size != 0) {
    io_buffer_ = new net::IOBufferWithSize(size);
    uint8* data_buffer =
        reinterpret_cast<uint8*>(io_buffer_->data());
    for (size_t i = 0; i < size; ++i) {
      int int_value = -1;
      data_list_value->GetInteger(i, &int_value);
      DCHECK(int_value < 256);
      DCHECK(int_value >= 0);
      uint8 truncated_int = static_cast<uint8>(int_value);
      *data_buffer++ = truncated_int;
    }
  }
  return true;
}

void SocketWriteFunction::Work() {
  int bytes_written = -1;
  Socket* socket = controller()->GetSocket(socket_id_);
  if (socket)
    bytes_written = socket->Write(io_buffer_, io_buffer_->size());
   else
    error_ = kSocketNotFoundError;

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  result_.reset(result);
}

bool SocketWriteFunction::Respond() {
  return true;
}

}  // namespace extensions
