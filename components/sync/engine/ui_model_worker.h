// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_UI_MODEL_WORKER_H_
#define COMPONENTS_SYNC_ENGINE_UI_MODEL_WORKER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "components/sync/engine/model_safe_worker.h"

namespace syncer {

// A ModelSafeWorker for UI models (e.g. bookmarks) that
// accepts work requests from the syncapi that need to be fulfilled
// from the MessageLoop home to the native model.
class UIModelWorker : public ModelSafeWorker {
 public:
  explicit UIModelWorker(scoped_refptr<base::SingleThreadTaskRunner> ui_thread);

  // ModelSafeWorker implementation.
  void RequestStop() override;
  ModelSafeGroup GetModelSafeGroup() override;
  bool IsOnModelThread() override;

 protected:
  SyncerError DoWorkAndWaitUntilDoneImpl(const WorkCallback& work) override;

 private:
  ~UIModelWorker() override;

  // A reference to the UI thread's task runner.
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;

  // Signaled when a task posted by DoWorkAndWaitUntilDoneImpl() is deleted,
  // i.e. after it runs or when it is abandoned. Reset at the beginning of every
  // DoWorkAndWaitUntilDoneImpl() call.
  base::WaitableEvent work_done_or_abandoned_;

  // Signaled from RequestStop(). When this is signaled,
  // DoWorkAndWaitUntilDoneImpl() returns immediately. This is needed to prevent
  // the UI thread from joining the sync thread while it is waiting for a
  // WorkCallback to run on the UI thread. See crbug.com/663600.
  base::WaitableEvent stop_requested_;

  // Verifies that calls to DoWorkAndWaitUntilDoneImpl() are sequenced.
  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(UIModelWorker);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_UI_MODEL_WORKER_H_
