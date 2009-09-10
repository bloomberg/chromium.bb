// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_MODEL_SAFE_WORKER_H_
#define CHROME_BROWSER_SYNC_ENGINE_MODEL_SAFE_WORKER_H_

#include "chrome/browser/sync/util/closure.h"
#include "chrome/browser/sync/util/sync_types.h"

namespace browser_sync {

// The Syncer uses a ModelSafeWorker for all tasks that could potentially
// modify syncable entries (e.g under a WriteTransaction). The ModelSafeWorker
// only knows how to do one thing, and that is take some work (in a fully
// pre-bound callback) and have it performed (as in Run()) from a thread which
// is guaranteed to be "model-safe", where "safe" refers to not allowing us to
// cause an embedding application model to fall out of sync with the
// syncable::Directory due to a race.
class ModelSafeWorker {
 public:
  ModelSafeWorker() { }
  virtual ~ModelSafeWorker() { }

  // Any time the Syncer performs model modifications (e.g employing a
  // WriteTransaction), it should be done by this method to ensure it is done
  // from a model-safe thread.
  //
  // TODO(timsteele): For now this is non-reentrant, meaning the work being
  // done should be at a high enough level in the stack that
  // DoWorkAndWaitUntilDone won't be called again by invoking Run() on |work|.
  // This is not strictly necessary; it may be best to call
  // DoWorkAndWaitUntilDone at lower levels, such as within ApplyUpdates, but
  // this is sufficient to simplify and test out our dispatching approach.
  virtual void DoWorkAndWaitUntilDone(Closure* work) {
    work->Run();  // By default, do the work on the current thread.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ModelSafeWorker);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_MODEL_SAFE_WORKER_H_
