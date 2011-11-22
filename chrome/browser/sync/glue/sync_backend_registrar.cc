// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/sync_backend_registrar.h"

#include <algorithm>
#include <cstddef>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/engine/passive_model_worker.h"
#include "chrome/browser/sync/glue/browser_thread_model_worker.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/history_model_worker.h"
#include "chrome/browser/sync/glue/password_model_worker.h"
#include "chrome/browser/sync/glue/ui_model_worker.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

namespace {

// Returns true if the current thread is the native thread for the
// given group (or if it is undeterminable).
bool IsOnThreadForGroup(ModelSafeGroup group) {
  switch (group) {
    case GROUP_PASSIVE:
      return false;
    case GROUP_UI:
      return BrowserThread::CurrentlyOn(BrowserThread::UI);
    case GROUP_DB:
      return BrowserThread::CurrentlyOn(BrowserThread::DB);
    case GROUP_FILE:
      return BrowserThread::CurrentlyOn(BrowserThread::FILE);
    case GROUP_HISTORY:
      // TODO(ncarter): How to determine this?
      return true;
    case GROUP_PASSWORD:
      // TODO(ncarter): How to determine this?
      return true;
    case MODEL_SAFE_GROUP_COUNT:
    default:
      return false;
  }
}

}  // namespace

SyncBackendRegistrar::SyncBackendRegistrar(
    const syncable::ModelTypeSet& initial_types,
    const std::string& name, Profile* profile,
    MessageLoop* sync_loop) :
    name_(name),
    profile_(profile),
    sync_loop_(sync_loop),
    ui_worker_(new UIModelWorker()),
    stopped_on_ui_thread_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(profile_);
  DCHECK(sync_loop_);
  workers_[GROUP_DB] = new DatabaseModelWorker();
  workers_[GROUP_FILE] = new FileModelWorker();
  workers_[GROUP_UI] = ui_worker_;
  workers_[GROUP_PASSIVE] = new PassiveModelWorker(sync_loop_);

  // Any datatypes that we want the syncer to pull down must be in the
  // routing_info map.  We set them to group passive, meaning that
  // updates will be applied to sync, but not dispatched to the native
  // models.
  for (syncable::ModelTypeSet::const_iterator it = initial_types.begin();
      it != initial_types.end(); ++it) {
    routing_info_[*it] = GROUP_PASSIVE;
  }

  HistoryService* history_service = profile->GetHistoryService(
      Profile::IMPLICIT_ACCESS);
  if (history_service) {
    workers_[GROUP_HISTORY] = new HistoryModelWorker(history_service);
  } else {
    LOG_IF(WARNING, initial_types.count(syncable::TYPED_URLS) > 0)
        << "History store disabled, cannot sync Omnibox History";
    routing_info_.erase(syncable::TYPED_URLS);
  }

  PasswordStore* password_store =
      profile->GetPasswordStore(Profile::IMPLICIT_ACCESS);
  if (password_store) {
    workers_[GROUP_PASSWORD] = new PasswordModelWorker(password_store);
  } else {
    LOG_IF(WARNING, initial_types.count(syncable::PASSWORDS) > 0)
        << "Password store not initialized, cannot sync passwords";
    routing_info_.erase(syncable::PASSWORDS);
  }
}

SyncBackendRegistrar::~SyncBackendRegistrar() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(stopped_on_ui_thread_);
}

bool SyncBackendRegistrar::IsNigoriEnabled() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(lock_);
  return routing_info_.find(syncable::NIGORI) != routing_info_.end();
}

syncable::ModelTypeSet SyncBackendRegistrar::ConfigureDataTypes(
    const syncable::ModelTypeSet& types_to_add,
    const syncable::ModelTypeSet& types_to_remove) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (DCHECK_IS_ON()) {
    syncable::ModelTypeSet intersection;
    std::set_intersection(types_to_add.begin(), types_to_add.end(),
                          types_to_remove.begin(), types_to_remove.end(),
                          std::inserter(intersection, intersection.end()));
    DCHECK(intersection.empty());
  }
  syncable::ModelTypeSet filtered_types_to_add = types_to_add;
  if (workers_.count(GROUP_HISTORY) == 0) {
    LOG(WARNING) << "No history worker -- removing TYPED_URLS";
    filtered_types_to_add.erase(syncable::TYPED_URLS);
  }
  if (workers_.count(GROUP_PASSWORD) == 0) {
    LOG(WARNING) << "No password worker -- removing PASSWORDS";
    filtered_types_to_add.erase(syncable::PASSWORDS);
  }

  base::AutoLock lock(lock_);
  syncable::ModelTypeSet newly_added_types;
  for (syncable::ModelTypeSet::const_iterator it =
           filtered_types_to_add.begin();
       it != filtered_types_to_add.end(); ++it) {
    // Add a newly specified data type as GROUP_PASSIVE into the
    // routing_info, if it does not already exist.
    if (routing_info_.count(*it) == 0) {
      routing_info_[*it] = GROUP_PASSIVE;
      newly_added_types.insert(*it);
    }
  }
  for (syncable::ModelTypeSet::const_iterator it = types_to_remove.begin();
       it != types_to_remove.end(); ++it) {
    routing_info_.erase(*it);
  }

  // TODO(akalin): Use SVLOG/SLOG if we add any more logging.
  VLOG(1) << name_ << ": Adding types "
          << syncable::ModelTypeSetToString(types_to_add)
          << " (with newly-added types "
          << syncable::ModelTypeSetToString(newly_added_types)
          << ") and removing types "
          << syncable::ModelTypeSetToString(types_to_remove)
          << " to get new routing info "
          << ModelSafeRoutingInfoToString(routing_info_);

  return newly_added_types;
}

