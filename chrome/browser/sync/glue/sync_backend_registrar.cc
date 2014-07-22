// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_registrar.h"

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/browser_thread_model_worker.h"
#include "chrome/browser/sync/glue/history_model_worker.h"
#include "chrome/browser/sync/glue/password_model_worker.h"
#include "chrome/browser/sync/glue/ui_model_worker.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/sync_driver/change_processor.h"
#include "content/public/browser/browser_thread.h"
#include "sync/internal_api/public/engine/passive_model_worker.h"
#include "sync/internal_api/public/user_share.h"

using content::BrowserThread;

namespace browser_sync {

namespace {

// Returns true if the current thread is the native thread for the
// given group (or if it is undeterminable).
bool IsOnThreadForGroup(syncer::ModelType type, syncer::ModelSafeGroup group) {
  switch (group) {
    case syncer::GROUP_PASSIVE:
      return IsControlType(type);
    case syncer::GROUP_UI:
      return BrowserThread::CurrentlyOn(BrowserThread::UI);
    case syncer::GROUP_DB:
      return BrowserThread::CurrentlyOn(BrowserThread::DB);
    case syncer::GROUP_FILE:
      return BrowserThread::CurrentlyOn(BrowserThread::FILE);
    case syncer::GROUP_HISTORY:
      // TODO(sync): How to check we're on the right thread?
      return type == syncer::TYPED_URLS;
    case syncer::GROUP_PASSWORD:
      // TODO(sync): How to check we're on the right thread?
      return type == syncer::PASSWORDS;
    case syncer::MODEL_SAFE_GROUP_COUNT:
    default:
      return false;
  }
}

}  // namespace

SyncBackendRegistrar::SyncBackendRegistrar(
    const std::string& name,
    Profile* profile,
    scoped_ptr<base::Thread> sync_thread) :
    name_(name),
    profile_(profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(profile_);

  sync_thread_ = sync_thread.Pass();
  if (!sync_thread_) {
    sync_thread_.reset(new base::Thread("Chrome_SyncThread"));
    base::Thread::Options options;
    options.timer_slack = base::TIMER_SLACK_MAXIMUM;
    CHECK(sync_thread_->StartWithOptions(options));
  }

  workers_[syncer::GROUP_DB] = new DatabaseModelWorker(this);
  workers_[syncer::GROUP_DB]->RegisterForLoopDestruction();

  workers_[syncer::GROUP_FILE] = new FileModelWorker(this);
  workers_[syncer::GROUP_FILE]->RegisterForLoopDestruction();

  workers_[syncer::GROUP_UI] = new UIModelWorker(this);
  workers_[syncer::GROUP_UI]->RegisterForLoopDestruction();

  // GROUP_PASSIVE worker does work on sync_loop_. But sync_loop_ is not
  // stopped until all workers have stopped. To break the cycle, use UI loop
  // instead.
  workers_[syncer::GROUP_PASSIVE] =
      new syncer::PassiveModelWorker(sync_thread_->message_loop(), this);
  workers_[syncer::GROUP_PASSIVE]->RegisterForLoopDestruction();

  HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile, Profile::IMPLICIT_ACCESS);
  if (history_service) {
    workers_[syncer::GROUP_HISTORY] =
        new HistoryModelWorker(history_service->AsWeakPtr(), this);
    workers_[syncer::GROUP_HISTORY]->RegisterForLoopDestruction();

  }

  scoped_refptr<password_manager::PasswordStore> password_store =
      PasswordStoreFactory::GetForProfile(profile, Profile::IMPLICIT_ACCESS);
  if (password_store.get()) {
    workers_[syncer::GROUP_PASSWORD] =
        new PasswordModelWorker(password_store, this);
    workers_[syncer::GROUP_PASSWORD]->RegisterForLoopDestruction();
  }
}

void SyncBackendRegistrar::SetInitialTypes(syncer::ModelTypeSet initial_types) {
  base::AutoLock lock(lock_);

  // This function should be called only once, shortly after construction. The
  // routing info at that point is expected to be empty.
  DCHECK(routing_info_.empty());

  // Set our initial state to reflect the current status of the sync directory.
  // This will ensure that our calculations in ConfigureDataTypes() will always
  // return correct results.
  for (syncer::ModelTypeSet::Iterator it = initial_types.First();
       it.Good(); it.Inc()) {
    routing_info_[it.Get()] = syncer::GROUP_PASSIVE;
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  for (syncer::ModelTypeSet::Iterator it =
           filtered_types_to_add.First();
       it.Good(); it.Inc()) {
    // Add a newly specified data type as syncer::GROUP_PASSIVE into the
    // routing_info, if it does not already exist.
    if (routing_info_.count(it.Get()) == 0) {
      routing_info_[it.Get()] = syncer::GROUP_PASSIVE;
      newly_added_types.Put(it.Get());
    }
  }
  for (syncer::ModelTypeSet::Iterator it = types_to_remove.First();
       it.Good(); it.Inc()) {
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
           <<syncer::ModelSafeRoutingInfoToString(routing_info_);
  last_configured_types_ = syncer::GetRoutingInfoTypes(routing_info_);

  return newly_added_types;
}

syncer::ModelTypeSet SyncBackendRegistrar::GetLastConfiguredTypes() const {
  return last_configured_types_;
}

void SyncBackendRegistrar::RequestWorkerStopOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(lock_);
  for (WorkerMap::const_iterator it = workers_.begin();
       it != workers_.end(); ++it) {
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

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) || IsControlType(type));
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
    int64 model_version,
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
    std::vector<scoped_refptr<syncer::ModelSafeWorker> >* out) {
  base::AutoLock lock(lock_);
  out->clear();
  for (WorkerMap::const_iterator it = workers_.begin();
       it != workers_.end(); ++it) {
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

SyncBackendRegistrar::~SyncBackendRegistrar() {
  DCHECK(workers_.empty());
}

void SyncBackendRegistrar::OnWorkerLoopDestroyed(syncer::ModelSafeGroup group) {
  RemoveWorker(group);
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

scoped_ptr<base::Thread> SyncBackendRegistrar::ReleaseSyncThread() {
  return sync_thread_.Pass();
}

void SyncBackendRegistrar::Shutdown() {
  // All data types should have been deactivated by now.
  DCHECK(processors_.empty());

  // Unregister worker from observing loop destruction.
  base::AutoLock al(lock_);
  for (WorkerMap::iterator it = workers_.begin();
      it != workers_.end(); ++it) {
    it->second->UnregisterForLoopDestruction(
        base::Bind(&SyncBackendRegistrar::OnWorkerUnregistrationDone,
                   base::Unretained(this)));
  }
}

base::Thread* SyncBackendRegistrar::sync_thread() {
  return sync_thread_.get();
}

}  // namespace browser_sync
