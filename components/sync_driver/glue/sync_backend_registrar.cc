// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/glue/sync_backend_registrar.h"

#include <cstddef>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "components/sync_driver/change_processor.h"
#include "components/sync_driver/sync_client.h"
#include "sync/internal_api/public/user_share.h"

namespace browser_sync {

SyncBackendRegistrar::SyncBackendRegistrar(
    const std::string& name,
    sync_driver::SyncClient* sync_client,
    std::unique_ptr<base::Thread> sync_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& file_thread)
    : name_(name),
      sync_client_(sync_client),
      ui_thread_(ui_thread),
      db_thread_(db_thread),
      file_thread_(file_thread) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  DCHECK(sync_client_);

  sync_thread_ = std::move(sync_thread);
  if (!sync_thread_) {
    sync_thread_.reset(new base::Thread("Chrome_SyncThread"));
    base::Thread::Options options;
    options.timer_slack = base::TIMER_SLACK_MAXIMUM;
    CHECK(sync_thread_->StartWithOptions(options));
  }

  MaybeAddWorker(syncer::GROUP_DB);
  MaybeAddWorker(syncer::GROUP_FILE);
  MaybeAddWorker(syncer::GROUP_UI);
  MaybeAddWorker(syncer::GROUP_PASSIVE);
  MaybeAddWorker(syncer::GROUP_HISTORY);
  MaybeAddWorker(syncer::GROUP_PASSWORD);

  // Must have at least one worker for SyncBackendRegistrar to be destroyed
  // correctly, as it is destroyed after the last worker dies.
  DCHECK_GT(workers_.size(), 0u);
}

void SyncBackendRegistrar::RegisterNonBlockingType(syncer::ModelType type) {
  DCHECK(routing_info_.find(type) == routing_info_.end() ||
         routing_info_[type] == syncer::GROUP_NON_BLOCKING);
  non_blocking_types_.Put(type);
}

void SyncBackendRegistrar::SetInitialTypes(syncer::ModelTypeSet initial_types) {
  base::AutoLock lock(lock_);

  // This function should be called only once, shortly after construction. The
  // routing info at that point is expected to be empty.
  DCHECK(routing_info_.empty());

  // Set our initial state to reflect the current status of the sync directory.
  // This will ensure that our calculations in ConfigureDataTypes() will always
  // return correct results.
  for (syncer::ModelTypeSet::Iterator it = initial_types.First(); it.Good();
       it.Inc()) {
    routing_info_[it.Get()] = GetInitialGroupForType(it.Get());
  }

  if (!workers_.count(syncer::GROUP_HISTORY)) {
    LOG_IF(WARNING, initial_types.Has(syncer::TYPED_URLS))
        << "History store disabled, cannot sync Omnibox History";
    routing_info_.erase(syncer::TYPED_URLS);
  }

  if (!workers_.count(syncer::GROUP_PASSWORD)) {
    LOG_IF(WARNING, initial_types.Has(syncer::PASSWORDS))
        << "Password store not initialized, cannot sync passwords";
    routing_info_.erase(syncer::PASSWORDS);
  }

  last_configured_types_ = syncer::GetRoutingInfoTypes(routing_info_);
}

bool SyncBackendRegistrar::IsNigoriEnabled() const {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  return routing_info_.find(syncer::NIGORI) != routing_info_.end();
}

syncer::ModelTypeSet SyncBackendRegistrar::ConfigureDataTypes(
    syncer::ModelTypeSet types_to_add,
    syncer::ModelTypeSet types_to_remove) {
  DCHECK(Intersection(types_to_add, types_to_remove).Empty());
  syncer::ModelTypeSet filtered_types_to_add = types_to_add;
  if (workers_.count(syncer::GROUP_HISTORY) == 0) {
    LOG(WARNING) << "No history worker -- removing TYPED_URLS";
    filtered_types_to_add.Remove(syncer::TYPED_URLS);
  }
  if (workers_.count(syncer::GROUP_PASSWORD) == 0) {
    LOG(WARNING) << "No password worker -- removing PASSWORDS";
    filtered_types_to_add.Remove(syncer::PASSWORDS);
  }

  base::AutoLock lock(lock_);
  syncer::ModelTypeSet newly_added_types;
  for (syncer::ModelTypeSet::Iterator it = filtered_types_to_add.First();
       it.Good(); it.Inc()) {
    // Add a newly specified data type as syncer::GROUP_PASSIVE into the
    // routing_info, if it does not already exist.
    if (routing_info_.count(it.Get()) == 0) {
      routing_info_[it.Get()] = GetInitialGroupForType(it.Get());
      newly_added_types.Put(it.Get());
    }
  }
  for (syncer::ModelTypeSet::Iterator it = types_to_remove.First(); it.Good();
       it.Inc()) {
    routing_info_.erase(it.Get());
  }

  // TODO(akalin): Use SVLOG/SLOG if we add any more logging.
  DVLOG(1) << name_ << ": Adding types "
           << syncer::ModelTypeSetToString(types_to_add)
           << " (with newly-added types "
           << syncer::ModelTypeSetToString(newly_added_types)
           << ") and removing types "
           << syncer::ModelTypeSetToString(types_to_remove)
           << " to get new routing info "
           << syncer::ModelSafeRoutingInfoToString(routing_info_);
  last_configured_types_ = syncer::GetRoutingInfoTypes(routing_info_);

  return newly_added_types;
}

