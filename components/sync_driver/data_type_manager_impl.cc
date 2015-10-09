// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/data_type_manager_impl.h"

#include <algorithm>
#include <functional>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "components/sync_driver/data_type_encryption_handler.h"
#include "components/sync_driver/data_type_manager_observer.h"
#include "components/sync_driver/data_type_status_table.h"
#include "sync/internal_api/public/data_type_debug_info_listener.h"

namespace sync_driver {

namespace {

DataTypeStatusTable::TypeErrorMap
GenerateCryptoErrorsForTypes(syncer::ModelTypeSet encrypted_types) {
  DataTypeStatusTable::TypeErrorMap crypto_errors;
  for (syncer::ModelTypeSet::Iterator iter = encrypted_types.First();
         iter.Good(); iter.Inc()) {
    crypto_errors[iter.Get()] = syncer::SyncError(
        FROM_HERE,
        syncer::SyncError::CRYPTO_ERROR,
        "",
        iter.Get());
  }
  return crypto_errors;
}

}  // namespace

DataTypeManagerImpl::AssociationTypesInfo::AssociationTypesInfo() {}
DataTypeManagerImpl::AssociationTypesInfo::~AssociationTypesInfo() {}

DataTypeManagerImpl::DataTypeManagerImpl(
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const DataTypeController::TypeMap* controllers,
    const DataTypeEncryptionHandler* encryption_handler,
    BackendDataTypeConfigurer* configurer,
    DataTypeManagerObserver* observer)
    : configurer_(configurer),
      controllers_(controllers),
      state_(DataTypeManager::STOPPED),
      needs_reconfigure_(false),
      last_configure_reason_(syncer::CONFIGURE_REASON_UNKNOWN),
      debug_info_listener_(debug_info_listener),
      model_association_manager_(controllers, this),
      observer_(observer),
      encryption_handler_(encryption_handler),
      catch_up_in_progress_(false),
      weak_ptr_factory_(this) {
  DCHECK(configurer_);
  DCHECK(observer_);
}

DataTypeManagerImpl::~DataTypeManagerImpl() {}

void DataTypeManagerImpl::Configure(syncer::ModelTypeSet desired_types,
                                    syncer::ConfigureReason reason) {
  // Once requested, we will remain in "catch up" mode until we notify the
  // caller (see NotifyDone). We do this to ensure that once started, subsequent
  // calls to Configure won't take us out of "catch up" mode.
  if (reason == syncer::CONFIGURE_REASON_CATCH_UP)
    catch_up_in_progress_ = true;

  if (reason == syncer::CONFIGURE_REASON_BACKUP_ROLLBACK)
    desired_types.PutAll(syncer::ControlTypes());
  else
    desired_types.PutAll(syncer::CoreTypes());

  // Only allow control types and types that have controllers.
  syncer::ModelTypeSet filtered_desired_types;
  for (syncer::ModelTypeSet::Iterator type = desired_types.First();
       type.Good(); type.Inc()) {
    DataTypeController::TypeMap::const_iterator iter =
        controllers_->find(type.Get());
    if (syncer::IsControlType(type.Get()) || iter != controllers_->end()) {
      if (iter != controllers_->end()) {
        if (!iter->second->ReadyForStart() &&
            !data_type_status_table_.GetUnreadyErrorTypes().Has(
                type.Get())) {
          // Add the type to the unready types set to prevent purging it. It's
          // up to the datatype controller to, if necessary, explicitly
          // mark the type as broken to trigger a purge.
          syncer::SyncError error(FROM_HERE,
                                  syncer::SyncError::UNREADY_ERROR,
                                  "Datatype not ready at config time.",
                                  type.Get());
          std::map<syncer::ModelType, syncer::SyncError> errors;
          errors[type.Get()] = error;
          data_type_status_table_.UpdateFailedDataTypes(errors);
        } else if (iter->second->ReadyForStart()) {
          data_type_status_table_.ResetUnreadyErrorFor(type.Get());
        }
      }
      filtered_desired_types.Put(type.Get());
    }
  }
  ConfigureImpl(filtered_desired_types, reason);
}

void DataTypeManagerImpl::ReenableType(syncer::ModelType type) {
  // TODO(zea): move the "should we reconfigure" logic into the datatype handler
  // itself.
  // Only reconfigure if the type actually had a data type or unready error.
  if (!data_type_status_table_.ResetDataTypeErrorFor(type) &&
      !data_type_status_table_.ResetUnreadyErrorFor(type)) {
    return;
  }

  // Only reconfigure if the type is actually desired.
  if (!last_requested_types_.Has(type))
    return;

  DVLOG(1) << "Reenabling " << syncer::ModelTypeToString(type);
  needs_reconfigure_ = true;
  last_configure_reason_ = syncer::CONFIGURE_REASON_PROGRAMMATIC;
  ProcessReconfigure();
}

void DataTypeManagerImpl::ResetDataTypeErrors() {
  data_type_status_table_.Reset();
}

void DataTypeManagerImpl::PurgeForMigration(
    syncer::ModelTypeSet undesired_types,
    syncer::ConfigureReason reason) {
  syncer::ModelTypeSet remainder = Difference(last_requested_types_,
                                              undesired_types);
  ConfigureImpl(remainder, reason);
}

void DataTypeManagerImpl::ConfigureImpl(
    syncer::ModelTypeSet desired_types,
    syncer::ConfigureReason reason) {
  DCHECK_NE(reason, syncer::CONFIGURE_REASON_UNKNOWN);
  DVLOG(1) << "Configuring for " << syncer::ModelTypeSetToString(desired_types)
           << " with reason " << reason;
  if (state_ == STOPPING) {
    // You can not set a configuration while stopping.
    LOG(ERROR) << "Configuration set while stopping.";
    return;
  }

  // TODO(zea): consider not performing a full configuration once there's a
  // reliable way to determine if the requested set of enabled types matches the
  // current set.

  last_requested_types_ = desired_types;
  last_configure_reason_ = reason;
  // Only proceed if we're in a steady state or retrying.
  if (state_ != STOPPED && state_ != CONFIGURED && state_ != RETRYING) {
    DVLOG(1) << "Received configure request while configuration in flight. "
             << "Postponing until current configuration complete.";
    needs_reconfigure_ = true;
    return;
  }

  Restart(reason);
}

BackendDataTypeConfigurer::DataTypeConfigStateMap
DataTypeManagerImpl::BuildDataTypeConfigStateMap(
    const syncer::ModelTypeSet& types_being_configured) const {
  // 1. Get the failed types (due to fatal, crypto, and unready errors).
  // 2. Add the difference between last_requested_types_ and the failed types
  //    as CONFIGURE_INACTIVE.
  // 3. Flip |types_being_configured| to CONFIGURE_ACTIVE.
  // 4. Set non-enabled user types as DISABLED.
  // 5. Set the fatal, crypto, and unready types to their respective states.
  syncer::ModelTypeSet error_types =
      data_type_status_table_.GetFailedTypes();
  syncer::ModelTypeSet fatal_types =
      data_type_status_table_.GetFatalErrorTypes();
  syncer::ModelTypeSet crypto_types =
      data_type_status_table_.GetCryptoErrorTypes();
  syncer::ModelTypeSet unready_types =
      data_type_status_table_.GetUnreadyErrorTypes();

  // Types with persistence errors are only purged/resynced when they're
  // actively being configured.
  syncer::ModelTypeSet clean_types =
      data_type_status_table_.GetPersistenceErrorTypes();
  clean_types.RetainAll(types_being_configured);

  // Types with unready errors do not count as unready if they've been disabled.
  unready_types.RetainAll(last_requested_types_);

  syncer::ModelTypeSet enabled_types = last_requested_types_;
  enabled_types.RemoveAll(error_types);

  // If we're catching up, add all enabled (non-error) types to the clean set to
  // ensure we download and apply them to the model types.
  if (catch_up_in_progress_)
    clean_types.PutAll(enabled_types);

  syncer::ModelTypeSet disabled_types =
      syncer::Difference(
          syncer::Union(syncer::UserTypes(), syncer::ControlTypes()),
          enabled_types);
  syncer::ModelTypeSet to_configure = syncer::Intersection(
      enabled_types, types_being_configured);
  DVLOG(1) << "Enabling: " << syncer::ModelTypeSetToString(enabled_types);
  DVLOG(1) << "Configuring: " << syncer::ModelTypeSetToString(to_configure);
  DVLOG(1) << "Disabling: " << syncer::ModelTypeSetToString(disabled_types);

  BackendDataTypeConfigurer::DataTypeConfigStateMap config_state_map;
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::CONFIGURE_INACTIVE, enabled_types,
      &config_state_map);
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE, to_configure,
      &config_state_map);
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::CONFIGURE_CLEAN, clean_types,
        &config_state_map);
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::DISABLED, disabled_types,
      &config_state_map);
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::FATAL, fatal_types,
      &config_state_map);
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::CRYPTO, crypto_types,
        &config_state_map);
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::UNREADY, unready_types,
        &config_state_map);
  return config_state_map;
}

