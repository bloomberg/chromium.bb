// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_ENGINE_FAKE_MODEL_WORKER_H_
#define COMPONENTS_SYNC_TEST_ENGINE_FAKE_MODEL_WORKER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync/base/syncer_error.h"
#include "components/sync/engine/model_safe_worker.h"

namespace syncer {

// Fake implementation of ModelSafeWorker that does work on the
// current thread regardless of the group.
class FakeModelWorker : public ModelSafeWorker, public base::NonThreadSafe {
 public:
  explicit FakeModelWorker(ModelSafeGroup group);

  // ModelSafeWorker implementation.
  void RegisterForLoopDestruction() override;
  ModelSafeGroup GetModelSafeGroup() override;

 protected:
  SyncerError DoWorkAndWaitUntilDoneImpl(const WorkCallback& work) override;

 private:
  ~FakeModelWorker() override;

  const ModelSafeGroup group_;

  DISALLOW_COPY_AND_ASSIGN(FakeModelWorker);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_ENGINE_FAKE_MODEL_WORKER_H_
