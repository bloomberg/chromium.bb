// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_WORKER_H_
#define CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_WORKER_H_
#pragma once

#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/util/unrecoverable_error_info.h"

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"

class PasswordStore;

namespace base {
class WaitableEvent;
}

namespace browser_sync {

// A ModelSafeWorker for password models that accepts requests
// from the syncapi that need to be fulfilled on the password thread,
// which is the DB thread on Linux and Windows.
class PasswordModelWorker : public browser_sync::ModelSafeWorker {
 public:
  explicit PasswordModelWorker(PasswordStore* password_store);
  virtual ~PasswordModelWorker();

  // ModelSafeWorker implementation. Called on syncapi SyncerThread.
  virtual UnrecoverableErrorInfo DoWorkAndWaitUntilDone(
      const WorkCallback& work) OVERRIDE;
  virtual ModelSafeGroup GetModelSafeGroup() OVERRIDE;

 private:
  void CallDoWorkAndSignalTask(
    const WorkCallback& work,
    base::WaitableEvent* done,
    UnrecoverableErrorInfo* error_info);

  scoped_refptr<PasswordStore> password_store_;
  DISALLOW_COPY_AND_ASSIGN(PasswordModelWorker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_PASSWORD_MODEL_WORKER_H_