void DataTypeManagerImpl::Restart(syncer::ConfigureReason reason) {
  DVLOG(1) << "Restarting...";

  // Only record the type histograms for user-triggered configurations or
  // restarts.
  if (reason == syncer::CONFIGURE_REASON_RECONFIGURATION ||
      reason == syncer::CONFIGURE_REASON_NEW_CLIENT ||
      reason == syncer::CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE) {
    for (syncer::ModelTypeSet::Iterator iter = last_requested_types_.First();
         iter.Good(); iter.Inc()) {
      UMA_HISTOGRAM_ENUMERATION("Sync.ConfigureDataTypes",
                                syncer::ModelTypeToHistogramInt(iter.Get()),
                                syncer::MODEL_TYPE_COUNT);
    }
  }

  // Check for new or resolved data type crypto errors.
  if (encryption_handler_->IsPassphraseRequired()) {
    syncer::ModelTypeSet encrypted_types =
        encryption_handler_->GetEncryptedDataTypes();
    encrypted_types.RetainAll(last_requested_types_);
    encrypted_types.RemoveAll(
        data_type_status_table_.GetCryptoErrorTypes());
    DataTypeStatusTable::TypeErrorMap crypto_errors =
        GenerateCryptoErrorsForTypes(encrypted_types);
    data_type_status_table_.UpdateFailedDataTypes(crypto_errors);
  } else {
    data_type_status_table_.ResetCryptoErrors();
  }

  syncer::ModelTypeSet failed_types =
      data_type_status_table_.GetFailedTypes();
  syncer::ModelTypeSet enabled_types =
      syncer::Difference(last_requested_types_, failed_types);

  last_restart_time_ = base::Time::Now();
  configuration_stats_.clear();

  DCHECK(state_ == STOPPED || state_ == CONFIGURED || state_ == RETRYING);

  const State old_state = state_;
  state_ = CONFIGURING;

  // Starting from a "steady state" (stopped or configured) state
  // should send a start notification.
  // Note: NotifyStart() must be called with the updated (non-idle) state,
  // otherwise logic listening for the configuration start might not be aware
  // of the fact that the DTM is in a configuration state.
  if (old_state == STOPPED || old_state == CONFIGURED)
    NotifyStart();

  download_types_queue_ = PrioritizeTypes(enabled_types);
  association_types_queue_ = std::queue<AssociationTypesInfo>();

  // If we're performing a "catch up", first stop the model types to ensure the
  // call to Initialize triggers model association.
  if (catch_up_in_progress_)
    model_association_manager_.Stop();
  model_association_manager_.Initialize(enabled_types);

  StartNextDownload(syncer::ModelTypeSet());
}

