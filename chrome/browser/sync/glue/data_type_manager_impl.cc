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

using content::BrowserThread;

namespace browser_sync {

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
      model_association_manager_(debug_info_listener,
                                 controllers,
                                 this),
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
  // Only proceed if we're in a steady state or blocked.
  if (state_ != STOPPED && state_ != CONFIGURED && state_ != BLOCKED) {
    DVLOG(1) << "Received configure request while configuration in flight. "
             << "Postponing until current configuration complete.";
    needs_reconfigure_ = true;
    last_configure_reason_ = reason;
    return;
  }

  Restart(reason);
}

BackendDataTypeConfigurer::DataTypeConfigStateMap
DataTypeManagerImpl::BuildDataTypeConfigStateMap() const {
  // In three steps:
  // 1. Add last_requested_types_ as ENABLED.
  // 2. Set other types as DISABLED.
  // 3. Overwrite state of failed types according to failed_datatypes_handler_.
  BackendDataTypeConfigurer::DataTypeConfigStateMap config_state_map;
  BackendDataTypeConfigurer::SetDataTypesState(
      BackendDataTypeConfigurer::ENABLED, last_requested_types_,
      &config_state_map);
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

  DCHECK(state_ == STOPPED || state_ == CONFIGURED || state_ == BLOCKED);

  // Starting from a "steady state" (stopped or configured) state
  // should send a start notification.
  if (state_ == STOPPED || state_ == CONFIGURED)
    NotifyStart();

  model_association_manager_.StopDisabledTypes();

  // Tell the backend about the new set of data types we wish to sync.
  // The task will be invoked when updates are downloaded.
  state_ = DOWNLOAD_PENDING;
  configurer_->ConfigureDataTypes(
      reason,
      BuildDataTypeConfigStateMap(),
      base::Bind(&DataTypeManagerImpl::DownloadReady,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DataTypeManagerImpl::OnDownloadRetry,
                 weak_ptr_factory_.GetWeakPtr()));
}

bool DataTypeManagerImpl::ProcessReconfigure() {
  if (!needs_reconfigure_) {
    return false;
  }
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
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DataTypeManagerImpl::ConfigureImpl,
                 weak_ptr_factory_.GetWeakPtr(),
                 last_requested_types_,
                 last_configure_reason_));

  needs_reconfigure_ = false;
  last_configure_reason_ = syncer::CONFIGURE_REASON_UNKNOWN;
  return true;
}

void DataTypeManagerImpl::OnDownloadRetry() {
  DCHECK_EQ(state_, DOWNLOAD_PENDING);
  observer_->OnConfigureRetry();
}

void DataTypeManagerImpl::DownloadReady(
    syncer::ModelTypeSet failed_configuration_types) {
  DCHECK_EQ(state_, DOWNLOAD_PENDING);

  // Ignore |failed_configuration_types| if we need to reconfigure
  // anyway.
  if (needs_reconfigure_) {
    model_association_manager_.ResetForReconfiguration();
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
  model_association_manager_.StartAssociationAsync();
}

void DataTypeManagerImpl::OnModelAssociationDone(
    const DataTypeManager::ConfigureResult& result) {
  if (result.status == ABORTED || result.status == UNRECOVERABLE_ERROR) {
    Abort(result.status, result.failed_data_types.size() >= 1 ?
                         result.failed_data_types.front() :
                         syncer::SyncError());
    return;
  }

  if (ProcessReconfigure()) {
    return;
  }

  if (result.status == CONFIGURE_BLOCKED) {
    failed_datatypes_info_.insert(failed_datatypes_info_.end(),
                                  result.failed_data_types.begin(),
                                  result.failed_data_types.end());
    SetBlockedAndNotify();
    return;
  }

  DCHECK(result.status == PARTIAL_SUCCESS || result.status == OK);
  state_ = CONFIGURED;
  failed_datatypes_info_.insert(failed_datatypes_info_.end(),
                                result.failed_data_types.begin(),
                                result.failed_data_types.end());
  ConfigureResult configure_result(result.status,
                                   result.requested_types,
                                   failed_datatypes_info_,
                                   result.waiting_to_start);
  NotifyDone(configure_result);
  failed_datatypes_info_.clear();
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

  // If we are currently configuring, then the current type is in a
  // partially started state.  Abort the startup of the current type,
  // which will synchronously invoke the start callback.
  if (state_ == CONFIGURING) {
    model_association_manager_.Stop();
    state_ = STOPPED;

    return;
  }

  const bool download_pending = state_ == DOWNLOAD_PENDING;
  state_ = STOPPING;
  if (download_pending) {
    // If Stop() is called while waiting for download, cancel all
    // outstanding tasks.
    weak_ptr_factory_.InvalidateWeakPtrs();
    Abort(ABORTED, syncer::SyncError());
    return;
  }

  FinishStop();
}

void DataTypeManagerImpl::FinishStop() {
  DCHECK(state_== CONFIGURING || state_ == STOPPING || state_ == BLOCKED ||
         state_ == DOWNLOAD_PENDING);
  model_association_manager_.Stop();

  failed_datatypes_info_.clear();
  state_ = STOPPED;
}

void DataTypeManagerImpl::Abort(ConfigureStatus status,
                                const syncer::SyncError& error) {
  DCHECK_NE(OK, status);
  FinishStop();
  std::list<syncer::SyncError> error_list;
  if (error.IsSet())
    error_list.push_back(error);
  ConfigureResult result(status,
                         last_requested_types_,
                         error_list,
                         syncer::ModelTypeSet());
  NotifyDone(result);
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
