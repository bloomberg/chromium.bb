// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/socket_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

SocketCreateFunction::SocketCreateFunction() {
}

SocketCreateFunction::~SocketCreateFunction() {
}

bool SocketCreateFunction::RunImpl() {
  std::string socket_type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &socket_type));

  // TODO(miket): this constitutes a second form of truth as to the
  // enum validity. But our unit-test framework skips the enum
  // validation. So in order to get an invalid-enum test to pass, we
  // need duplicative value-checking. Too bad. Fix this if/when the
  // argument validation code is moved to C++ rather than its current
  // JavaScript form.
  if (socket_type != "udp") {
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
  result->SetInteger("socketId", 42);
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

SocketConnectFunction::SocketConnectFunction() {
}

SocketConnectFunction::~SocketConnectFunction() {
}

bool SocketConnectFunction::RunImpl() {
  std::string socket_type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &socket_type));

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SocketConnectFunction::WorkOnIOThread, this));
  DCHECK(rv);
  return true;
}

void SocketConnectFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  result_.reset(Value::CreateIntegerValue(4));
  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SocketConnectFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SocketConnectFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

SocketDisconnectFunction::SocketDisconnectFunction() {
}

SocketDisconnectFunction::~SocketDisconnectFunction() {
}

bool SocketDisconnectFunction::RunImpl() {
  std::string socket_type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &socket_type));

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SocketDisconnectFunction::WorkOnIOThread, this));
  DCHECK(rv);
  return true;
}

void SocketDisconnectFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  result_.reset(Value::CreateIntegerValue(4));
  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SocketDisconnectFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SocketDisconnectFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

SocketSendFunction::SocketSendFunction() {
}

SocketSendFunction::~SocketSendFunction() {
}

bool SocketSendFunction::RunImpl() {
  std::string socket_type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &socket_type));

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SocketSendFunction::WorkOnIOThread, this));
  DCHECK(rv);
  return true;
}

void SocketSendFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  result_.reset(Value::CreateIntegerValue(4));
  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SocketSendFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SocketSendFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

}  // namespace extensions