syncer::ModelTypeSet DataTypeManagerImpl::GetPriorityTypes() const {
  syncer::ModelTypeSet high_priority_types;
  high_priority_types.PutAll(syncer::PriorityCoreTypes());
  high_priority_types.PutAll(syncer::PriorityUserTypes());
  return high_priority_types;
}

TypeSetPriorityList DataTypeManagerImpl::PrioritizeTypes(
    const syncer::ModelTypeSet& types) {
  syncer::ModelTypeSet high_priority_types = GetPriorityTypes();
  high_priority_types.RetainAll(types);

  syncer::ModelTypeSet low_priority_types =
      syncer::Difference(types, high_priority_types);

  TypeSetPriorityList result;
  if (!high_priority_types.Empty())
    result.push(high_priority_types);
  if (!low_priority_types.Empty())
    result.push(low_priority_types);

  // Could be empty in case of purging for migration, sync nothing, etc.
  // Configure empty set to purge data from backend.
  if (result.empty())
    result.push(syncer::ModelTypeSet());

  return result;
}

void DataTypeManagerImpl::ProcessReconfigure() {
  DCHECK(needs_reconfigure_);

  // Wait for current download and association to finish.
  if (!download_types_queue_.empty() ||
      model_association_manager_.state() ==
          ModelAssociationManager::ASSOCIATING) {
    return;
  }

  association_types_queue_ = std::queue<AssociationTypesInfo>();

  // An attempt was made to reconfigure while we were already configuring.
  // This can be because a passphrase was accepted or the user changed the
  // set of desired types. Either way, |last_requested_types_| will contain
  // the most recent set of desired types, so we just call configure.
  // Note: we do this whether or not GetControllersNeedingStart is true,
  // because we may need to stop datatypes.
  DVLOG(1) << "Reconfiguring due to previous configure attempt occuring while"
           << " busy.";

  // Note: ConfigureImpl is called directly, rather than posted, in order to
  // ensure that any purging/unapplying/journaling happens while the set of
  // failed types is still up to date. If stack unwinding were to be done
  // via PostTask, the failed data types may be reset before the purging was
  // performed.
  state_ = RETRYING;
  needs_reconfigure_ = false;
  ConfigureImpl(last_requested_types_, last_configure_reason_);
}

