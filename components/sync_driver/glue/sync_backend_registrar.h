// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_REGISTRAR_H_
#define COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_REGISTRAR_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
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
class SyncClient;
}

namespace browser_sync {

class UIModelWorker;

// A class that keep track of the workers, change processors, and
// routing info for the enabled sync types, and also routes change
// events to the right processors.
class SyncBackendRegistrar : public syncer::SyncManager::ChangeDelegate,
                             public syncer::WorkerLoopDestructionObserver {
 public:
  // |name| is used for debugging.  Does not take ownership of |profile|.
  // Must be created on the UI thread.
  SyncBackendRegistrar(
      const std::string& name,
      sync_driver::SyncClient* sync_client,
      std::unique_ptr<base::Thread> sync_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& file_thread);

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
  ~SyncBackendRegistrar() override;

  // Adds |type| to set of non-blocking types. These types are assigned to
  // GROUP_NON_BLOCKING model safe group and will be treated differently in
  // ModelTypeRegistry. Unlike directory types, non-blocking types always stay
  // assigned to GROUP_NON_BLOCKING group.
  void RegisterNonBlockingType(syncer::ModelType type);

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
  syncer::ModelTypeSet ConfigureDataTypes(syncer::ModelTypeSet types_to_add,
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
  void OnChangesApplied(
      syncer::ModelType model_type,
      int64_t model_version,
      const syncer::BaseTransaction* trans,
      const syncer::ImmutableChangeRecordList& changes) override;
  void OnChangesComplete(syncer::ModelType model_type) override;

  void GetWorkers(std::vector<scoped_refptr<syncer::ModelSafeWorker>>* out);
  void GetModelSafeRoutingInfo(syncer::ModelSafeRoutingInfo* out);

  // syncer::WorkerLoopDestructionObserver implementation.
  void OnWorkerLoopDestroyed(syncer::ModelSafeGroup group) override;

  // Release ownership of |sync_thread_|. Called when sync is disabled.
  std::unique_ptr<base::Thread> ReleaseSyncThread();

  // Unregister workers from loop destruction observation.
  void Shutdown();

  base::Thread* sync_thread();

 private:
  typedef std::map<syncer::ModelSafeGroup,
                   scoped_refptr<syncer::ModelSafeWorker>> WorkerMap;
  typedef std::map<syncer::ModelType, sync_driver::ChangeProcessor*>
      ProcessorMap;

  // Add a worker for |group| to the worker map if one can be created.
  void MaybeAddWorker(syncer::ModelSafeGroup group);

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
  bool IsCurrentThreadSafeForModel(syncer::ModelType model_type) const;

  // Returns true if the current thread is the native thread for the
  // given group (or if it is undeterminable).
  bool IsOnThreadForGroup(syncer::ModelType type,
                          syncer::ModelSafeGroup group) const;

  // Returns model safe group that should be assigned to type when it is first
  // configured (before activation). Returns GROUP_PASSIVE for directory types
  // and GROUP_NON_BLOCKING for non-blocking types.
  syncer::ModelSafeGroup GetInitialGroupForType(syncer::ModelType type) const;

  // Name used for debugging.
  const std::string name_;

  // A pointer to the sync client.
  sync_driver::SyncClient* const sync_client_;

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
  std::vector<scoped_refptr<syncer::ModelSafeWorker>> stopped_workers_;

  // References to the thread task runners that sync depends on.
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;
  const scoped_refptr<base::SingleThreadTaskRunner> db_thread_;
  const scoped_refptr<base::SingleThreadTaskRunner> file_thread_;

  // Declare |sync_thread_| at the end so that it will be destroyed before
  // objects above because tasks on sync thread depend on those objects,
  // e.g. Shutdown() depends on |lock_|, SyncManager::Init() depends on
  // workers, etc.
  std::unique_ptr<base::Thread> sync_thread_;

  // Set of types with non-blocking implementation (as opposed to directory
  // based).
  syncer::ModelTypeSet non_blocking_types_;

  DISALLOW_COPY_AND_ASSIGN(SyncBackendRegistrar);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SYNC_DRIVER_GLUE_SYNC_BACKEND_REGISTRAR_H_
