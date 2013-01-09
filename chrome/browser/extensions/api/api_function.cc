// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/api_function.h"

#include "base/bind.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"

using content::BrowserThread;

namespace extensions {

ApiFunction::ApiFunction() {
}

ApiFunction::~ApiFunction() {
}

// AsyncApiFunction
AsyncApiFunction::AsyncApiFunction()
    : work_thread_id_(BrowserThread::IO) {
}

AsyncApiFunction::~AsyncApiFunction() {
}

bool AsyncApiFunction::RunImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!PrePrepare() || !Prepare()) {
    return false;
  }
  bool rv = BrowserThread::PostTask(
      work_thread_id_, FROM_HERE,
      base::Bind(&AsyncApiFunction::WorkOnWorkThread, this));
  DCHECK(rv);
  return true;
}

bool AsyncApiFunction::PrePrepare() {
  return true;
}

void AsyncApiFunction::Work() {
}

void AsyncApiFunction::AsyncWorkStart() {
  Work();
  AsyncWorkCompleted();
}

void AsyncApiFunction::AsyncWorkCompleted() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    bool rv = BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AsyncApiFunction::RespondOnUIThread, this));
    DCHECK(rv);
  } else {
    SendResponse(Respond());
  }
}

void AsyncApiFunction::WorkOnWorkThread() {
  DCHECK(BrowserThread::CurrentlyOn(work_thread_id_));
  DCHECK(work_thread_id_ != BrowserThread::UI) <<
      "You have specified that AsyncApiFunction::Work() should happen on "
      "the UI thread. This nullifies the point of this class. Either "
      "specify a different thread or derive from a different class.";
  AsyncWorkStart();
}

void AsyncApiFunction::RespondOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendResponse(Respond());
}

}  // namespace extensions
