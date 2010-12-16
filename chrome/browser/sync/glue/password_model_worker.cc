// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/password_model_worker.h"

#include "base/callback.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "base/waitable_event.h"
#include "chrome/browser/password_manager/password_store.h"

using base::WaitableEvent;

namespace browser_sync {

PasswordModelWorker::PasswordModelWorker(PasswordStore* password_store)
  : password_store_(password_store) {
  DCHECK(password_store);
}

PasswordModelWorker::~PasswordModelWorker() {}

void PasswordModelWorker::DoWorkAndWaitUntilDone(Callback0::Type* work) {
  WaitableEvent done(false, false);
  password_store_->ScheduleTask(
      NewRunnableMethod(this, &PasswordModelWorker::CallDoWorkAndSignalTask,
                        work, &done));
  done.Wait();
}

void PasswordModelWorker::CallDoWorkAndSignalTask(Callback0::Type* work,
                                                  WaitableEvent* done) {
  work->Run();
  done->Signal();
}

ModelSafeGroup PasswordModelWorker::GetModelSafeGroup() {
  return GROUP_PASSWORD;
}

bool PasswordModelWorker::CurrentThreadIsWorkThread() {
  // TODO(ncarter): How to determine this?
  return true;
}

}  // namespace browser_sync
