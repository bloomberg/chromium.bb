// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/fake_model_worker.h"

#include "base/callback.h"

namespace syncer {

FakeModelWorker::FakeModelWorker(ModelSafeGroup group) : group_(group) {}

FakeModelWorker::~FakeModelWorker() {
  // We may need to relax this is FakeModelWorker is used in a
  // multi-threaded test; since ModelSafeWorkers are
  // RefCountedThreadSafe, they could theoretically be destroyed from
  // a different thread.
  DCHECK(thread_checker_.CalledOnValidThread());
}

SyncerError FakeModelWorker::DoWorkAndWaitUntilDoneImpl(
    const WorkCallback& work) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Simply do the work on the current thread.
  return work.Run();
}

ModelSafeGroup FakeModelWorker::GetModelSafeGroup() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return group_;
}

bool FakeModelWorker::IsOnModelThread() {
  return thread_checker_.CalledOnValidThread();
}

}  // namespace syncer
