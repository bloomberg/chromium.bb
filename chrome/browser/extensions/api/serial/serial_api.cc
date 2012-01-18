// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/serial/serial_api.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

const char kConnectionIdKey[] = "connectionId";

SerialOpenFunction::SerialOpenFunction() {
}

SerialOpenFunction::~SerialOpenFunction() {
}

bool SerialOpenFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &port_));

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SerialOpenFunction::WorkOnIOThread, this));
  DCHECK(rv);

  return true;
}

void SerialOpenFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  DictionaryValue* result = new DictionaryValue();
  result->SetInteger(kConnectionIdKey, 42);
  result_.reset(result);

  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SerialOpenFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SerialOpenFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

SerialCloseFunction::SerialCloseFunction() {
}

SerialCloseFunction::~SerialCloseFunction() {
}

bool SerialCloseFunction::RunImpl() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &connection_id_));

  bool rv = BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SerialCloseFunction::WorkOnIOThread, this));
  DCHECK(rv);

  return true;
}

void SerialCloseFunction::WorkOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  result_.reset(Value::CreateBooleanValue(true));

  bool rv = BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SerialCloseFunction::RespondOnUIThread, this));
  DCHECK(rv);
}

void SerialCloseFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(true);
}

}  // namespace extensions
