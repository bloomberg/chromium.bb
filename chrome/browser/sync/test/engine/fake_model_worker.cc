// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/engine/fake_model_worker.h"

namespace browser_sync {

FakeModelWorker::FakeModelWorker(ModelSafeGroup group) : group_(group) {}

FakeModelWorker::~FakeModelWorker() {
  // We may need to relax this is FakeModelWorker is used in a
  // multi-threaded test; since ModelSafeWorkers are
  // RefCountedThreadSafe, they could theoretically be destroyed from
  // a different thread.
  DCHECK(non_thread_safe_.CalledOnValidThread());
}

UnrecoverableErrorInfo FakeModelWorker::DoWorkAndWaitUntilDone(
    const WorkCallback& work) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  // Simply do the work on the current thread.
  return work.Run();
}

ModelSafeGroup FakeModelWorker::GetModelSafeGroup() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  return group_;
}

}  // namespace browser_sync