syncer::ModelTypeSet SyncBackendRegistrar::GetLastConfiguredTypes() const {
  return last_configured_types_;
}

void SyncBackendRegistrar::RequestWorkerStopOnUIThread() {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  for (WorkerMap::const_iterator it = workers_.begin(); it != workers_.end();
       ++it) {
    it->second->RequestStop();
  }
}

void SyncBackendRegistrar::ActivateDataType(
    syncer::ModelType type,
    syncer::ModelSafeGroup group,
    sync_driver::ChangeProcessor* change_processor,
    syncer::UserShare* user_share) {
  DVLOG(1) << "Activate: " << syncer::ModelTypeToString(type);

  base::AutoLock lock(lock_);
  // Ensure that the given data type is in the PASSIVE group.
  syncer::ModelSafeRoutingInfo::iterator i = routing_info_.find(type);
  DCHECK(i != routing_info_.end());
  DCHECK_EQ(i->second, syncer::GROUP_PASSIVE);
  routing_info_[type] = group;

  // Add the data type's change processor to the list of change
  // processors so it can receive updates.
  DCHECK_EQ(processors_.count(type), 0U);
  processors_[type] = change_processor;

  // Start the change processor.
  change_processor->Start(user_share);
  DCHECK(GetProcessorUnsafe(type));
}

void SyncBackendRegistrar::DeactivateDataType(syncer::ModelType type) {
  DVLOG(1) << "Deactivate: " << syncer::ModelTypeToString(type);

  DCHECK(ui_thread_->BelongsToCurrentThread() || IsControlType(type));
  base::AutoLock lock(lock_);

  routing_info_.erase(type);
  ignore_result(processors_.erase(type));
  DCHECK(!GetProcessorUnsafe(type));
}

bool SyncBackendRegistrar::IsTypeActivatedForTest(
    syncer::ModelType type) const {
  return GetProcessor(type) != NULL;
}

void SyncBackendRegistrar::OnChangesApplied(
    syncer::ModelType model_type,
    int64_t model_version,
    const syncer::BaseTransaction* trans,
    const syncer::ImmutableChangeRecordList& changes) {
  sync_driver::ChangeProcessor* processor = GetProcessor(model_type);
  if (!processor)
    return;

  processor->ApplyChangesFromSyncModel(trans, model_version, changes);
}

void SyncBackendRegistrar::OnChangesComplete(syncer::ModelType model_type) {
  sync_driver::ChangeProcessor* processor = GetProcessor(model_type);
  if (!processor)
    return;

  // This call just notifies the processor that it can commit; it
  // already buffered any changes it plans to makes so needs no
  // further information.
  processor->CommitChangesFromSyncModel();
}

void SyncBackendRegistrar::GetWorkers(
    std::vector<scoped_refptr<syncer::ModelSafeWorker>>* out) {
  base::AutoLock lock(lock_);
  out->clear();
  for (WorkerMap::const_iterator it = workers_.begin(); it != workers_.end();
       ++it) {
    out->push_back(it->second.get());
  }
}

void SyncBackendRegistrar::GetModelSafeRoutingInfo(
    syncer::ModelSafeRoutingInfo* out) {
  base::AutoLock lock(lock_);
  syncer::ModelSafeRoutingInfo copy(routing_info_);
  out->swap(copy);
}

sync_driver::ChangeProcessor* SyncBackendRegistrar::GetProcessor(
    syncer::ModelType type) const {
  base::AutoLock lock(lock_);
  sync_driver::ChangeProcessor* processor = GetProcessorUnsafe(type);
  if (!processor)
    return NULL;

  // We can only check if |processor| exists, as otherwise the type is
  // mapped to syncer::GROUP_PASSIVE.
  CHECK(IsCurrentThreadSafeForModel(type));
  return processor;
}

