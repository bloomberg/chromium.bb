// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/model_association_manager.h"

#include <algorithm>
#include <functional>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "sync/internal_api/public/base/model_type.h"

using syncer::ModelTypeSet;

namespace sync_driver {

namespace {

static const syncer::ModelType kStartOrder[] = {
  syncer::NIGORI,               //  Listed for completeness.
  syncer::DEVICE_INFO,          //  Listed for completeness.
  syncer::EXPERIMENTS,          //  Listed for completeness.
  syncer::PROXY_TABS,           //  Listed for completeness.

  // Kick off the association of the non-UI types first so they can associate
  // in parallel with the UI types.
  syncer::PASSWORDS,
  syncer::AUTOFILL,
  syncer::AUTOFILL_PROFILE,
  syncer::EXTENSION_SETTINGS,
  syncer::APP_SETTINGS,
  syncer::TYPED_URLS,
  syncer::HISTORY_DELETE_DIRECTIVES,
  syncer::SYNCED_NOTIFICATIONS,
  syncer::SYNCED_NOTIFICATION_APP_INFO,

  // UI thread data types.
  syncer::BOOKMARKS,
  syncer::SUPERVISED_USERS,     //  Syncing supervised users on initial login
                                //  might block creating a new supervised user,
                                //  so we want to do it early.
  syncer::PREFERENCES,
  syncer::PRIORITY_PREFERENCES,
  syncer::EXTENSIONS,
  syncer::APPS,
  syncer::APP_LIST,
  syncer::THEMES,
  syncer::SEARCH_ENGINES,
  syncer::SESSIONS,
  syncer::APP_NOTIFICATIONS,
  syncer::DICTIONARY,
  syncer::FAVICON_IMAGES,
  syncer::FAVICON_TRACKING,
  syncer::SUPERVISED_USER_SETTINGS,
  syncer::SUPERVISED_USER_SHARED_SETTINGS,
  syncer::ARTICLES,
};

COMPILE_ASSERT(arraysize(kStartOrder) ==
               syncer::MODEL_TYPE_COUNT - syncer::FIRST_REAL_MODEL_TYPE,
               kStartOrder_IncorrectSize);

// The amount of time we wait for association to finish. If some types haven't
// finished association by the time, configuration result will be
// PARTIAL_SUCCESS and DataTypeManager is notified of the unfinished types.
const int64 kAssociationTimeOutInSeconds = 600;

syncer::DataTypeAssociationStats BuildAssociationStatsFromMergeResults(
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result,
    const base::TimeDelta& association_wait_time,
    const base::TimeDelta& association_time) {
  DCHECK_EQ(local_merge_result.model_type(), syncer_merge_result.model_type());
  syncer::DataTypeAssociationStats stats;
  stats.had_error = local_merge_result.error().IsSet() ||
                    syncer_merge_result.error().IsSet();
  stats.num_local_items_before_association =
      local_merge_result.num_items_before_association();
  stats.num_sync_items_before_association =
      syncer_merge_result.num_items_before_association();
  stats.num_local_items_after_association =
      local_merge_result.num_items_after_association();
  stats.num_sync_items_after_association =
      syncer_merge_result.num_items_after_association();
  stats.num_local_items_added =
      local_merge_result.num_items_added();
  stats.num_local_items_deleted =
      local_merge_result.num_items_deleted();
  stats.num_local_items_modified =
      local_merge_result.num_items_modified();
  stats.local_version_pre_association =
      local_merge_result.pre_association_version();
  stats.num_sync_items_added =
      syncer_merge_result.num_items_added();
  stats.num_sync_items_deleted =
      syncer_merge_result.num_items_deleted();
  stats.num_sync_items_modified =
      syncer_merge_result.num_items_modified();
  stats.sync_version_pre_association =
      syncer_merge_result.pre_association_version();
  stats.association_wait_time = association_wait_time;
  stats.association_time = association_time;
  return stats;
}

}  // namespace

ModelAssociationManager::ModelAssociationManager(
    const DataTypeController::TypeMap* controllers,
    ModelAssociationManagerDelegate* processor)
    : state_(IDLE),
      controllers_(controllers),
      delegate_(processor),
      weak_ptr_factory_(this),
      configure_status_(DataTypeManager::UNKNOWN) {
  // Ensure all data type controllers are stopped.
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DCHECK_EQ(DataTypeController::NOT_RUNNING, (*it).second->state());
  }
}

