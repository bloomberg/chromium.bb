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
    const scoped_refptr<PasswordStore>& password_store,
    syncer::WorkerLoopDestructionObserver* observer)
  : syncer::ModelSafeWorker(observer),
    password_store_(password_store) {
  DCHECK(password_store.get());
}

void PasswordModelWorker::RegisterForLoopDestruction() {
  password_store_->ScheduleTask(
      base::Bind(&PasswordModelWorker::RegisterForPasswordLoopDestruction,
                 this));
}

syncer::SyncerError PasswordModelWorker::DoWorkAndWaitUntilDoneImpl(
    const syncer::WorkCallback& work) {
  syncer::SyncerError error = syncer::UNSET;
  if (password_store_->ScheduleTask(
          base::Bind(&PasswordModelWorker::CallDoWorkAndSignalTask,
                     this, work, work_done_or_stopped(), &error))) {
    work_done_or_stopped()->Wait();
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

void PasswordModelWorker::RegisterForPasswordLoopDestruction() {
  base::MessageLoop::current()->AddDestructionObserver(this);
  SetWorkingLoopToCurrent();
}

}  // namespace browser_sync