void DataTypeManagerImpl::OnDownloadRetry() {
  DCHECK_EQ(CONFIGURING, state_);
}

void DataTypeManagerImpl::DownloadReady(
    syncer::ModelTypeSet types_to_download,
    syncer::ModelTypeSet first_sync_types,
    syncer::ModelTypeSet failed_configuration_types) {
  DCHECK_EQ(CONFIGURING, state_);

  // Persistence errors are reset after each backend configuration attempt
  // during which they would have been purged.
  data_type_status_table_.ResetPersistenceErrorsFrom(types_to_download);

  if (!failed_configuration_types.Empty()) {
    DataTypeStatusTable::TypeErrorMap errors;
    for (syncer::ModelTypeSet::Iterator iter =
             failed_configuration_types.First(); iter.Good(); iter.Inc()) {
      syncer::SyncError error(
          FROM_HERE,
          syncer::SyncError::DATATYPE_ERROR,
          "Backend failed to download type.",
          iter.Get());
      errors[iter.Get()] = error;
    }
    data_type_status_table_.UpdateFailedDataTypes(errors);
    needs_reconfigure_ = true;
  }

  if (needs_reconfigure_) {
    download_types_queue_ = TypeSetPriorityList();
    ProcessReconfigure();
    return;
  }

  CHECK(!download_types_queue_.empty());
  download_types_queue_.pop();

  // Those types that were already downloaded (non first sync/error types)
  // should already be associating. Just kick off association of the newly
  // downloaded types if necessary.
  if (!association_types_queue_.empty()) {
    association_types_queue_.back().first_sync_types = first_sync_types;
    association_types_queue_.back().download_ready_time = base::Time::Now();
    StartNextAssociation(UNREADY_AT_CONFIG);
  } else if (download_types_queue_.empty() &&
             model_association_manager_.state() !=
                 ModelAssociationManager::ASSOCIATING) {
    // There's nothing more to download or associate (implying either there were
    // no types to associate or they associated as part of |ready_types|).
    // If the model association manager is also finished, then we're done
    // configuring.
    state_ = CONFIGURED;
    ConfigureResult result(OK, last_requested_types_);
    NotifyDone(result);
    return;
  }

  StartNextDownload(types_to_download);
}

void DataTypeManagerImpl::StartNextDownload(
    syncer::ModelTypeSet high_priority_types_before) {
  if (download_types_queue_.empty())
    return;

  // Tell the backend about the new set of data types we wish to sync.
  // The task will be invoked when updates are downloaded.
  syncer::ModelTypeSet ready_types = configurer_->ConfigureDataTypes(
      last_configure_reason_,
      BuildDataTypeConfigStateMap(download_types_queue_.front()),
      base::Bind(&DataTypeManagerImpl::DownloadReady,
                 weak_ptr_factory_.GetWeakPtr(),
                 download_types_queue_.front()),
      base::Bind(&DataTypeManagerImpl::OnDownloadRetry,
                 weak_ptr_factory_.GetWeakPtr()));

  AssociationTypesInfo association_info;
  association_info.types = download_types_queue_.front();
  association_info.ready_types = ready_types;
  association_info.download_start_time = base::Time::Now();
  association_info.high_priority_types_before = high_priority_types_before;
  association_types_queue_.push(association_info);

  // Start associating those types that are already downloaded (does nothing
  // if model associator is busy).
  StartNextAssociation(READY_AT_CONFIG);
}