ModelAssociationManager::~ModelAssociationManager() {
}

void ModelAssociationManager::Initialize(syncer::ModelTypeSet desired_types) {
  // state_ can be INITIALIZED_TO_CONFIGURE if types are reconfigured when
  // data is being downloaded, so StartAssociationAsync() is never called for
  // the first configuration.
  DCHECK_NE(CONFIGURING, state_);

  // Only keep types that have controllers.
  desired_types_.Clear();
  slow_types_.Clear();
  for (syncer::ModelTypeSet::Iterator it = desired_types.First();
      it.Good(); it.Inc()) {
    if (controllers_->find(it.Get()) != controllers_->end())
      desired_types_.Put(it.Get());
  }

  DVLOG(1) << "ModelAssociationManager: Initializing for "
           << syncer::ModelTypeSetToString(desired_types_);

  state_ = INITIALIZED_TO_CONFIGURE;

  StopDisabledTypes();
  LoadEnabledTypes();
}

void ModelAssociationManager::StopDatatype(DataTypeController* dtc) {
  // First tell the sync backend that we no longer want to listen to
  // changes for this type.
  delegate_->OnSingleDataTypeWillStop(dtc->type());

  // Then tell all data type specific logic to shut down.
  dtc->Stop();
}

void ModelAssociationManager::StopDisabledTypes() {
  DVLOG(1) << "ModelAssociationManager: Stopping disabled types.";
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DataTypeController* dtc = (*it).second.get();
    if (dtc->state() != DataTypeController::NOT_RUNNING &&
        (!desired_types_.Has(dtc->type()) ||
            failed_data_types_info_.count(dtc->type()) > 0)) {
      DVLOG(1) << "ModelTypeToString: stop " << dtc->name();
      StopDatatype(dtc);
      loaded_types_.Remove(dtc->type());
      associated_types_.Remove(dtc->type());
    }
  }
}

