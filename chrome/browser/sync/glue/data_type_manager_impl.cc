// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/data_type_manager_impl.h"

#include <algorithm>
#include <functional>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "chrome/browser/sync/failed_datatypes_handler.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager_observer.h"
#include "content/public/browser/browser_thread.h"
#include "sync/internal_api/public/data_type_debug_info_listener.h"

using content::BrowserThread;

namespace browser_sync {

DataTypeManagerImpl::AssociationTypesInfo::AssociationTypesInfo() {}
DataTypeManagerImpl::AssociationTypesInfo::~AssociationTypesInfo() {}

DataTypeManagerImpl::DataTypeManagerImpl(
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    BackendDataTypeConfigurer* configurer,
    const DataTypeController::TypeMap* controllers,
    DataTypeManagerObserver* observer,
    const FailedDatatypesHandler* failed_datatypes_handler)
    : configurer_(configurer),
      controllers_(controllers),
      state_(DataTypeManager::STOPPED),
      needs_reconfigure_(false),
      last_configure_reason_(syncer::CONFIGURE_REASON_UNKNOWN),
      weak_ptr_factory_(this),
      debug_info_listener_(debug_info_listener),
      model_association_manager_(controllers, this),
      observer_(observer),
      failed_datatypes_handler_(failed_datatypes_handler) {
  DCHECK(configurer_);
  DCHECK(observer_);
}

DataTypeManagerImpl::~DataTypeManagerImpl() {}

void DataTypeManagerImpl::Configure(TypeSet desired_types,
                                    syncer::ConfigureReason reason) {
  desired_types.PutAll(syncer::ControlTypes());
  ConfigureImpl(desired_types, reason);
}

void DataTypeManagerImpl::PurgeForMigration(
    TypeSet undesired_types,
    syncer::ConfigureReason reason) {
  TypeSet remainder = Difference(last_requested_types_, undesired_types);
  ConfigureImpl(remainder, reason);
}

void DataTypeManagerImpl::ConfigureImpl(
    TypeSet desired_types,
    syncer::ConfigureReason reason) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_NE(reason, syncer::CONFIGURE_REASON_UNKNOWN);
  if (state_ == STOPPING) {
    // You can not set a configuration while stopping.
    LOG(ERROR) << "Configuration set while stopping.";
    return;
  }

  if (state_ == CONFIGURED &&
      last_requested_types_.Equals(desired_types) &&
      reason == syncer::CONFIGURE_REASON_RECONFIGURATION) {
    // If we're already configured and the types haven't changed, we can exit
    // out early.
    NotifyStart();
    ConfigureResult result(OK, last_requested_types_);
    NotifyDone(result);
    return;
  }

  last_requested_types_ = desired_types;
  last_configure_reason_ = reason;
  // Only proceed if we're in a steady state or blocked.
  if (state_ != STOPPED && state_ != CONFIGURED && state_ != BLOCKED) {
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
  // In four steps:
  // 1. Add last_requested_types_ as CONFIGURE_INACTIVE.
  // 2. Flip |types_being_configured| to CONFIGURE_ACTIVE.
  // 3. Set other types as DISABLED.
  // 4. Overwrite state of failed types according to failed_datatypes_handler_.
  BackendDataTypeConfigurer::DataTypeConfigStateMap config_state_map;
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::CONFIGURE_INACTIVE, last_requested_types_,
      &config_state_map);
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE,
      types_being_configured, &config_state_map);
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::DISABLED,
      syncer::Difference(syncer::UserTypes(), last_requested_types_),
      &config_state_map);
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::DISABLED,
      syncer::Difference(syncer::ControlTypes(), last_requested_types_),
      &config_state_map);
  if (failed_datatypes_handler_) {
    BackendDataTypeConfigurer::SetDataTypesState(
        BackendDataTypeConfigurer::FAILED,
        failed_datatypes_handler_->GetFailedTypes(),
        &config_state_map);
  }
  return config_state_map;
}