void SyncBackendRegistrar::StopOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!stopped_on_ui_thread_);
  ui_worker_->Stop();
  stopped_on_ui_thread_ = true;
}

void SyncBackendRegistrar::OnSyncerShutdownComplete() {
  DCHECK_EQ(MessageLoop::current(), sync_loop_);
  ui_worker_->OnSyncerShutdownComplete();
}

void SyncBackendRegistrar::ActivateDataType(
    syncable::ModelType type,
    ModelSafeGroup group,
    ChangeProcessor* change_processor,
    sync_api::UserShare* user_share) {
  CHECK(IsOnThreadForGroup(group));
  base::AutoLock lock(lock_);
  // Ensure that the given data type is in the PASSIVE group.
  ModelSafeRoutingInfo::iterator i = routing_info_.find(type);
  DCHECK(i != routing_info_.end());
  DCHECK_EQ(i->second, GROUP_PASSIVE);
  routing_info_[type] = group;
  CHECK(IsCurrentThreadSafeForModel(type));

  // Add the data type's change processor to the list of change
  // processors so it can receive updates.
  DCHECK_EQ(processors_.count(type), 0U);
  processors_[type] = change_processor;

  // Start the change processor.
  change_processor->Start(profile_, user_share);
  DCHECK(GetProcessorUnsafe(type));
}

void SyncBackendRegistrar::DeactivateDataType(syncable::ModelType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::AutoLock lock(lock_);
  ChangeProcessor* change_processor = GetProcessorUnsafe(type);
  if (change_processor)
    change_processor->Stop();

  routing_info_.erase(type);
  ignore_result(processors_.erase(type));
  DCHECK(!GetProcessorUnsafe(type));
}

bool SyncBackendRegistrar::IsTypeActivatedForTest(
    syncable::ModelType type) const {
  return GetProcessor(type) != NULL;
}

void SyncBackendRegistrar::OnChangesApplied(
    syncable::ModelType model_type,
    const sync_api::BaseTransaction* trans,
    const sync_api::ImmutableChangeRecordList& changes) {
  ChangeProcessor* processor = GetProcessor(model_type);
  if (!processor)
    return;

  processor->ApplyChangesFromSyncModel(trans, changes);
}

void SyncBackendRegistrar::OnChangesComplete(
    syncable::ModelType model_type) {
  ChangeProcessor* processor = GetProcessor(model_type);
  if (!processor)
    return;

  // This call just notifies the processor that it can commit; it
  // already buffered any changes it plans to makes so needs no
  // further information.
  processor->CommitChangesFromSyncModel();
}

void SyncBackendRegistrar::GetWorkers(
    std::vector<ModelSafeWorker*>* out) {
  base::AutoLock lock(lock_);
  out->clear();
  for (WorkerMap::const_iterator it = workers_.begin();
       it != workers_.end(); ++it) {
    out->push_back(it->second);
  }
}

void SyncBackendRegistrar::GetModelSafeRoutingInfo(
    ModelSafeRoutingInfo* out) {
  base::AutoLock lock(lock_);
  ModelSafeRoutingInfo copy(routing_info_);
  out->swap(copy);
}

ChangeProcessor* SyncBackendRegistrar::GetProcessor(
    syncable::ModelType type) const {
  base::AutoLock lock(lock_);
  ChangeProcessor* processor = GetProcessorUnsafe(type);
  if (!processor)
    return NULL;

  // We can only check if |processor| exists, as otherwise the type is
  // mapped to GROUP_PASSIVE.
  CHECK(IsCurrentThreadSafeForModel(type));
  CHECK(processor->IsRunning());
  return processor;
}

ChangeProcessor* SyncBackendRegistrar::GetProcessorUnsafe(
    syncable::ModelType type) const {
  lock_.AssertAcquired();
  std::map<syncable::ModelType, ChangeProcessor*>::const_iterator it =
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
    syncable::ModelType model_type) const {
  lock_.AssertAcquired();
  return IsOnThreadForGroup(GetGroupForModelType(model_type, routing_info_));
}

}  // namespace browser_sync