void ModelAssociationManager::LoadEnabledTypes() {
  // Load in kStartOrder.
  for (size_t i = 0; i < arraysize(kStartOrder); i++) {
    syncer::ModelType type = kStartOrder[i];
    if (!desired_types_.Has(type))
      continue;

    DCHECK(controllers_->find(type) != controllers_->end());
    DataTypeController* dtc = controllers_->find(type)->second.get();
    if (dtc->state() == DataTypeController::NOT_RUNNING) {
      DCHECK(!loaded_types_.Has(dtc->type()));
      DCHECK(!associated_types_.Has(dtc->type()));
      dtc->LoadModels(base::Bind(&ModelAssociationManager::ModelLoadCallback,
                                 weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void ModelAssociationManager::StartAssociationAsync(
    const syncer::ModelTypeSet& types_to_associate) {
  DCHECK_NE(CONFIGURING, state_);
  state_ = CONFIGURING;

  association_start_time_ = base::TimeTicks::Now();

  requested_types_ = types_to_associate;

  associating_types_ = types_to_associate;
  associating_types_.RetainAll(desired_types_);
  associating_types_.RemoveAll(associated_types_);

  // Assume success.
  configure_status_ = DataTypeManager::OK;

  // Remove types that already failed.
  for (std::map<syncer::ModelType, syncer::SyncError>::const_iterator it =
          failed_data_types_info_.begin();
      it != failed_data_types_info_.end(); ++it) {
    associating_types_.Remove(it->first);
  }

  // Done if no types to associate.
  if (associating_types_.Empty()) {
    ModelAssociationDone();
    return;
  }

  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kAssociationTimeOutInSeconds),
               this,
               &ModelAssociationManager::ModelAssociationDone);

  // Start association of types that are loaded in specified order.
  for (size_t i = 0; i < arraysize(kStartOrder); i++) {
    syncer::ModelType type = kStartOrder[i];
    if (!associating_types_.Has(type) || !loaded_types_.Has(type))
      continue;

    DataTypeController* dtc = controllers_->find(type)->second.get();
    DCHECK(DataTypeController::MODEL_LOADED == dtc->state() ||
           DataTypeController::ASSOCIATING == dtc->state());
    if (dtc->state() == DataTypeController::MODEL_LOADED) {
      TRACE_EVENT_ASYNC_BEGIN1("sync", "ModelAssociation",
                               dtc,
                               "DataType",
                               ModelTypeToString(type));

      dtc->StartAssociating(
          base::Bind(&ModelAssociationManager::TypeStartCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     type, base::TimeTicks::Now()));
    }
  }
}

void ModelAssociationManager::ResetForNextAssociation() {
  DVLOG(1) << "ModelAssociationManager: Reseting for next configuration";
  // |loaded_types_| and |associated_types_| are not cleared. So
  // reconfiguration won't restart types that are already started.
  requested_types_.Clear();
  failed_data_types_info_.clear();
  associating_types_.Clear();
  needs_crypto_types_.Clear();
}

void ModelAssociationManager::Stop() {
  // Ignore callbacks from controllers.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Stop started data types.
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DataTypeController* dtc = (*it).second.get();
    if (dtc->state() != DataTypeController::NOT_RUNNING) {
      StopDatatype(dtc);
      DVLOG(1) << "ModelAssociationManager: Stopped " << dtc->name();
    }
  }

  desired_types_.Clear();
  loaded_types_.Clear();
  associated_types_.Clear();
  slow_types_.Clear();

  if (state_ == CONFIGURING) {
    if (configure_status_ == DataTypeManager::OK)
      configure_status_ = DataTypeManager::ABORTED;
    DVLOG(1) << "ModelAssociationManager: Calling OnModelAssociationDone";
    ModelAssociationDone();
  }

  ResetForNextAssociation();

  state_ = IDLE;
}

void ModelAssociationManager::AppendToFailedDatatypesAndLogError(
    const syncer::SyncError& error) {
  failed_data_types_info_[error.model_type()] = error;
  LOG(ERROR) << "Failed to associate models for "
             << syncer::ModelTypeToString(error.model_type());
  UMA_HISTOGRAM_ENUMERATION("Sync.ConfigureFailed",
                            ModelTypeToHistogramInt(error.model_type()),
                            syncer::MODEL_TYPE_COUNT);
}

void ModelAssociationManager::ModelLoadCallback(syncer::ModelType type,
                                                syncer::SyncError error) {
  DVLOG(1) << "ModelAssociationManager: ModelLoadCallback for "
      << syncer::ModelTypeToString(type);

  // TODO(haitaol): temporary fix for 335606.
  if (slow_types_.Has(type))
    return;

  // This happens when slow loading type is disabled by new configuration.
  if (!desired_types_.Has(type))
    return;

  DCHECK(!loaded_types_.Has(type));
  if (error.IsSet()) {
    syncer::SyncMergeResult local_merge_result(type);
    local_merge_result.set_error(error);
    TypeStartCallback(type,
                      base::TimeTicks::Now(),
                      DataTypeController::ASSOCIATION_FAILED,
                      local_merge_result,
                      syncer::SyncMergeResult(type));
    return;
  }

  loaded_types_.Put(type);
  if (associating_types_.Has(type) || slow_types_.Has(type)) {
    DataTypeController* dtc = controllers_->find(type)->second.get();
    dtc->StartAssociating(
        base::Bind(&ModelAssociationManager::TypeStartCallback,
                   weak_ptr_factory_.GetWeakPtr(),
                   type, base::TimeTicks::Now()));
  }
}

void ModelAssociationManager::TypeStartCallback(
    syncer::ModelType type,
    base::TimeTicks type_start_time,
    DataTypeController::StartResult start_result,
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result) {
  // TODO(haitaol): temporary fix for 335606.
  if (slow_types_.Has(type))
    return;

  // This happens when slow associating type is disabled by new configuration.
  if (!desired_types_.Has(type))
    return;

  slow_types_.Remove(type);

  DCHECK(!associated_types_.Has(type));
  if (DataTypeController::IsSuccessfulResult(start_result)) {
    associated_types_.Put(type);
  } else if (state_ == IDLE) {
    // For type that failed in IDLE mode, simply stop the controller. Next
    // configuration will try to restart from scratch if the type is still
    // enabled.
    DataTypeController* dtc = controllers_->find(type)->second.get();
    if (dtc->state() != DataTypeController::NOT_RUNNING)
      StopDatatype(dtc);
    loaded_types_.Remove(type);
  } else {
    // Record error in CONFIGURING or INITIALIZED_TO_CONFIGURE mode. The error
    // will be reported when data types association finishes.
    if (start_result == DataTypeController::NEEDS_CRYPTO) {
      DVLOG(1) << "ModelAssociationManager: Encountered an undecryptable type";
      needs_crypto_types_.Put(type);
    } else {
      DVLOG(1) << "ModelAssociationManager: Encountered a failed type";
      AppendToFailedDatatypesAndLogError(local_merge_result.error());
    }
  }

  if (state_ != CONFIGURING)
    return;

  TRACE_EVENT_ASYNC_END1("sync", "ModelAssociation",
                         controllers_->find(type)->second.get(),
                         "DataType",
                         ModelTypeToString(type));

  // Track the merge results if we succeeded or an association failure
  // occurred.
  if ((DataTypeController::IsSuccessfulResult(start_result) ||
       start_result == DataTypeController::ASSOCIATION_FAILED) &&
      syncer::ProtocolTypes().Has(type)) {
    base::TimeDelta association_wait_time =
        std::max(base::TimeDelta(), type_start_time - association_start_time_);
    base::TimeDelta association_time =
        base::TimeTicks::Now() - type_start_time;;
    syncer::DataTypeAssociationStats stats =
        BuildAssociationStatsFromMergeResults(local_merge_result,
                                              syncer_merge_result,
                                              association_wait_time,
                                              association_time);
    delegate_->OnSingleDataTypeAssociationDone(type, stats);
  }

  // Update configuration result.
  if (configure_status_ == DataTypeManager::OK &&
      start_result == DataTypeController::ASSOCIATION_FAILED) {
    configure_status_ = DataTypeManager::PARTIAL_SUCCESS;
  }
  if (start_result == DataTypeController::UNRECOVERABLE_ERROR)
    configure_status_ = DataTypeManager::UNRECOVERABLE_ERROR;

  associating_types_.Remove(type);

  if (associating_types_.Empty())
    ModelAssociationDone();
}

void ModelAssociationManager::ModelAssociationDone() {
  CHECK_EQ(CONFIGURING, state_);

  timer_.Stop();

  slow_types_.PutAll(associating_types_);

  // TODO(haitaol): temporary fix for 335606.
  for (syncer::ModelTypeSet::Iterator it = associating_types_.First();
      it.Good(); it.Inc()) {
    AppendToFailedDatatypesAndLogError(
        syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                          "Association timed out.", it.Get()));
  }

  // Stop controllers of failed types.
  StopDisabledTypes();

  if (configure_status_ == DataTypeManager::OK &&
      (!associating_types_.Empty() || !failed_data_types_info_.empty() ||
          !needs_crypto_types_.Empty())) {
    // We have not configured all types that we have been asked to configure.
    // Either we have failed types or types that have not completed loading
    // yet.
    DVLOG(1) << "ModelAssociationManager: setting partial success";
    configure_status_ = DataTypeManager::PARTIAL_SUCCESS;
  }

  DataTypeManager::ConfigureResult result(configure_status_,
                                          requested_types_,
                                          failed_data_types_info_,
                                          associating_types_,
                                          needs_crypto_types_);

  // Reset state before notifying |delegate_| because that might
  // trigger a new round of configuration.
  ResetForNextAssociation();
  state_ = IDLE;

  delegate_->OnModelAssociationDone(result);
}

base::OneShotTimer<ModelAssociationManager>*
    ModelAssociationManager::GetTimerForTesting() {
  return &timer_;
}

}  // namespace sync_driver
