// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_MODEL_SAFE_WORKER_H_
#define CHROME_BROWSER_SYNC_ENGINE_MODEL_SAFE_WORKER_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_old.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/browser/sync/util/unrecoverable_error_info.h"

namespace base {
class DictionaryValue;
}  // namespace

namespace browser_sync {

typedef base::Callback<UnrecoverableErrorInfo(void)> WorkCallback;

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
  MODEL_SAFE_GROUP_COUNT,
};

std::string ModelSafeGroupToString(ModelSafeGroup group);

// The Syncer uses a ModelSafeWorker for all tasks that could potentially
// modify syncable entries (e.g under a WriteTransaction). The ModelSafeWorker
// only knows how to do one thing, and that is take some work (in a fully
// pre-bound callback) and have it performed (as in Run()) from a thread which
// is guaranteed to be "model-safe", where "safe" refers to not allowing us to
// cause an embedding application model to fall out of sync with the
// syncable::Directory due to a race.
class ModelSafeWorker : public base::RefCountedThreadSafe<ModelSafeWorker> {
 public:
  // Any time the Syncer performs model modifications (e.g employing a
  // WriteTransaction), it should be done by this method to ensure it is done
  // from a model-safe thread.
  virtual UnrecoverableErrorInfo DoWorkAndWaitUntilDone(
      const WorkCallback& work) = 0;

  virtual ModelSafeGroup GetModelSafeGroup() = 0;

 protected:
  virtual ~ModelSafeWorker();

 private:
  friend class base::RefCountedThreadSafe<ModelSafeWorker>;
};

// A map that details which ModelSafeGroup each syncable::ModelType
// belongs to.  Routing info can change in response to the user enabling /
// disabling sync for certain types, as well as model association completions.
typedef std::map<syncable::ModelType, ModelSafeGroup>
    ModelSafeRoutingInfo;

// Caller takes ownership of return value.
base::DictionaryValue* ModelSafeRoutingInfoToValue(
    const ModelSafeRoutingInfo& routing_info);

std::string ModelSafeRoutingInfoToString(
    const ModelSafeRoutingInfo& routing_info);

syncable::ModelTypeSet GetRoutingInfoTypes(
    const ModelSafeRoutingInfo& routing_info);

ModelSafeGroup GetGroupForModelType(const syncable::ModelType type,
                                    const ModelSafeRoutingInfo& routes);

// Maintain the up-to-date state regarding which ModelSafeWorkers exist and
// which types get routed to which worker.  When a sync session begins, it will
// snapshot the state at that instant, and will use that for the entire
// session.  This means if a model becomes synced (or unsynced) by the user
// during a sync session, that session will complete and be unaware of this
// change -- it will only get picked up for the next session.
// TODO(tim): That's really the only way I can make sense of it in the Syncer
// HOWEVER, it is awkward for running ModelAssociation. We need to make sure
// we don't run such a thing until an active session wraps up.
class ModelSafeWorkerRegistrar {
 public:
  ModelSafeWorkerRegistrar() { }
  // Get the current list of active ModelSafeWorkers.  Should be threadsafe.
  virtual void GetWorkers(std::vector<ModelSafeWorker*>* out) = 0;

  // Get the current routing information for all enabled model types.
  // If a model type is not enabled (that is, if the syncer should not
  // be trying to sync it), it is not in this map.
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) = 0;
 protected:
  virtual ~ModelSafeWorkerRegistrar() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(ModelSafeWorkerRegistrar);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_MODEL_SAFE_WORKER_H_