void DataTypeManagerImpl::StartNextAssociation(AssociationGroup group) {
  CHECK(!association_types_queue_.empty());

  // If the model association manager is already associating, let it finish.
  // The model association done event will result in associating any remaining
  // association groups.
  if (model_association_manager_.state() !=
          ModelAssociationManager::INITIALIZED) {
    return;
  }

  syncer::ModelTypeSet types_to_associate;
  if (group == READY_AT_CONFIG) {
    association_types_queue_.front().ready_association_request_time =
        base::Time::Now();
    types_to_associate = association_types_queue_.front().ready_types;
  } else {
    DCHECK_EQ(UNREADY_AT_CONFIG, group);
    // Only start associating the rest of the types if they have all finished
    // downloading.
    if (association_types_queue_.front().download_ready_time.is_null())
      return;
    association_types_queue_.front().full_association_request_time =
        base::Time::Now();
    // We request the full set of types here for completeness sake. All types
    // within the READY_AT_CONFIG set will already be started and should be
    // no-ops.
    types_to_associate = association_types_queue_.front().types;
  }


  DVLOG(1) << "Associating "
           << syncer::ModelTypeSetToString(types_to_associate);
  model_association_manager_.StartAssociationAsync(types_to_associate);
}

void DataTypeManagerImpl::OnSingleDataTypeWillStop(
    syncer::ModelType type,
    const syncer::SyncError& error) {
  DataTypeController::TypeMap::const_iterator c_it = controllers_->find(type);
  DCHECK(c_it != controllers_->end());
  // Delegate deactivation to the controller.
  c_it->second->DeactivateDataType(configurer_);

  if (error.IsSet()) {
    DataTypeStatusTable::TypeErrorMap failed_types;
    failed_types[type] = error;
    data_type_status_table_.UpdateFailedDataTypes(
            failed_types);

    // Unrecoverable errors will shut down the entire backend, so no need to
    // reconfigure.
    if (error.error_type() != syncer::SyncError::UNRECOVERABLE_ERROR) {
      needs_reconfigure_ = true;
      last_configure_reason_ = syncer::CONFIGURE_REASON_PROGRAMMATIC;
      ProcessReconfigure();
    }
  }
}

void DataTypeManagerImpl::OnSingleDataTypeAssociationDone(
    syncer::ModelType type,
    const syncer::DataTypeAssociationStats& association_stats) {
  DCHECK(!association_types_queue_.empty());
  DataTypeController::TypeMap::const_iterator c_it = controllers_->find(type);
  DCHECK(c_it != controllers_->end());
  if (c_it->second->state() == DataTypeController::RUNNING) {
    // Delegate activation to the controller.
    c_it->second->ActivateDataType(configurer_);
  }

  if (!debug_info_listener_.IsInitialized())
    return;

  AssociationTypesInfo& info = association_types_queue_.front();
  configuration_stats_.push_back(syncer::DataTypeConfigurationStats());
  configuration_stats_.back().model_type = type;
  configuration_stats_.back().association_stats = association_stats;
  if (info.types.Has(type)) {
    // Times in |info| only apply to non-slow types.
    configuration_stats_.back().download_wait_time =
        info.download_start_time - last_restart_time_;
    if (info.first_sync_types.Has(type)) {
      configuration_stats_.back().download_time =
          info.download_ready_time - info.download_start_time;
    }
    if (info.ready_types.Has(type)) {
      configuration_stats_.back().association_wait_time_for_high_priority =
          info.ready_association_request_time - info.download_start_time;
    } else {
      configuration_stats_.back().association_wait_time_for_high_priority =
          info.full_association_request_time - info.download_ready_time;
    }
    configuration_stats_.back().high_priority_types_configured_before =
        info.high_priority_types_before;
    configuration_stats_.back().same_priority_types_configured_before =
        info.configured_types;
    info.configured_types.Put(type);
  }
}

