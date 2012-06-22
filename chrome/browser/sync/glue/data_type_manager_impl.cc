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
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace browser_sync {

DataTypeManagerImpl::DataTypeManagerImpl(
    BackendDataTypeConfigurer* configurer,
    const DataTypeController::TypeMap* controllers)
    : configurer_(configurer),
      controllers_(controllers),
      state_(DataTypeManager::STOPPED),
      needs_reconfigure_(false),
      last_configure_reason_(csync::CONFIGURE_REASON_UNKNOWN),
      last_nigori_state_(BackendDataTypeConfigurer::WITHOUT_NIGORI),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      model_association_manager_(controllers,
                                 ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(configurer_);
}

DataTypeManagerImpl::~DataTypeManagerImpl() {}

void DataTypeManagerImpl::Configure(TypeSet desired_types,
                                    csync::ConfigureReason reason) {
  ConfigureImpl(desired_types, reason,
                BackendDataTypeConfigurer::WITH_NIGORI);
}

void DataTypeManagerImpl::ConfigureWithoutNigori(TypeSet desired_types,
    csync::ConfigureReason reason) {
  ConfigureImpl(desired_types, reason,
                BackendDataTypeConfigurer::WITHOUT_NIGORI);
}

void DataTypeManagerImpl::ConfigureImpl(
    TypeSet desired_types,
    csync::ConfigureReason reason,
    BackendDataTypeConfigurer::NigoriState nigori_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (state_ == STOPPING) {
    // You can not set a configuration while stopping.
    LOG(ERROR) << "Configuration set while stopping.";
    return;
  }

  if (state_ == CONFIGURED &&
      last_requested_types_.Equals(desired_types) &&
      reason == csync::CONFIGURE_REASON_RECONFIGURATION) {
    // If we're already configured and the types haven't changed, we can exit
    // out early.
    NotifyStart();
    ConfigureResult result(OK, last_requested_types_);
    NotifyDone(result);
    return;
  }

  last_requested_types_ = desired_types;
  last_nigori_state_ = nigori_state;
  // Only proceed if we're in a steady state or blocked.
  if (state_ != STOPPED && state_ != CONFIGURED && state_ != BLOCKED) {
    DVLOG(1) << "Received configure request while configuration in flight. "
             << "Postponing until current configuration complete.";
    needs_reconfigure_ = true;
    last_configure_reason_ = reason;
    return;
  }

  Restart(reason, nigori_state);
}

void DataTypeManagerImpl::Restart(
    csync::ConfigureReason reason,
    BackendDataTypeConfigurer::NigoriState nigori_state) {
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
  // Hopefully http://crbug.com/79970 will make this less verbose.
  syncable::ModelTypeSet all_types;
  for (DataTypeController::TypeMap::const_iterator it =
           controllers_->begin(); it != controllers_->end(); ++it) {
    all_types.Put(it->first);
  }
  const syncable::ModelTypeSet types_to_add = last_requested_types_;
  // Check that types_to_add \subseteq all_types.
  DCHECK(all_types.HasAll(types_to_add));
  // Set types_to_remove to all_types \setminus types_to_add.
  const syncable::ModelTypeSet types_to_remove =
      Difference(all_types, types_to_add);
  configurer_->ConfigureDataTypes(
      reason,
      types_to_add,
      types_to_remove,
      nigori_state,
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
                 last_configure_reason_,
                 last_nigori_state_));

  needs_reconfigure_ = false;
  last_configure_reason_ = csync::CONFIGURE_REASON_UNKNOWN;
  last_nigori_state_ = BackendDataTypeConfigurer::WITHOUT_NIGORI;
  return true;
}

void DataTypeManagerImpl::OnDownloadRetry() {
  DCHECK_EQ(state_, DOWNLOAD_PENDING);

  // Inform the listeners we are waiting.
  ConfigureResult result;
  result.status = DataTypeManager::RETRY;

  // TODO(lipalani): Add a new  NOTIFICATION_SYNC_CONFIGURE_RETRY.
  // crbug.com/111676.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
      content::Source<DataTypeManager>(this),
      content::Details<const ConfigureResult>(&result));
}

void DataTypeManagerImpl::DownloadReady(
    syncable::ModelTypeSet failed_configuration_types) {
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
        syncable::ModelTypeSetToString(failed_configuration_types);
    SyncError error(FROM_HERE, error_msg,
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
                         SyncError());
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

  Restart(csync::CONFIGURE_REASON_RECONFIGURATION,
          last_nigori_state_);
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
    Abort(ABORTED, SyncError());
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
                                const SyncError& error) {
  DCHECK_NE(OK, status);
  FinishStop();
  std::list<SyncError> error_list;
  if (error.IsSet())
    error_list.push_back(error);
  ConfigureResult result(status,
                         last_requested_types_,
                         error_list,
                         syncable::ModelTypeSet());
  NotifyDone(result);
}

void DataTypeManagerImpl::NotifyStart() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SYNC_CONFIGURE_START,
      content::Source<DataTypeManager>(this),
      content::NotificationService::NoDetails());
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
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
      content::Source<DataTypeManager>(this),
      content::Details<const ConfigureResult>(&result));
}

DataTypeManager::State DataTypeManagerImpl::state() const {
  return state_;
}

void DataTypeManagerImpl::SetBlockedAndNotify() {
  state_ = BLOCKED;
  AddToConfigureTime();
  DVLOG(1) << "Accumulated spent configuring: "
           << configure_time_delta_.InSecondsF() << "s";
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SYNC_CONFIGURE_BLOCKED,
      content::Source<DataTypeManager>(this),
      content::NotificationService::NoDetails());
}

void DataTypeManagerImpl::AddToConfigureTime() {
  DCHECK(!last_restart_time_.is_null());
  configure_time_delta_ += (base::Time::Now() - last_restart_time_);
}

}  // namespace browser_sync
