// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/glue/sync_backend_registrar.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "components/sync/core/user_share.h"
#include "components/sync/driver/change_processor.h"
#include "components/sync/driver/sync_client.h"

namespace syncer {

SyncBackendRegistrar::SyncBackendRegistrar(
    const std::string& name,
    SyncClient* sync_client,
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

  MaybeAddWorker(GROUP_DB);
  MaybeAddWorker(GROUP_FILE);
  MaybeAddWorker(GROUP_UI);
  MaybeAddWorker(GROUP_PASSIVE);
  MaybeAddWorker(GROUP_HISTORY);
  MaybeAddWorker(GROUP_PASSWORD);

  // Must have at least one worker for SyncBackendRegistrar to be destroyed
  // correctly, as it is destroyed after the last worker dies.
  DCHECK_GT(workers_.size(), 0u);
}

void SyncBackendRegistrar::RegisterNonBlockingType(ModelType type) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  // There may have been a previously successful sync of a type when passive,
  // which is now NonBlocking. We're not sure what order these two sets of types
  // are being registered in, so guard against SetInitialTypes(...) having been
  // already called by undoing everything to these types.
  if (routing_info_.find(type) != routing_info_.end() &&
      routing_info_[type] != GROUP_NON_BLOCKING) {
    routing_info_.erase(type);
    last_configured_types_.Remove(type);
  }
  non_blocking_types_.Put(type);
}

void SyncBackendRegistrar::SetInitialTypes(ModelTypeSet initial_types) {
  base::AutoLock lock(lock_);

  // This function should be called only once, shortly after construction. The
  // routing info at that point is expected to be empty.
  DCHECK(routing_info_.empty());

  // Set our initial state to reflect the current status of the sync directory.
  // This will ensure that our calculations in ConfigureDataTypes() will always
  // return correct results.
  for (ModelTypeSet::Iterator it = initial_types.First(); it.Good(); it.Inc()) {
    // If this type is also registered as NonBlocking, assume that it shouldn't
    // be registered as passive. The NonBlocking path will eventually take care
    // of adding to routing_info_ later on.
    if (!non_blocking_types_.Has(it.Get())) {
      routing_info_[it.Get()] = GROUP_PASSIVE;
    }
  }

  if (!workers_.count(GROUP_HISTORY)) {
    LOG_IF(WARNING, initial_types.Has(TYPED_URLS))
        << "History store disabled, cannot sync Omnibox History";
    routing_info_.erase(TYPED_URLS);
  }

  if (!workers_.count(GROUP_PASSWORD)) {
    LOG_IF(WARNING, initial_types.Has(PASSWORDS))
        << "Password store not initialized, cannot sync passwords";
    routing_info_.erase(PASSWORDS);
  }

  // Although this can re-set NonBlocking types, this should be idempotent.
  last_configured_types_ = GetRoutingInfoTypes(routing_info_);
}

void SyncBackendRegistrar::AddRestoredNonBlockingType(ModelType type) {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  DCHECK(non_blocking_types_.Has(type));
  DCHECK(routing_info_.find(type) == routing_info_.end());
  routing_info_[type] = GROUP_NON_BLOCKING;
  last_configured_types_.Put(type);
}

