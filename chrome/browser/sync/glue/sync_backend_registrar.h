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
#include "base/threading/thread.h"
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

namespace sync_driver {
class ChangeProcessor;
}

namespace browser_sync {

class UIModelWorker;

// A class that keep track of the workers, change processors, and
// routing info for the enabled sync types, and also routes change
// events to the right processors.
class SyncBackendRegistrar : public syncer::SyncManager::ChangeDelegate,
                             public syncer::WorkerLoopDestructionObserver {
 public:
  // |name| is used for debugging.  Does not take ownership of |profile| or
  // |sync_loop|.  Must be created on the UI thread.
  SyncBackendRegistrar(const std::string& name,
                       Profile* profile,
                       scoped_ptr<base::Thread> sync_thread);

  // SyncBackendRegistrar must be destroyed as follows:
  //
  //   1) On the UI thread, call RequestWorkerStopOnUIThread().
  //   2) UI posts task to shut down syncer on sync thread.
  //   3) If sync is disabled, call ReleaseSyncThread() on the UI thread.
  //   3) UI posts SyncBackendRegistrar::ShutDown() on sync thread to
  //      unregister workers from observing destruction of their working loops.
  //   4) Workers notify registrar when unregistration finishes or working
  //      loops are destroyed. Registrar destroys itself on last worker
  //      notification. Sync thread will be stopped if ownership was not
  //      released.
  virtual ~SyncBackendRegistrar();

  // Informs the SyncBackendRegistrar of the currently enabled set of types.
  // These types will be placed in the passive group.  This function should be
  // called exactly once during startup.
  void SetInitialTypes(syncer::ModelTypeSet initial_types);

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

  // Returns the set of enabled types as of the last configuration. Note that
  // this might be different from the current types in the routing info due
  // to DeactiveDataType being called separately from ConfigureDataTypes.
  syncer::ModelTypeSet GetLastConfiguredTypes() const;

  // Must be called from the UI thread. (See destructor comment.)
  void RequestWorkerStopOnUIThread();

  // Activates the given data type (which should belong to the given
  // group) and starts the given change processor.  Must be called
  // from |group|'s native thread.
  void ActivateDataType(syncer::ModelType type,
                        syncer::ModelSafeGroup group,
                        sync_driver::ChangeProcessor* change_processor,
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

  void GetWorkers(std::vector<scoped_refptr<syncer::ModelSafeWorker> >* out);
  void GetModelSafeRoutingInfo(syncer::ModelSafeRoutingInfo* out);

  // syncer::WorkerLoopDestructionObserver implementation.
  virtual void OnWorkerLoopDestroyed(syncer::ModelSafeGroup group) OVERRIDE;

  // Release ownership of |sync_thread_|. Called when sync is disabled.
  scoped_ptr<base::Thread> ReleaseSyncThread();

  // Unregister workers from loop destruction observation.
  void Shutdown();

  base::Thread* sync_thread();

 private:
  typedef std::map<syncer::ModelSafeGroup,
      scoped_refptr<syncer::ModelSafeWorker> > WorkerMap;
  typedef std::map<syncer::ModelType, sync_driver::ChangeProcessor*>
      ProcessorMap;

  // Callback after workers unregister from observing destruction of their
  // working loops.
  void OnWorkerUnregistrationDone(syncer::ModelSafeGroup group);

  void RemoveWorker(syncer::ModelSafeGroup group);

  // Returns the change processor for the given model, or NULL if none
  // exists.  Must be called from |group|'s native thread.
  sync_driver::ChangeProcessor* GetProcessor(syncer::ModelType type) const;

  // Must be called with |lock_| held.  Simply returns the change
  // processor for the given type, if it exists.  May be called from
  // any thread.
  sync_driver::ChangeProcessor* GetProcessorUnsafe(
      syncer::ModelType type) const;

  // Return true if |model_type| lives on the current thread.  Must be
  // called with |lock_| held.  May be called on any thread.
  bool IsCurrentThreadSafeForModel(
      syncer::ModelType model_type) const;

  // Name used for debugging.
  const std::string name_;

  Profile* const profile_;

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
  ProcessorMap processors_;

  // The types that were enabled as of the last configuration. Updated on each
  // call to ConfigureDataTypes as well as SetInitialTypes.
  syncer::ModelTypeSet last_configured_types_;

  // Parks stopped workers because they may still be referenced by syncer.
  std::vector<scoped_refptr<syncer::ModelSafeWorker> > stopped_workers_;

  // Declare |sync_thread_| at the end so that it will be destroyed before
  // objects above because tasks on sync thread depend on those objects,
  // e.g. Shutdown() depends on |lock_|, SyncManager::Init() depends on
  // workers, etc.
  scoped_ptr<base::Thread> sync_thread_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendRegistrar);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_SYNC_BACKEND_REGISTRAR_H_
