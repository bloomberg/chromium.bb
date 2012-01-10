// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/password_model_worker.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/password_manager/password_store.h"

using base::WaitableEvent;

namespace browser_sync {

PasswordModelWorker::PasswordModelWorker(PasswordStore* password_store)
  : password_store_(password_store) {
  DCHECK(password_store);
}

PasswordModelWorker::~PasswordModelWorker() {}

SyncerError PasswordModelWorker::DoWorkAndWaitUntilDone(
    const WorkCallback& work) {
  WaitableEvent done(false, false);
  SyncerError error = UNSET;
  password_store_->ScheduleTask(
      base::Bind(&PasswordModelWorker::CallDoWorkAndSignalTask,
                 this, work, &done, &error));
  done.Wait();
  return error;
}

void PasswordModelWorker::CallDoWorkAndSignalTask(
    const WorkCallback& work,
    WaitableEvent* done,
    SyncerError *error) {
  *error = work.Run();
  done->Signal();
}

ModelSafeGroup PasswordModelWorker::GetModelSafeGroup() {
  return GROUP_PASSWORD;
}

}  // namespace browser_sync
