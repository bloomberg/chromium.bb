// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_WORKER_H_

#include "sync/internal_api/public/engine/model_safe_worker.h"

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"

namespace base {
class WaitableEvent;
}

namespace password_manager {
class PasswordStore;
}

namespace browser_sync {

// A syncer::ModelSafeWorker for password models that accepts requests
// from the syncapi that need to be fulfilled on the password thread,
// which is the DB thread on Linux and Windows.
class PasswordModelWorker : public syncer::ModelSafeWorker {
 public:
  PasswordModelWorker(
      const scoped_refptr<password_manager::PasswordStore>& password_store,
      syncer::WorkerLoopDestructionObserver* observer);

  // syncer::ModelSafeWorker implementation. Called on syncapi SyncerThread.
  virtual void RegisterForLoopDestruction() OVERRIDE;
  virtual syncer::ModelSafeGroup GetModelSafeGroup() OVERRIDE;
  virtual void RequestStop() OVERRIDE;

 protected:
  virtual syncer::SyncerError DoWorkAndWaitUntilDoneImpl(
      const syncer::WorkCallback& work) OVERRIDE;

 private:
  virtual ~PasswordModelWorker();

  void CallDoWorkAndSignalTask(
    const syncer::WorkCallback& work,
    base::WaitableEvent* done,
    syncer::SyncerError* error);

  // Called on password thread to add PasswordModelWorker as destruction
  // observer.
  void RegisterForPasswordLoopDestruction();

  // |password_store_| is used on password thread but released on UI thread.
  // Protected by |password_store_lock_|.
  base::Lock password_store_lock_;
  scoped_refptr<password_manager::PasswordStore> password_store_;
  DISALLOW_COPY_AND_ASSIGN(PasswordModelWorker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_WORKER_H_