void DataTypeManagerImpl::Restart(syncer::ConfigureReason reason) {
  DVLOG(1) << "Restarting...";
  model_association_manager_.Initialize(last_requested_types_);
  last_restart_time_ = base::Time::Now();
  configure_result_ = ConfigureResult();
  configure_result_.status = OK;

  DCHECK(state_ == STOPPED || state_ == CONFIGURED || state_ == BLOCKED);

  // Starting from a "steady state" (stopped or configured) state
  // should send a start notification.
  if (state_ == STOPPED || state_ == CONFIGURED)
    NotifyStart();

  model_association_manager_.StopDisabledTypes();

  download_types_queue_ = PrioritizeTypes(last_requested_types_);
  association_types_queue_ = std::queue<AssociationTypesInfo>();

  // Tell the backend about the new set of data types we wish to sync.
  // The task will be invoked when updates are downloaded.
  state_ = DOWNLOAD_PENDING;
  configurer_->ConfigureDataTypes(
      reason,
      BuildDataTypeConfigStateMap(download_types_queue_.front()),
      base::Bind(&DataTypeManagerImpl::DownloadReady,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Time::Now(), syncer::ModelTypeSet()),
      base::Bind(&DataTypeManagerImpl::OnDownloadRetry,
                 weak_ptr_factory_.GetWeakPtr()));
}

TypeSetPriorityList DataTypeManagerImpl::PrioritizeTypes(
    const syncer::ModelTypeSet& types) {
  syncer::ModelTypeSet high_priority_types;
  high_priority_types.PutAll(syncer::ControlTypes());
  high_priority_types.PutAll(syncer::PriorityUserTypes());
  high_priority_types.RetainAll(types);

  syncer::ModelTypeSet low_priority_types =
      syncer::Difference(types, high_priority_types);

  TypeSetPriorityList result;
  if (!high_priority_types.Empty())
    result.push(high_priority_types);
  if (!low_priority_types.Empty())
    result.push(low_priority_types);
  return result;
}

void DataTypeManagerImpl::ProcessReconfigure() {
  DCHECK(needs_reconfigure_);

  // Wait for current download and association to finish.
  if (!(download_types_queue_.empty() && association_types_queue_.empty()))
    return;

  model_association_manager_.ResetForReconfiguration();

  // An attempt was made to reconfigure while we were already configuring.
  // This can be because a passphrase was accepted or the user changed the
  // set of desired types. Either way, |last_requested_types_| will contain
  // the most recent set of desired types, so we just call configure.
  // Note: we do this whether or not GetControllersNeedingStart is true,
  // because we may need to stop datatypes.
  SetBlockedAndNotify();
  DVLOG(1) << "Reconfiguring due to previous configure attempt occuring while"
           << " busy.";

  // Unwind the stack before executing configure. The method configure and its
  // callees are not re-entrant.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DataTypeManagerImpl::ConfigureImpl,
                 weak_ptr_factory_.GetWeakPtr(),
                 last_requested_types_,
                 last_configure_reason_));

  needs_reconfigure_ = false;
}

void DataTypeManagerImpl::OnDownloadRetry() {
  DCHECK(state_ == DOWNLOAD_PENDING || state_ == CONFIGURING);
  observer_->OnConfigureRetry();
}

