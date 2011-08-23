// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_REGISTRAR_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_REGISTRAR_H_
#pragma once

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/syncable/model_type.h"

class MessageLoop;
class Profile;

namespace sync_api {
struct UserShare;
}  // namespace sync_api

namespace browser_sync {

class ChangeProcessor;
class UIModelWorker;

// A class to keep track of the workers and routing info for the
// enabled sync types.
class SyncBackendRegistrar : public ModelSafeWorkerRegistrar {
 public:
  // |initial_types| contains the initial set of types to sync
  // (initially put in the passive group).  Does not take ownership of
  // |profile| or |sync_loop|.  Must be created on the UI thread.
  SyncBackendRegistrar(const syncable::ModelTypeSet& initial_types,
                       Profile* profile,
                       MessageLoop* sync_loop);

  // SyncBackendRegistrar must be destroyed as follows:
  //
  //   1) On the sync thread, call OnSyncerShutdownComplete() after
  //      the syncer is shutdown.
  //   2) Meanwhile, on the UI thread, call StopOnUIThread(), which
  //      blocks until OnSyncerShutdownComplete() is called.
  //   3) Destroy the SyncBackendRegistrar.
  //
  // This is to handle the complicated shutdown requirements of the
  // UIModelWorker (since the UI thread is both the main thread and a
  // thread which the syncer pushes changes to).
  virtual ~SyncBackendRegistrar();

  // Returns whether or not we are currently syncing encryption keys.
  // Must be called on the UI thread.
  bool IsNigoriEnabled() const;

  // Removes all types in |types_to_remove| from the routing info and
  // adds all the types in |types_to_add| to the routing info that are
  // not already there (initially put in the passive group).
  // |types_to_remove| and |types_to_add| must be disjoint.  Returns
  // the set of newly-added types.  Must be called on the UI thread.
  syncable::ModelTypeSet ConfigureDataTypes(
      const syncable::ModelTypeSet& types_to_add,
      const syncable::ModelTypeSet& types_to_remove);

  // Must be called from the UI thread. (See destructor comment.)
  void StopOnUIThread();

  // Must be called from the sync thread. (See destructor comment.)
  void OnSyncerShutdownComplete();

  // Activates the given data type (which should belong to the given
  // group) and starts the given change processor.  Must be called
  // from |group|'s native thread.
  void ActivateDataType(syncable::ModelType type,
                        ModelSafeGroup group,
                        ChangeProcessor* change_processor,
                        sync_api::UserShare* user_share);

  // Deactivates the given type if necessary.  Must be called from the
  // UI thread and not |type|'s native thread.  Yes, this is
  // surprising: see http://crbug.com/92804.
  void DeactivateDataType(syncable::ModelType type);

  // Returns the change processor for the given model, or NULL if none
  // exists.  Must be called from |group|'s native thread.
  ChangeProcessor* GetProcessor(syncable::ModelType type);

  // ModelSafeWorkerRegistrar implementation.  May be called from any
  // thread.
  virtual void GetWorkers(
      std::vector<ModelSafeWorker*>* out) OVERRIDE;
  virtual void GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) OVERRIDE;

 private:
  typedef std::map<ModelSafeGroup,
                   scoped_refptr<ModelSafeWorker> > WorkerMap;

  // Must be called with |lock_| held.  Simply returns the change
  // processor for the given type, if it exists.  May be called from
  // any thread.
  ChangeProcessor* GetProcessorUnsafe(syncable::ModelType type);

  // Return true if |model_type| lives on the current thread.  Must be
  // called with |lock_| held.  May be called on any thread.
  bool IsCurrentThreadSafeForModel(
      syncable::ModelType model_type) const;

  Profile* const profile_;

  MessageLoop* const sync_loop_;

  const scoped_refptr<UIModelWorker> ui_worker_;

  bool stopped_on_ui_thread_;

  // Protects all variables below.
  mutable base::Lock lock_;

  // We maintain ownership of all workers.  In some cases, we need to
  // ensure shutdown occurs in an expected sequence by Stop()ing
  // certain workers.  They are guaranteed to be valid because we only
  // destroy elements of |workers_| after the syncapi has been
  // destroyed.  Unless a worker is no longer needed because all types
  // that get routed to it have been disabled (from syncing). In that
  // case, we'll destroy on demand *after* routing any dependent types
  // to GROUP_PASSIVE, so that the syncapi doesn't call into garbage.
  // If a key is present, it means at least one ModelType that routes
  // to that model safe group is being synced.
  WorkerMap workers_;
  ModelSafeRoutingInfo routing_info_;

  // The change processors that handle the different data types.
  std::map<syncable::ModelType, ChangeProcessor*> processors_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendRegistrar);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_REGISTRAR_H_
