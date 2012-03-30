// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/api_resource_controller.h"
#include "chrome/browser/extensions/api/socket/socket.h"
#include "chrome/browser/extensions/api/socket/tcp_socket.h"
#include "chrome/browser/extensions/api/socket/udp_socket.h"
#include "chrome/browser/extensions/extension_service.h"

namespace extensions {

const char kBytesWrittenKey[] = "bytesWritten";
const char kMessageKey[] = "message";
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
  std::string message;
  Socket* socket = controller()->GetSocket(socket_id_);
  if (socket)
    message = socket->Read();

  DictionaryValue* result = new DictionaryValue();
  result->SetString(kMessageKey, message);
  result_.reset(result);
}

bool SocketReadFunction::Respond() {
  return true;
}

bool SocketWriteFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &message_));
  return true;
}

void SocketWriteFunction::Work() {
  int bytes_written = -1;
  Socket* socket = controller()->GetSocket(socket_id_);
  if (socket)
    bytes_written = socket->Write(message_);
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