bool SyncBackendRegistrar::IsNigoriEnabled() const {
  DCHECK(ui_thread_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  return routing_info_.find(NIGORI) != routing_info_.end();
}

ModelTypeSet SyncBackendRegistrar::ConfigureDataTypes(
    ModelTypeSet types_to_add,
    ModelTypeSet types_to_remove) {
  DCHECK(Intersection(types_to_add, types_to_remove).Empty());
  ModelTypeSet filtered_types_to_add = types_to_add;
  if (workers_.count(GROUP_HISTORY) == 0) {
    LOG(WARNING) << "No history worker -- removing TYPED_URLS";
    filtered_types_to_add.Remove(TYPED_URLS);
  }
  if (workers_.count(GROUP_PASSWORD) == 0) {
    LOG(WARNING) << "No password worker -- removing PASSWORDS";
    filtered_types_to_add.Remove(PASSWORDS);
  }

  base::AutoLock lock(lock_);
  ModelTypeSet newly_added_types;
  for (ModelTypeSet::Iterator it = filtered_types_to_add.First(); it.Good();
       it.Inc()) {
    // Add a newly specified data type corresponding initial group into the
    // routing_info, if it does not already exist.
    if (routing_info_.count(it.Get()) == 0) {
      routing_info_[it.Get()] = GetInitialGroupForType(it.Get());
      newly_added_types.Put(it.Get());
    }
  }
  for (ModelTypeSet::Iterator it = types_to_remove.First(); it.Good();
       it.Inc()) {
    routing_info_.erase(it.Get());
  }

  // TODO(akalin): Use SVLOG/SLOG if we add any more logging.
  DVLOG(1) << name_ << ": Adding types " << ModelTypeSetToString(types_to_add)
           << " (with newly-added types "
           << ModelTypeSetToString(newly_added_types) << ") and removing types "
           << ModelTypeSetToString(types_to_remove)
           << " to get new routing info "
           << ModelSafeRoutingInfoToString(routing_info_);
  last_configured_types_ = GetRoutingInfoTypes(routing_info_);

  return newly_added_types;
}

ModelTypeSet SyncBackendRegistrar::GetLastConfiguredTypes() const {
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

void SyncBackendRegistrar::ActivateDataType(ModelType type,
                                            ModelSafeGroup group,
                                            ChangeProcessor* change_processor,
                                            UserShare* user_share) {
  DVLOG(1) << "Activate: " << ModelTypeToString(type);

  base::AutoLock lock(lock_);
  // Ensure that the given data type is in the PASSIVE group.
  ModelSafeRoutingInfo::iterator i = routing_info_.find(type);
  DCHECK(i != routing_info_.end());
  DCHECK_EQ(i->second, GROUP_PASSIVE);
  routing_info_[type] = group;

  // Add the data type's change processor to the list of change
  // processors so it can receive updates.
  DCHECK_EQ(processors_.count(type), 0U);
  processors_[type] = change_processor;

  // Start the change processor.
  change_processor->Start(user_share);
  DCHECK(GetProcessorUnsafe(type));
}

void SyncBackendRegistrar::DeactivateDataType(ModelType type) {
  DVLOG(1) << "Deactivate: " << ModelTypeToString(type);

  DCHECK(ui_thread_->BelongsToCurrentThread() || IsControlType(type));
  base::AutoLock lock(lock_);

  routing_info_.erase(type);
  ignore_result(processors_.erase(type));
  DCHECK(!GetProcessorUnsafe(type));
}

bool SyncBackendRegistrar::IsTypeActivatedForTest(ModelType type) const {
  return GetProcessor(type) != NULL;
}

void SyncBackendRegistrar::OnChangesApplied(
    ModelType model_type,
    int64_t model_version,
    const BaseTransaction* trans,
    const ImmutableChangeRecordList& changes) {
  ChangeProcessor* processor = GetProcessor(model_type);
  if (!processor)
    return;

  processor->ApplyChangesFromSyncModel(trans, model_version, changes);
}

void SyncBackendRegistrar::OnChangesComplete(ModelType model_type) {
  ChangeProcessor* processor = GetProcessor(model_type);
  if (!processor)
    return;

  // This call just notifies the processor that it can commit; it
  // already buffered any changes it plans to makes so needs no
  // further information.
  processor->CommitChangesFromSyncModel();
}

void SyncBackendRegistrar::GetWorkers(
    std::vector<scoped_refptr<ModelSafeWorker>>* out) {
  base::AutoLock lock(lock_);
  out->clear();
  for (WorkerMap::const_iterator it = workers_.begin(); it != workers_.end();
       ++it) {
    out->push_back(it->second.get());
  }
}

void SyncBackendRegistrar::GetModelSafeRoutingInfo(ModelSafeRoutingInfo* out) {
  base::AutoLock lock(lock_);
  ModelSafeRoutingInfo copy(routing_info_);
  out->swap(copy);
}

ChangeProcessor* SyncBackendRegistrar::GetProcessor(ModelType type) const {
  base::AutoLock lock(lock_);
  ChangeProcessor* processor = GetProcessorUnsafe(type);
  if (!processor)
    return NULL;

  // We can only check if |processor| exists, as otherwise the type is
  // mapped to GROUP_PASSIVE.
  CHECK(IsCurrentThreadSafeForModel(type));
  return processor;
}

ChangeProcessor* SyncBackendRegistrar::GetProcessorUnsafe(
    ModelType type) const {
  lock_.AssertAcquired();
  std::map<ModelType, ChangeProcessor*>::const_iterator it =
      processors_.find(type);

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
    ModelType model_type) const {
  lock_.AssertAcquired();
  return IsOnThreadForGroup(model_type,
                            GetGroupForModelType(model_type, routing_info_));
}

bool SyncBackendRegistrar::IsOnThreadForGroup(ModelType type,
                                              ModelSafeGroup group) const {
  switch (group) {
    case GROUP_PASSIVE:
      return IsControlType(type);
    case GROUP_UI:
      return ui_thread_->BelongsToCurrentThread();
    case GROUP_DB:
      return db_thread_->BelongsToCurrentThread();
    case GROUP_FILE:
      return file_thread_->BelongsToCurrentThread();
    case GROUP_HISTORY:
      // TODO(sync): How to check we're on the right thread?
      return type == TYPED_URLS;
    case GROUP_PASSWORD:
      // TODO(sync): How to check we're on the right thread?
      return type == PASSWORDS;
    case GROUP_NON_BLOCKING:
      // IsOnThreadForGroup shouldn't be called for non-blocking types.
      return false;
  }
  NOTREACHED();
  return false;
}

SyncBackendRegistrar::~SyncBackendRegistrar() {
  DCHECK(workers_.empty());
}

void SyncBackendRegistrar::OnWorkerLoopDestroyed(ModelSafeGroup group) {
  RemoveWorker(group);
}

void SyncBackendRegistrar::MaybeAddWorker(ModelSafeGroup group) {
  const scoped_refptr<ModelSafeWorker> worker =
      sync_client_->CreateModelWorkerForGroup(group, this);
  if (worker) {
    DCHECK(workers_.find(group) == workers_.end());
    workers_[group] = worker;
    workers_[group]->RegisterForLoopDestruction();
  }
}

void SyncBackendRegistrar::OnWorkerUnregistrationDone(ModelSafeGroup group) {
  RemoveWorker(group);
}

void SyncBackendRegistrar::RemoveWorker(ModelSafeGroup group) {
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

ModelSafeGroup SyncBackendRegistrar::GetInitialGroupForType(
    ModelType type) const {
  return non_blocking_types_.Has(type) ? GROUP_NON_BLOCKING : GROUP_PASSIVE;
}

}  // namespace syncer
