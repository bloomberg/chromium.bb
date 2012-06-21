// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_WORKER_H_
#pragma once

#include "sync/internal_api/public/engine/model_safe_worker.h"

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"

class PasswordStore;

namespace base {
class WaitableEvent;
}

namespace browser_sync {

// A csync::ModelSafeWorker for password models that accepts requests
// from the syncapi that need to be fulfilled on the password thread,
// which is the DB thread on Linux and Windows.
class PasswordModelWorker : public csync::ModelSafeWorker {
 public:
  explicit PasswordModelWorker(
      const scoped_refptr<PasswordStore>& password_store);

  // csync::ModelSafeWorker implementation. Called on syncapi SyncerThread.
  virtual csync::SyncerError DoWorkAndWaitUntilDone(
      const csync::WorkCallback& work) OVERRIDE;
  virtual csync::ModelSafeGroup GetModelSafeGroup() OVERRIDE;

 private:
  virtual ~PasswordModelWorker();

  void CallDoWorkAndSignalTask(
    const csync::WorkCallback& work,
    base::WaitableEvent* done,
    csync::SyncerError* error);

  scoped_refptr<PasswordStore> password_store_;
  DISALLOW_COPY_AND_ASSIGN(PasswordModelWorker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_WORKER_H_
