// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/socket/socket_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/socket/socket_api_controller.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char kBytesWrittenKey[] = "bytesWritten";
const char kSocketIdKey[] = "socketId";
const char kUDPSocketType[] = "udp";

SocketCreateFunction::SocketCreateFunction() {
}

SocketCreateFunction::~SocketCreateFunction() {
}

bool SocketCreateFunction::RunImpl() {
  std::string socket_type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &socket_type));

  // TODO(miket): this constitutes a second form of truth as to the enum
  // validity. But our unit-test framework skips the enum validation. So in
  // order to get an invalid-enum test to pass, we need duplicative
  // value-checking. Too bad. Fix this if/when the argument validation code is
  // moved to C++ rather than its current JavaScript form.
  if (socket_type != kUDPSocketType) {
    return false;
  }

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SocketCreateFunction::WorkOnIOThread, this));
  DCHECK(rv);
  return true;
}

void SocketCreateFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DictionaryValue* result = new DictionaryValue();
  SocketController* controller = SocketController::GetInstance();

  int socket_id = controller->CreateUdp(profile(), extension_id(),
                                       source_url());
  result->SetInteger(kSocketIdKey, socket_id);
  result_.reset(result);

  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SocketCreateFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SocketCreateFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

SocketDestroyFunction::SocketDestroyFunction() {
}

SocketDestroyFunction::~SocketDestroyFunction() {
}

bool SocketDestroyFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SocketDestroyFunction::WorkOnIOThread, this));
  DCHECK(rv);
  return true;
}

void SocketDestroyFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  SocketController* controller = SocketController::GetInstance();
  controller->DestroyUdp(socket_id_);

  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SocketDestroyFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SocketDestroyFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

SocketConnectFunction::SocketConnectFunction() {
}

SocketConnectFunction::~SocketConnectFunction() {
}

bool SocketConnectFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &address_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(2, &port_));

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SocketConnectFunction::WorkOnIOThread, this));
  DCHECK(rv);
  return true;
}

void SocketConnectFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SocketController* controller = SocketController::GetInstance();
  bool result = controller->ConnectUdp(socket_id_, address_, port_);
  result_.reset(Value::CreateBooleanValue(result));

  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SocketConnectFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SocketConnectFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

SocketCloseFunction::SocketCloseFunction() {
}

SocketCloseFunction::~SocketCloseFunction() {
}

bool SocketCloseFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SocketCloseFunction::WorkOnIOThread, this));
  DCHECK(rv);
  return true;
}

void SocketCloseFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SocketController* controller = SocketController::GetInstance();
  controller->CloseUdp(socket_id_);
  result_.reset(Value::CreateNullValue());

  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SocketCloseFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SocketCloseFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

SocketWriteFunction::SocketWriteFunction() {
}

SocketWriteFunction::~SocketWriteFunction() {
}

bool SocketWriteFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &message_));

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SocketWriteFunction::WorkOnIOThread, this));
  DCHECK(rv);
  return true;
}

void SocketWriteFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SocketController* controller = SocketController::GetInstance();
  int bytesWritten = controller->WriteUdp(socket_id_, message_);

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kBytesWrittenKey, bytesWritten);
  result_.reset(result);
  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SocketWriteFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SocketWriteFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

}  // namespace extensions
