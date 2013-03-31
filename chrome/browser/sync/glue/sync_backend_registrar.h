// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_REGISTRAR_H_
#define CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_REGISTRAR_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "sync/internal_api/public/sync_manager.h"

class Profile;

namespace base {
class MessageLoop;
}

namespace syncer {
struct UserShare;
}  // namespace syncer

namespace browser_sync {

class ChangeProcessor;
class UIModelWorker;

// A class that keep track of the workers, change processors, and
// routing info for the enabled sync types, and also routes change
// events to the right processors.
class SyncBackendRegistrar : public syncer::SyncManager::ChangeDelegate {
 public:
  // |name| is used for debugging.  Does not take ownership of |profile| or
  // |sync_loop|.  Must be created on the UI thread.
  SyncBackendRegistrar(const std::string& name,
                       Profile* profile,
                       base::MessageLoop* sync_loop);

  // Informs the SyncBackendRegistrar of the currently enabled set of types.
  // These types will be placed in the passive group.  This function should be
  // called exactly once during startup.
  void SetInitialTypes(syncer::ModelTypeSet initial_types);

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
  syncer::ModelTypeSet ConfigureDataTypes(
      syncer::ModelTypeSet types_to_add,
      syncer::ModelTypeSet types_to_remove);

  // Must be called from the UI thread. (See destructor comment.)
  void StopOnUIThread();

  // Must be called from the sync thread. (See destructor comment.)
  void OnSyncerShutdownComplete();

  // Activates the given data type (which should belong to the given
  // group) and starts the given change processor.  Must be called
  // from |group|'s native thread.
  void ActivateDataType(syncer::ModelType type,
                        syncer::ModelSafeGroup group,
                        ChangeProcessor* change_processor,
                        syncer::UserShare* user_share);

  // Deactivates the given type if necessary.  Must be called from the
  // UI thread and not |type|'s native thread.  Yes, this is
  // surprising: see http://crbug.com/92804.
  void DeactivateDataType(syncer::ModelType type);

  // Returns true only between calls to ActivateDataType(type, ...)
  // and DeactivateDataType(type).  Used only by tests.
  bool IsTypeActivatedForTest(syncer::ModelType type) const;

  // SyncManager::ChangeDelegate implementation.  May be called from
  // any thread.
  virtual void OnChangesApplied(
      syncer::ModelType model_type,
      int64 model_version,
      const syncer::BaseTransaction* trans,
      const syncer::ImmutableChangeRecordList& changes) OVERRIDE;
  virtual void OnChangesComplete(syncer::ModelType model_type) OVERRIDE;

  void GetWorkers(std::vector<syncer::ModelSafeWorker*>* out);
  void GetModelSafeRoutingInfo(syncer::ModelSafeRoutingInfo* out);

 private:
  typedef std::map<syncer::ModelSafeGroup,
                   scoped_refptr<syncer::ModelSafeWorker> > WorkerMap;

  // Returns the change processor for the given model, or NULL if none
  // exists.  Must be called from |group|'s native thread.
  ChangeProcessor* GetProcessor(syncer::ModelType type) const;

  // Must be called with |lock_| held.  Simply returns the change
  // processor for the given type, if it exists.  May be called from
  // any thread.
  ChangeProcessor* GetProcessorUnsafe(syncer::ModelType type) const;

  // Return true if |model_type| lives on the current thread.  Must be
  // called with |lock_| held.  May be called on any thread.
  bool IsCurrentThreadSafeForModel(
      syncer::ModelType model_type) const;

  // Name used for debugging.
  const std::string name_;

  Profile* const profile_;

  base::MessageLoop* const sync_loop_;

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
  // to syncer::GROUP_PASSIVE, so that the syncapi doesn't call into garbage.
  // If a key is present, it means at least one ModelType that routes
  // to that model safe group is being synced.
  WorkerMap workers_;
  syncer::ModelSafeRoutingInfo routing_info_;

  // The change processors that handle the different data types.
  std::map<syncer::ModelType, ChangeProcessor*> processors_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendRegistrar);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_REGISTRAR_H_