void DataTypeManagerImpl::OnModelAssociationDone(
    const DataTypeManager::ConfigureResult& result) {
  DCHECK(state_ == STOPPING || state_ == CONFIGURING);

  if (state_ == STOPPING)
    return;

  // Ignore abort/unrecoverable error if we need to reconfigure anyways.
  if (needs_reconfigure_) {
    ProcessReconfigure();
    return;
  }

  if (result.status == ABORTED || result.status == UNRECOVERABLE_ERROR) {
    Abort(result.status);
    return;
  }

  DCHECK(result.status == OK);

  CHECK(!association_types_queue_.empty());

  // If this model association was for the full set of types, then this priority
  // set is done. Otherwise it was just the ready types and the unready types
  // still need to be associated.
  if (result.requested_types.Equals(association_types_queue_.front().types)) {
    association_types_queue_.pop();
    if (!association_types_queue_.empty()) {
      StartNextAssociation(READY_AT_CONFIG);
    } else if (download_types_queue_.empty()) {
      state_ = CONFIGURED;
      NotifyDone(result);
    }
  } else {
    DCHECK(result.requested_types.Equals(
        association_types_queue_.front().ready_types));
    // Will do nothing if the types are still downloading.
    StartNextAssociation(UNREADY_AT_CONFIG);
  }
}

void DataTypeManagerImpl::Stop() {
  if (state_ == STOPPED)
    return;

  bool need_to_notify = state_ == CONFIGURING;
  StopImpl();

  if (need_to_notify) {
    ConfigureResult result(ABORTED,
                           last_requested_types_);
    NotifyDone(result);
  }
}

void DataTypeManagerImpl::Abort(ConfigureStatus status) {
  DCHECK_EQ(CONFIGURING, state_);

  StopImpl();

  DCHECK_NE(OK, status);
  ConfigureResult result(status,
                         last_requested_types_);
  NotifyDone(result);
}

void DataTypeManagerImpl::StopImpl() {
  state_ = STOPPING;

  // Invalidate weak pointer to drop download callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Stop all data types. This may trigger association callback but the
  // callback will do nothing because state is set to STOPPING above.
  model_association_manager_.Stop();

  state_ = STOPPED;
}

void DataTypeManagerImpl::NotifyStart() {
  observer_->OnConfigureStart();
}

void DataTypeManagerImpl::NotifyDone(const ConfigureResult& raw_result) {
  catch_up_in_progress_ = false;

  AddToConfigureTime();

  ConfigureResult result = raw_result;
  result.data_type_status_table = data_type_status_table_;

  DVLOG(1) << "Total time spent configuring: "
           << configure_time_delta_.InSecondsF() << "s";
  switch (result.status) {
    case DataTypeManager::OK:
      DVLOG(1) << "NotifyDone called with result: OK";
      UMA_HISTOGRAM_LONG_TIMES("Sync.ConfigureTime_Long.OK",
                               configure_time_delta_);
      if (debug_info_listener_.IsInitialized() &&
          !configuration_stats_.empty()) {
        debug_info_listener_.Call(
            FROM_HERE,
            &syncer::DataTypeDebugInfoListener::OnDataTypeConfigureComplete,
            configuration_stats_);
      }
      configuration_stats_.clear();
      break;
    case DataTypeManager::ABORTED:
      DVLOG(1) << "NotifyDone called with result: ABORTED";
      UMA_HISTOGRAM_LONG_TIMES("Sync.ConfigureTime_Long.ABORTED",
                               configure_time_delta_);
      break;
    case DataTypeManager::UNRECOVERABLE_ERROR:
      DVLOG(1) << "NotifyDone called with result: UNRECOVERABLE_ERROR";
      UMA_HISTOGRAM_LONG_TIMES("Sync.ConfigureTime_Long.UNRECOVERABLE_ERROR",
                               configure_time_delta_);
      break;
    case DataTypeManager::UNKNOWN:
      NOTREACHED();
      break;
  }
  observer_->OnConfigureDone(result);
}

DataTypeManager::State DataTypeManagerImpl::state() const {
  return state_;
}

void DataTypeManagerImpl::AddToConfigureTime() {
  DCHECK(!last_restart_time_.is_null());
  configure_time_delta_ += (base::Time::Now() - last_restart_time_);
}

}  // namespace sync_driver
