// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_MODEL_SAFE_WORKER_H_
#define COMPONENTS_SYNC_ENGINE_MODEL_SAFE_WORKER_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/syncer_error.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace syncer {

using WorkCallback = base::Callback<enum SyncerError(void)>;

enum ModelSafeGroup {
  GROUP_PASSIVE = 0,   // Models that are just "passively" being synced; e.g.
                       // changes to these models don't need to be pushed to a
                       // native model.
  GROUP_UI,            // Models that live on UI thread and are being synced.
  GROUP_DB,            // Models that live on DB thread and are being synced.
  GROUP_FILE,          // Models that live on FILE thread and are being synced.
  GROUP_HISTORY,       // Models that live on history thread and are being
                       // synced.
  GROUP_PASSWORD,      // Models that live on the password thread and are
                       // being synced.  On windows and linux, this runs on the
                       // DB thread.
  GROUP_NON_BLOCKING,  // Models that correspond to non-blocking types. These
                       // models always stay in GROUP_NON_BLOCKING; changes are
                       // forwarded to these models without ModelSafeWorker/
                       // SyncBackendRegistrar involvement.
};

std::string ModelSafeGroupToString(ModelSafeGroup group);

// The Syncer uses a ModelSafeWorker for all tasks that could potentially
// modify syncable entries (e.g under a WriteTransaction). The ModelSafeWorker
// only knows how to do one thing, and that is take some work (in a fully
// pre-bound callback) and have it performed (as in Run()) from a thread which
// is guaranteed to be "model-safe", where "safe" refers to not allowing us to
// cause an embedding application model to fall out of sync with the
// syncable::Directory due to a race. Each ModelSafeWorker is affiliated with
// a thread and does actual work on that thread.
class ModelSafeWorker : public base::RefCountedThreadSafe<ModelSafeWorker> {
 public:
  // If not stopped, call DoWorkAndWaitUntilDoneImpl() to do work. Otherwise
  // return CANNOT_DO_WORK.
  SyncerError DoWorkAndWaitUntilDone(const WorkCallback& work);

  // Soft stop worker by setting stopped_ flag. Called when sync is disabled
  // or browser is shutting down. Called on UI loop.
  virtual void RequestStop();

  // Return true if the worker was stopped. Thread safe.
  bool IsStopped();

  virtual ModelSafeGroup GetModelSafeGroup() = 0;

  // Returns true if called on the thread this worker works on.
  virtual bool IsOnModelThread() = 0;

 protected:
  ModelSafeWorker();
  virtual ~ModelSafeWorker();

  // Any time the Syncer performs model modifications (e.g employing a
  // WriteTransaction), it should be done by this method to ensure it is done
  // from a model-safe thread.
  virtual SyncerError DoWorkAndWaitUntilDoneImpl(const WorkCallback& work) = 0;

 private:
  friend class base::RefCountedThreadSafe<ModelSafeWorker>;

  // Whether the worker should do more work. Set when sync is disabled.
  base::AtomicFlag stopped_;

  DISALLOW_COPY_AND_ASSIGN(ModelSafeWorker);
};

// A map that details which ModelSafeGroup each ModelType
// belongs to.  Routing info can change in response to the user enabling /
// disabling sync for certain types, as well as model association completions.
using ModelSafeRoutingInfo = std::map<ModelType, ModelSafeGroup>;

// Caller takes ownership of return value.
std::unique_ptr<base::DictionaryValue> ModelSafeRoutingInfoToValue(
    const ModelSafeRoutingInfo& routing_info);

std::string ModelSafeRoutingInfoToString(
    const ModelSafeRoutingInfo& routing_info);

ModelTypeSet GetRoutingInfoTypes(const ModelSafeRoutingInfo& routing_info);

ModelSafeGroup GetGroupForModelType(const ModelType type,
                                    const ModelSafeRoutingInfo& routes);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_MODEL_SAFE_WORKER_H_
