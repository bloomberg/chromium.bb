// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/socket/socket_api_controller.h"
#include "chrome/browser/extensions/api/socket/socket_event_notifier.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char kBytesWrittenKey[] = "bytesWritten";
const char kMessageKey[] = "message";
const char kSocketIdKey[] = "socketId";
const char kTCPOption[] = "tcp";
const char kUDPOption[] = "udp";

SocketController* SocketApiFunction::controller() {
  return profile()->GetExtensionService()->socket_controller();
}

bool SocketApiFunction::RunImpl() {
  if (!Prepare()) {
    return false;
  }
  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SocketApiFunction::WorkOnIOThread, this));
  DCHECK(rv);
  return true;
}

void SocketApiFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  Work();
  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SocketApiFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SocketApiFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(Respond());
}

SocketCreateFunction::SocketCreateFunction()
    : src_id_(-1) {
}

bool SocketCreateFunction::Prepare() {
  std::string socket_type_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &socket_type_string));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &address_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(2, &port_));

  scoped_ptr<DictionaryValue> options(new DictionaryValue());
  if (args_->GetSize() > 3) {
    DictionaryValue* temp_options = NULL;
    if (args_->GetDictionary(3, &temp_options))
      options.reset(temp_options->DeepCopy());
  }

  // If we tacked on a srcId to the options object, pull it out here to provide
  // to the Socket.
  if (options->HasKey(kSrcIdKey)) {
    EXTENSION_FUNCTION_VALIDATE(options->GetInteger(kSrcIdKey, &src_id_));
  }

  if (socket_type_string == kTCPOption)
    socket_type_ = kSocketTypeTCP;
  else if (socket_type_string == kUDPOption)
    socket_type_ = kSocketTypeUDP;
  else
    return false;
  return true;
}

void SocketCreateFunction::Work() {
  SocketEventNotifier* event_notifier(new SocketEventNotifier(
      profile()->GetExtensionEventRouter(), profile(), extension_id(),
      src_id_, source_url()));
  int socket_id = socket_type_ == kSocketTypeTCP ?
      controller()->CreateTCPSocket(address_, port_, event_notifier) :
      controller()->CreateUDPSocket(address_, port_, event_notifier);
  DictionaryValue* result = new DictionaryValue();

  result->SetInteger(kSocketIdKey, socket_id);
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
  controller()->DestroySocket(socket_id_);
}

bool SocketDestroyFunction::Respond() {
  return true;
}

bool SocketConnectFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketConnectFunction::Work() {
  int result = controller()->ConnectSocket(socket_id_);
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
  controller()->DisconnectSocket(socket_id_);
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
  std::string message = controller()->ReadSocket(socket_id_);

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
  int bytes_written = controller()->WriteSocket(socket_id_, message_);

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytes_written);
  result_.reset(result);
}

bool SocketWriteFunction::Respond() {
  return true;
}

}  // namespace extensions