sync_driver::ChangeProcessor* SyncBackendRegistrar::GetProcessorUnsafe(
    syncer::ModelType type) const {
  lock_.AssertAcquired();
  std::map<syncer::ModelType, sync_driver::ChangeProcessor*>::const_iterator
      it = processors_.find(type);

  // Until model association happens for a datatype, it will not
  // appear in the processors list.  During this time, it is OK to
  // drop changes on the floor (since model association has not
  // happened yet).  When the data type is activated, model
  // association takes place then the change processor is added to the
  // |processors_| list.
  if (it == processors_.end())
    return NULL;

  return it->second;
}

bool SyncBackendRegistrar::IsCurrentThreadSafeForModel(
    syncer::ModelType model_type) const {
  lock_.AssertAcquired();
  return IsOnThreadForGroup(model_type,
                            GetGroupForModelType(model_type, routing_info_));
}

bool SyncBackendRegistrar::IsOnThreadForGroup(
    syncer::ModelType type,
    syncer::ModelSafeGroup group) const {
  switch (group) {
    case syncer::GROUP_PASSIVE:
      return IsControlType(type);
    case syncer::GROUP_UI:
      return ui_thread_->BelongsToCurrentThread();
    case syncer::GROUP_DB:
      return db_thread_->BelongsToCurrentThread();
    case syncer::GROUP_FILE:
      return file_thread_->BelongsToCurrentThread();
    case syncer::GROUP_HISTORY:
      // TODO(sync): How to check we're on the right thread?
      return type == syncer::TYPED_URLS;
    case syncer::GROUP_PASSWORD:
      // TODO(sync): How to check we're on the right thread?
      return type == syncer::PASSWORDS;
    case syncer::GROUP_NON_BLOCKING:
      // IsOnThreadForGroup shouldn't be called for non-blocking types.
      return false;
  }
  NOTREACHED();
  return false;
}

SyncBackendRegistrar::~SyncBackendRegistrar() {
  DCHECK(workers_.empty());
}

void SyncBackendRegistrar::OnWorkerLoopDestroyed(syncer::ModelSafeGroup group) {
  RemoveWorker(group);
}

void SyncBackendRegistrar::MaybeAddWorker(syncer::ModelSafeGroup group) {
  const scoped_refptr<syncer::ModelSafeWorker> worker =
      sync_client_->CreateModelWorkerForGroup(group, this);
  if (worker) {
    DCHECK(workers_.find(group) == workers_.end());
    workers_[group] = worker;
    workers_[group]->RegisterForLoopDestruction();
  }
}

void SyncBackendRegistrar::OnWorkerUnregistrationDone(
    syncer::ModelSafeGroup group) {
  RemoveWorker(group);
}

void SyncBackendRegistrar::RemoveWorker(syncer::ModelSafeGroup group) {
  DVLOG(1) << "Remove " << ModelSafeGroupToString(group) << " worker.";

  bool last_worker = false;
  {
    base::AutoLock al(lock_);
    WorkerMap::iterator it = workers_.find(group);
    CHECK(it != workers_.end());
    stopped_workers_.push_back(it->second);
    workers_.erase(it);
    last_worker = workers_.empty();
  }

  if (last_worker) {
    // Self-destruction after last worker.
    DVLOG(1) << "Destroy registrar on loop of "
             << ModelSafeGroupToString(group);
    delete this;
  }
}

std::unique_ptr<base::Thread> SyncBackendRegistrar::ReleaseSyncThread() {
  return std::move(sync_thread_);
}

void SyncBackendRegistrar::Shutdown() {
  // All data types should have been deactivated by now.
  DCHECK(processors_.empty());

  // Unregister worker from observing loop destruction.
  base::AutoLock al(lock_);
  for (WorkerMap::iterator it = workers_.begin(); it != workers_.end(); ++it) {
    it->second->UnregisterForLoopDestruction(
        base::Bind(&SyncBackendRegistrar::OnWorkerUnregistrationDone,
                   base::Unretained(this)));
  }
}

base::Thread* SyncBackendRegistrar::sync_thread() {
  return sync_thread_.get();
}

syncer::ModelSafeGroup SyncBackendRegistrar::GetInitialGroupForType(
    syncer::ModelType type) const {
  if (non_blocking_types_.Has(type))
    return syncer::GROUP_NON_BLOCKING;
  else
    return syncer::GROUP_PASSIVE;
}

}  // namespace browser_sync
