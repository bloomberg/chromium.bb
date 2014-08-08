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
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "components/sync_driver/data_type_controller.h"
#include "components/sync_driver/data_type_encryption_handler.h"
#include "components/sync_driver/data_type_manager_observer.h"
#include "components/sync_driver/failed_data_types_handler.h"
#include "sync/internal_api/public/data_type_debug_info_listener.h"

namespace sync_driver {

namespace {

FailedDataTypesHandler::TypeErrorMap
GenerateCryptoErrorsForTypes(syncer::ModelTypeSet encrypted_types) {
  FailedDataTypesHandler::TypeErrorMap crypto_errors;
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
    const base::Closure& unrecoverable_error_method,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const DataTypeController::TypeMap* controllers,
    const DataTypeEncryptionHandler* encryption_handler,
    BackendDataTypeConfigurer* configurer,
    DataTypeManagerObserver* observer,
    FailedDataTypesHandler* failed_data_types_handler)
    : configurer_(configurer),
      controllers_(controllers),
      state_(DataTypeManager::STOPPED),
      needs_reconfigure_(false),
      last_configure_reason_(syncer::CONFIGURE_REASON_UNKNOWN),
      debug_info_listener_(debug_info_listener),
      model_association_manager_(controllers, this),
      observer_(observer),
      failed_data_types_handler_(failed_data_types_handler),
      encryption_handler_(encryption_handler),
      unrecoverable_error_method_(unrecoverable_error_method),
      weak_ptr_factory_(this) {
  DCHECK(failed_data_types_handler_);
  DCHECK(configurer_);
  DCHECK(observer_);
}

DataTypeManagerImpl::~DataTypeManagerImpl() {}

void DataTypeManagerImpl::Configure(syncer::ModelTypeSet desired_types,
                                    syncer::ConfigureReason reason) {
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
    if (syncer::IsControlType(type.Get()) ||
        iter != controllers_->end()) {
      if (iter != controllers_->end()) {
        if (!iter->second->ReadyForStart() &&
            !failed_data_types_handler_->GetUnreadyErrorTypes().Has(
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
          failed_data_types_handler_->UpdateFailedDataTypes(errors);
        } else if (iter->second->ReadyForStart()) {
          failed_data_types_handler_->ResetUnreadyErrorFor(type.Get());
        }
      }
      filtered_desired_types.Put(type.Get());
    }
  }
  ConfigureImpl(filtered_desired_types, reason);
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
      failed_data_types_handler_->GetFailedTypes();
  syncer::ModelTypeSet fatal_types =
      failed_data_types_handler_->GetFatalErrorTypes();
  syncer::ModelTypeSet crypto_types =
      failed_data_types_handler_->GetCryptoErrorTypes();
  syncer::ModelTypeSet unready_types=
      failed_data_types_handler_->GetUnreadyErrorTypes();

  // Types with persistence errors are only purged/resynced when they're
  // actively being configured.
  syncer::ModelTypeSet persistence_types =
      failed_data_types_handler_->GetPersistenceErrorTypes();
  persistence_types.RetainAll(types_being_configured);

  // Types with unready errors do not count as unready if they've been disabled.
  unready_types.RetainAll(last_requested_types_);

  syncer::ModelTypeSet enabled_types = last_requested_types_;
  enabled_types.RemoveAll(error_types);
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
      BackendDataTypeConfigurer::CONFIGURE_CLEAN, persistence_types,
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

  // Check for new or resolved data type crypto errors.
  if (encryption_handler_->IsPassphraseRequired()) {
    syncer::ModelTypeSet encrypted_types =
        encryption_handler_->GetEncryptedDataTypes();
    encrypted_types.RetainAll(last_requested_types_);
    encrypted_types.RemoveAll(
        failed_data_types_handler_->GetCryptoErrorTypes());
    FailedDataTypesHandler::TypeErrorMap crypto_errors =
        GenerateCryptoErrorsForTypes(encrypted_types);
    failed_data_types_handler_->UpdateFailedDataTypes(crypto_errors);
  } else {
    failed_data_types_handler_->ResetCryptoErrors();
  }

  syncer::ModelTypeSet failed_types =
      failed_data_types_handler_->GetFailedTypes();
  syncer::ModelTypeSet enabled_types =
      syncer::Difference(last_requested_types_, failed_types);

  last_restart_time_ = base::Time::Now();
  configuration_stats_.clear();

  DCHECK(state_ == STOPPED || state_ == CONFIGURED || state_ == RETRYING);

  // Starting from a "steady state" (stopped or configured) state
  // should send a start notification.
  if (state_ == STOPPED || state_ == CONFIGURED)
    NotifyStart();

  model_association_manager_.Initialize(enabled_types);

  download_types_queue_ = PrioritizeTypes(enabled_types);
  association_types_queue_ = std::queue<AssociationTypesInfo>();

  // Tell the backend about the new set of data types we wish to sync.
  // The task will be invoked when updates are downloaded.
  state_ = DOWNLOAD_PENDING;
  configurer_->ConfigureDataTypes(
      reason,
      BuildDataTypeConfigStateMap(download_types_queue_.front()),
      base::Bind(&DataTypeManagerImpl::DownloadReady,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Time::Now(),
                 download_types_queue_.front(),
                 syncer::ModelTypeSet()),
      base::Bind(&DataTypeManagerImpl::OnDownloadRetry,
                 weak_ptr_factory_.GetWeakPtr()));
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
  if (!(download_types_queue_.empty() && association_types_queue_.empty()))
    return;

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
  DCHECK(state_ == DOWNLOAD_PENDING || state_ == CONFIGURING);
  observer_->OnConfigureRetry();
}

void DataTypeManagerImpl::DownloadReady(
    base::Time download_start_time,
    syncer::ModelTypeSet types_to_download,
    syncer::ModelTypeSet high_priority_types_before,
    syncer::ModelTypeSet first_sync_types,
    syncer::ModelTypeSet failed_configuration_types) {
  DCHECK(state_ == DOWNLOAD_PENDING || state_ == CONFIGURING);

  // Persistence errors are reset after each backend configuration attempt
  // during which they would have been purged.
  failed_data_types_handler_->ResetPersistenceErrorsFrom(types_to_download);

  // Ignore |failed_configuration_types| if we need to reconfigure
  // anyway.
  if (needs_reconfigure_) {
    download_types_queue_ = TypeSetPriorityList();
    ProcessReconfigure();
    return;
  }

  if (!failed_configuration_types.Empty()) {
    if (!unrecoverable_error_method_.is_null())
      unrecoverable_error_method_.Run();
    FailedDataTypesHandler::TypeErrorMap errors;
    for (syncer::ModelTypeSet::Iterator iter =
             failed_configuration_types.First(); iter.Good(); iter.Inc()) {
      syncer::SyncError error(
          FROM_HERE,
          syncer::SyncError::UNRECOVERABLE_ERROR,
          "Backend failed to download type.",
          iter.Get());
      errors[iter.Get()] = error;
    }
    failed_data_types_handler_->UpdateFailedDataTypes(errors);
    Abort(UNRECOVERABLE_ERROR);
    return;
  }

  state_ = CONFIGURING;

  // Pop and associate download-ready types.
  syncer::ModelTypeSet ready_types = types_to_download;
  download_types_queue_.pop();
  syncer::ModelTypeSet new_types_to_download;
  if (!download_types_queue_.empty())
    new_types_to_download = download_types_queue_.front();

  AssociationTypesInfo association_info;
  association_info.types = ready_types;
  association_info.first_sync_types = first_sync_types;
  association_info.download_start_time = download_start_time;
  association_info.download_ready_time = base::Time::Now();
  association_info.high_priority_types_before = high_priority_types_before;
  association_types_queue_.push(association_info);
  if (association_types_queue_.size() == 1u)
    StartNextAssociation();

  // Download types of low priority while configuring types of high priority.
  if (!new_types_to_download.Empty()) {
    configurer_->ConfigureDataTypes(
        last_configure_reason_,
        BuildDataTypeConfigStateMap(new_types_to_download),
        base::Bind(&DataTypeManagerImpl::DownloadReady,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Time::Now(),
                   new_types_to_download,
                   syncer::Union(ready_types, high_priority_types_before)),
        base::Bind(&DataTypeManagerImpl::OnDownloadRetry,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void DataTypeManagerImpl::StartNextAssociation() {
  CHECK(!association_types_queue_.empty());

  association_types_queue_.front().association_request_time =
      base::Time::Now();
  model_association_manager_.StartAssociationAsync(
      association_types_queue_.front().types);
}

void DataTypeManagerImpl::OnSingleDataTypeWillStop(
    syncer::ModelType type,
    const syncer::SyncError& error) {
  configurer_->DeactivateDataType(type);
  if (error.IsSet()) {
    FailedDataTypesHandler::TypeErrorMap failed_types;
    failed_types[type] = error;
    failed_data_types_handler_->UpdateFailedDataTypes(
            failed_types);

    // Unrecoverable errors will shut down the entire backend, so no need to
    // reconfigure.
    if (error.error_type() != syncer::SyncError::UNRECOVERABLE_ERROR) {
      needs_reconfigure_ = true;
      ProcessReconfigure();
    } else {
      DCHECK_EQ(state_, CONFIGURING);
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
    // Tell the backend about the change processor for this type so it can
    // begin routing changes to it.
    configurer_->ActivateDataType(type, c_it->second->model_safe_group(),
                                  c_it->second->GetChangeProcessor());
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
    configuration_stats_.back().association_wait_time_for_high_priority =
        info.association_request_time - info.download_ready_time;
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
    association_types_queue_ = std::queue<AssociationTypesInfo>();
    ProcessReconfigure();
    return;
  }

  if (result.status == ABORTED || result.status == UNRECOVERABLE_ERROR) {
    Abort(result.status);
    return;
  }

  DCHECK(result.status == OK);

  association_types_queue_.pop();
  if (!association_types_queue_.empty()) {
    StartNextAssociation();
  } else if (download_types_queue_.empty()) {
    state_ = CONFIGURED;
    NotifyDone(result);
  }
}

void DataTypeManagerImpl::Stop() {
  if (state_ == STOPPED)
    return;

  bool need_to_notify =
      state_ == DOWNLOAD_PENDING || state_ == CONFIGURING;
  StopImpl();

  if (need_to_notify) {
    ConfigureResult result(ABORTED,
                           last_requested_types_);
    NotifyDone(result);
  }
}

void DataTypeManagerImpl::Abort(ConfigureStatus status) {
  DCHECK(state_ == DOWNLOAD_PENDING || state_ == CONFIGURING);

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

void DataTypeManagerImpl::NotifyDone(const ConfigureResult& result) {
  AddToConfigureTime();

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
