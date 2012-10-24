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

PasswordModelWorker::PasswordModelWorker(
    const scoped_refptr<PasswordStore>& password_store)
  : password_store_(password_store) {
  DCHECK(password_store);
}

syncer::SyncerError PasswordModelWorker::DoWorkAndWaitUntilDone(
    const syncer::WorkCallback& work) {
  WaitableEvent done(false, false);
  syncer::SyncerError error = syncer::UNSET;
  if (password_store_->ScheduleTask(
          base::Bind(&PasswordModelWorker::CallDoWorkAndSignalTask,
                     this, work, &done, &error))) {
    done.Wait();
  } else {
    error = syncer::CANNOT_DO_WORK;
  }
  return error;
}

syncer::ModelSafeGroup PasswordModelWorker::GetModelSafeGroup() {
  return syncer::GROUP_PASSWORD;
}

PasswordModelWorker::~PasswordModelWorker() {}

void PasswordModelWorker::CallDoWorkAndSignalTask(
    const syncer::WorkCallback& work,
    WaitableEvent* done,
    syncer::SyncerError *error) {
  *error = work.Run();
  done->Signal();
}

}  // namespace browser_sync