void DataTypeManagerImpl::DownloadReady(
    base::Time download_start_time,
    syncer::ModelTypeSet high_priority_types_before,
    syncer::ModelTypeSet first_sync_types,
    syncer::ModelTypeSet failed_configuration_types) {
  DCHECK(state_ == DOWNLOAD_PENDING || state_ == CONFIGURING);

  // Ignore |failed_configuration_types| if we need to reconfigure
  // anyway.
  if (needs_reconfigure_) {
    download_types_queue_ = TypeSetPriorityList();
    ProcessReconfigure();
    return;
  }

  if (!failed_configuration_types.Empty()) {
    ChromeReportUnrecoverableError();
    std::string error_msg =
        "Configuration failed for types " +
        syncer::ModelTypeSetToString(failed_configuration_types);
    syncer::SyncError error(FROM_HERE, error_msg,
                            failed_configuration_types.First().Get());
    Abort(UNRECOVERABLE_ERROR, error);
    return;
  }

  state_ = CONFIGURING;

  // Pop and associate download-ready types.
  syncer::ModelTypeSet ready_types = download_types_queue_.front();
  download_types_queue_.pop();

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
  if (!download_types_queue_.empty()) {
    configurer_->ConfigureDataTypes(
        last_configure_reason_,
        BuildDataTypeConfigStateMap(download_types_queue_.front()),
        base::Bind(&DataTypeManagerImpl::DownloadReady,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Time::Now(),
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

void DataTypeManagerImpl::OnSingleDataTypeAssociationDone(
    syncer::ModelType type,
    const syncer::DataTypeAssociationStats& association_stats) {
  DCHECK(!association_types_queue_.empty());

  if (!debug_info_listener_.IsInitialized())
    return;

  syncer::DataTypeConfigurationStats configuration_stats;
  configuration_stats.model_type = type;
  configuration_stats.association_stats = association_stats;

  AssociationTypesInfo& info = association_types_queue_.front();
  configuration_stats.download_wait_time =
      info.download_start_time - last_restart_time_;
  if (info.first_sync_types.Has(type)) {
    configuration_stats.download_time =
        info.download_ready_time - info.download_start_time;
  }
  configuration_stats.association_wait_time_for_high_priority =
      info.association_request_time - info.download_ready_time;
  configuration_stats.high_priority_types_configured_before =
      info.high_priority_types_before;
  configuration_stats.same_priority_types_configured_before =
      info.configured_types;

  info.configured_types.Put(type);

  debug_info_listener_.Call(
      FROM_HERE,
      &syncer::DataTypeDebugInfoListener::OnSingleDataTypeConfigureComplete,
      configuration_stats);
}

void DataTypeManagerImpl::OnModelAssociationDone(
    const DataTypeManager::ConfigureResult& result) {
  DCHECK(state_ == STOPPING || state_ == CONFIGURING);

  if (state_ == STOPPING)
    return;

  if (needs_reconfigure_) {
    association_types_queue_ = std::queue<AssociationTypesInfo>();
    ProcessReconfigure();
    return;
  }

  if (result.status == ABORTED || result.status == UNRECOVERABLE_ERROR) {
    Abort(result.status, result.failed_data_types.size() >= 1 ?
                         result.failed_data_types.front() :
                         syncer::SyncError());
    return;
  }

  if (result.status == CONFIGURE_BLOCKED) {
    SetBlockedAndNotify();
    return;
  }

  DCHECK(result.status == PARTIAL_SUCCESS || result.status == OK);

  if (result.status == PARTIAL_SUCCESS)
    configure_result_.status = PARTIAL_SUCCESS;
  configure_result_.requested_types.PutAll(result.requested_types);
  configure_result_.failed_data_types.insert(
      configure_result_.failed_data_types.end(),
      result.failed_data_types.begin(),
      result.failed_data_types.end());
  configure_result_.waiting_to_start.PutAll(result.waiting_to_start);

  association_types_queue_.pop();
  if (!association_types_queue_.empty()) {
    StartNextAssociation();
  } else if (download_types_queue_.empty()) {
    state_ = CONFIGURED;
    NotifyDone(configure_result_);
  }
}

void DataTypeManagerImpl::OnTypesLoaded() {
  if (state_ != CONFIGURED) {
    // Ignore this. either we just started another configuration or
    // we are in some sort of error.
    return;
  }

  Restart(syncer::CONFIGURE_REASON_RECONFIGURATION);
}


void DataTypeManagerImpl::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (state_ == STOPPED)
    return;

  bool need_to_notify = state_ == DOWNLOAD_PENDING || state_ == CONFIGURING;
  StopImpl();

  if (need_to_notify) {
    ConfigureResult result(ABORTED,
                           last_requested_types_,
                           std::list<syncer::SyncError>(),
                           syncer::ModelTypeSet());
    NotifyDone(result);
  }
}

void DataTypeManagerImpl::Abort(ConfigureStatus status,
                                const syncer::SyncError& error) {
  DCHECK(state_ == DOWNLOAD_PENDING || state_ == CONFIGURING);

  StopImpl();

  DCHECK_NE(OK, status);
  std::list<syncer::SyncError> error_list;
  if (error.IsSet())
    error_list.push_back(error);
  ConfigureResult result(status,
                         last_requested_types_,
                         error_list,
                         syncer::ModelTypeSet());
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
    case DataTypeManager::PARTIAL_SUCCESS:
      DVLOG(1) << "NotifyDone called with result: PARTIAL_SUCCESS";
      UMA_HISTOGRAM_LONG_TIMES("Sync.ConfigureTime_Long.PARTIAL_SUCCESS",
                               configure_time_delta_);
      break;
    default:
      NOTREACHED();
      break;
  }
  observer_->OnConfigureDone(result);
}

DataTypeManager::State DataTypeManagerImpl::state() const {
  return state_;
}

void DataTypeManagerImpl::SetBlockedAndNotify() {
  // Drop download callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  state_ = BLOCKED;
  AddToConfigureTime();
  DVLOG(1) << "Accumulated spent configuring: "
           << configure_time_delta_.InSecondsF() << "s";
  observer_->OnConfigureBlocked();
}

void DataTypeManagerImpl::AddToConfigureTime() {
  DCHECK(!last_restart_time_.is_null());
  configure_time_delta_ += (base::Time::Now() - last_restart_time_);
}

}  // namespace browser_sync
