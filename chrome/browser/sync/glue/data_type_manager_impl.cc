// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/data_type_manager_impl.h"

#include <algorithm>
#include <functional>

#include "base/bind.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stringprintf.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace browser_sync {

namespace {

static const syncable::ModelType kStartOrder[] = {
  syncable::NIGORI,  //  Listed for completeness.
  syncable::BOOKMARKS,
  syncable::PREFERENCES,
  syncable::AUTOFILL,
  syncable::AUTOFILL_PROFILE,
  syncable::EXTENSION_SETTINGS,
  syncable::EXTENSIONS,
  syncable::APP_SETTINGS,
  syncable::APPS,
  syncable::THEMES,
  syncable::TYPED_URLS,
  syncable::PASSWORDS,
  syncable::SEARCH_ENGINES,
  syncable::SESSIONS,
  syncable::APP_NOTIFICATIONS,
};

COMPILE_ASSERT(arraysize(kStartOrder) ==
               syncable::MODEL_TYPE_COUNT - syncable::FIRST_REAL_MODEL_TYPE,
               kStartOrder_IncorrectSize);

// Comparator used when sorting data type controllers.
class SortComparator : public std::binary_function<DataTypeController*,
                                                   DataTypeController*,
                                                   bool> {
 public:
  explicit SortComparator(std::map<syncable::ModelType, int>* order)
      : order_(order) { }

  // Returns true if lhs precedes rhs.
  bool operator() (DataTypeController* lhs, DataTypeController* rhs) {
    return (*order_)[lhs->type()] < (*order_)[rhs->type()];
  }

 private:
  std::map<syncable::ModelType, int>* order_;
};

}  // namespace

DataTypeManagerImpl::DataTypeManagerImpl(SyncBackendHost* backend,
    const DataTypeController::TypeMap* controllers)
    : backend_(backend),
      controllers_(controllers),
      state_(DataTypeManager::STOPPED),
      needs_reconfigure_(false),
      last_configure_reason_(sync_api::CONFIGURE_REASON_UNKNOWN),
      last_enable_nigori_(false),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(backend_);
  // Ensure all data type controllers are stopped.
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DCHECK_EQ(DataTypeController::NOT_RUNNING, (*it).second->state());
  }

  // Build a ModelType -> order map for sorting.
  for (int i = 0; i < static_cast<int>(arraysize(kStartOrder)); i++)
    start_order_[kStartOrder[i]] = i;
}

DataTypeManagerImpl::~DataTypeManagerImpl() {}

bool DataTypeManagerImpl::GetControllersNeedingStart(
    std::vector<DataTypeController*>* needs_start) {
  // Add any data type controllers into the needs_start_ list that are
  // currently NOT_RUNNING or STOPPING.
  bool found_any = false;
  for (TypeSet::const_iterator it = last_requested_types_.begin();
       it != last_requested_types_.end(); ++it) {
    DataTypeController::TypeMap::const_iterator dtc = controllers_->find(*it);
    if (dtc != controllers_->end() &&
        (dtc->second->state() == DataTypeController::NOT_RUNNING ||
         dtc->second->state() == DataTypeController::STOPPING)) {
      found_any = true;
      if (needs_start)
        needs_start->push_back(dtc->second.get());
      if (dtc->second->state() == DataTypeController::DISABLED) {
        DVLOG(1) << "Found " << syncable::ModelTypeToString(dtc->second->type())
                 << " in disabled state.";
      }
    }
  }
  return found_any;
}

void DataTypeManagerImpl::Configure(const TypeSet& desired_types,
                                    sync_api::ConfigureReason reason) {
  ConfigureImpl(desired_types, reason, true);
}

void DataTypeManagerImpl::ConfigureWithoutNigori(const TypeSet& desired_types,
    sync_api::ConfigureReason reason) {
  ConfigureImpl(desired_types, reason, false);
}

void DataTypeManagerImpl::ConfigureImpl(const TypeSet& desired_types,
                                        sync_api::ConfigureReason reason,
                                        bool enable_nigori) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (state_ == STOPPING) {
    // You can not set a configuration while stopping.
    LOG(ERROR) << "Configuration set while stopping.";
    return;
  }

  if (state_ == CONFIGURED &&
      last_requested_types_ == desired_types &&
      reason == sync_api::CONFIGURE_REASON_RECONFIGURATION) {
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
    last_enable_nigori_ = enable_nigori;
    return;
  }

  needs_start_.clear();
  GetControllersNeedingStart(&needs_start_);
  // Sort these according to kStartOrder.
  std::sort(needs_start_.begin(),
            needs_start_.end(),
            SortComparator(&start_order_));

  // Add any data type controllers into that needs_stop_ list that are
  // currently MODEL_STARTING, ASSOCIATING, RUNNING or DISABLED.
  needs_stop_.clear();
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DataTypeController* dtc = (*it).second;
    if (desired_types.count(dtc->type()) == 0 && (
            dtc->state() == DataTypeController::MODEL_STARTING ||
            dtc->state() == DataTypeController::ASSOCIATING ||
            dtc->state() == DataTypeController::RUNNING ||
            dtc->state() == DataTypeController::DISABLED)) {
      needs_stop_.push_back(dtc);
      DVLOG(1) << "Will stop " << dtc->name();
    }
  }
  // Sort these according to kStartOrder.
  std::sort(needs_stop_.begin(),
            needs_stop_.end(),
            SortComparator(&start_order_));

  // Restart to start/stop data types and notify the backend that the
  // desired types have changed (need to do this even if there aren't any
  // types to start/stop, because it could be that some types haven't
  // started due to crypto errors but the backend host needs to know that we're
  // disabling them anyway).
  Restart(reason, enable_nigori);
}

void DataTypeManagerImpl::Restart(sync_api::ConfigureReason reason,
                                  bool enable_nigori) {
  DVLOG(1) << "Restarting...";
  last_restart_time_ = base::Time::Now();

  DCHECK(state_ == STOPPED || state_ == CONFIGURED || state_ == BLOCKED);

  // Starting from a "steady state" (stopped or configured) state
  // should send a start notification.
  if (state_ == STOPPED || state_ == CONFIGURED)
    NotifyStart();

  // Stop requested data types.
  for (size_t i = 0; i < needs_stop_.size(); ++i) {
    DVLOG(1) << "Stopping " << needs_stop_[i]->name();
    needs_stop_[i]->Stop();
  }
  needs_stop_.clear();

  // Tell the backend about the new set of data types we wish to sync.
  // The task will be invoked when updates are downloaded.
  state_ = DOWNLOAD_PENDING;
  // Hopefully http://crbug.com/79970 will make this less verbose.
  syncable::ModelTypeSet all_types;
  const syncable::ModelTypeSet& types_to_add = last_requested_types_;
  syncable::ModelTypeSet types_to_remove;
  for (DataTypeController::TypeMap::const_iterator it =
           controllers_->begin(); it != controllers_->end(); ++it) {
    all_types.insert(it->first);
  }
  // Check that types_to_add \subseteq all_types.
  DCHECK(std::includes(all_types.begin(), all_types.end(),
                       types_to_add.begin(), types_to_add.end()));
  // Set types_to_remove to all_types \setminus types_to_add.
  ignore_result(
      std::set_difference(
          all_types.begin(), all_types.end(),
          types_to_add.begin(), types_to_add.end(),
          std::inserter(types_to_remove, types_to_remove.end())));
  backend_->ConfigureDataTypes(
      types_to_add,
      types_to_remove,
      reason,
      base::Bind(&DataTypeManagerImpl::DownloadReady,
                 weak_ptr_factory_.GetWeakPtr()),
      enable_nigori);
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
                 last_enable_nigori_));

  needs_reconfigure_ = false;
  last_configure_reason_ = sync_api::CONFIGURE_REASON_UNKNOWN;
  last_enable_nigori_ = false;
  return true;
}

void DataTypeManagerImpl::DownloadReady(
    const syncable::ModelTypeSet& failed_configuration_types) {
  DCHECK_EQ(state_, DOWNLOAD_PENDING);

  // Ignore |failed_configuration_types| if we need to reconfigure
  // anyway.
  if (ProcessReconfigure()) {
    return;
  }

  if (!failed_configuration_types.empty()) {
    std::string error_msg =
        "Configuration failed for types " +
        syncable::ModelTypeSetToString(failed_configuration_types);
    SyncError error(FROM_HERE, error_msg,
                    *failed_configuration_types.begin());
    Abort(UNRECOVERABLE_ERROR, error);
    return;
  }

  state_ = CONFIGURING;
  StartNextType();
}

void DataTypeManagerImpl::StartNextType() {
  // If there are any data types left to start, start the one at the
  // front of the list.
  if (!needs_start_.empty()) {
    DVLOG(1) << "Starting " << needs_start_[0]->name();
    TRACE_EVENT_BEGIN1("sync", "ModelAssociation",
                       "DataType", ModelTypeToString(needs_start_[0]->type()));
    needs_start_[0]->Start(
        NewCallback(this, &DataTypeManagerImpl::TypeStartCallback));
    return;
  }

  DCHECK_EQ(state_, CONFIGURING);
  if (ProcessReconfigure()) {
    return;
  }

  // Do a fresh calculation to see if controllers need starting to account for
  // things like encryption, which may still need to be sorted out before we
  // can announce we're "Done" configuration entirely.
  if (GetControllersNeedingStart(NULL)) {
    DVLOG(1) << "GetControllersNeedingStart returned true. DTM blocked";
    SetBlockedAndNotify();
    return;
  }

  // If no more data types need starting, we're done.
  state_ = CONFIGURED;
  ConfigureStatus status = OK;
  if (!failed_datatypes_info_.empty()) {
    status = PARTIAL_SUCCESS;
  }
  ConfigureResult result(status,
                         last_requested_types_,
                         failed_datatypes_info_);
  NotifyDone(result);
  failed_datatypes_info_.clear();
}

void DataTypeManagerImpl::TypeStartCallback(
    DataTypeController::StartResult result,
    const SyncError& error) {
  // When the data type controller invokes this callback, it must be
  // on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TRACE_EVENT_END0("sync", "ModelAssociation");

  if (state_ == STOPPING) {
    // If we reach this callback while stopping, this means that
    // DataTypeManager::Stop() was called while the current data type
    // was starting.  Now that it has finished starting, we can finish
    // stopping the DataTypeManager.  This is considered an ABORT.
    Abort(ABORTED, SyncError());
    return;
  } else if (state_ == STOPPED) {
    // If our state_ is STOPPED, we have already stopped all of the data
    // types.  We should not be getting callbacks from stopped data types.
    LOG(ERROR) << "Start callback called by stopped data type!";
    return;
  }

  // We're done with the data type at the head of the list -- remove it.
  DataTypeController* started_dtc = needs_start_[0];
  DCHECK(needs_start_.size());
  DCHECK_EQ(needs_start_[0], started_dtc);
  needs_start_.erase(needs_start_.begin());

  if (result == DataTypeController::ASSOCIATION_FAILED) {
    failed_datatypes_info_.push_back(error);
    LOG(ERROR) << "Failed to associate models for "
            << syncable::ModelTypeToString(error.type());
    UMA_HISTOGRAM_ENUMERATION("Sync.ConfigureFailed",
                              error.type(),
                              syncable::MODEL_TYPE_COUNT);
  }

  // If the type started normally, continue to the next type.
  // If the type is waiting for the cryptographer, continue to the next type.
  // Once the cryptographer is ready, we'll attempt to restart this type.
  // If this type encountered a type specific error continue to the next type.
  if (result == DataTypeController::NEEDS_CRYPTO ||
      result == DataTypeController::OK ||
      result == DataTypeController::OK_FIRST_RUN ||
      result == DataTypeController::ASSOCIATION_FAILED) {
    StartNextType();
    return;
  }

  // Any other result requires reconfiguration. Pass it on through the callback.
  LOG(ERROR) << "Failed to configure " << started_dtc->name();
  DCHECK(error.IsSet());
  DCHECK_EQ(started_dtc->type(), error.type());
  ConfigureStatus configure_status = DataTypeManager::ABORTED;
  switch (result) {
    case DataTypeController::ABORTED:
      configure_status = DataTypeManager::ABORTED;
      break;
    case DataTypeController::UNRECOVERABLE_ERROR:
      configure_status = DataTypeManager::UNRECOVERABLE_ERROR;
      break;
    default:
      NOTREACHED();
      break;
  }

  Abort(configure_status, error);
}

void DataTypeManagerImpl::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (state_ == STOPPED)
    return;

  // If we are currently configuring, then the current type is in a
  // partially started state.  Abort the startup of the current type,
  // which will synchronously invoke the start callback.
  if (state_ == CONFIGURING) {
    state_ = STOPPING;

    DCHECK_LT(0U, needs_start_.size());
    needs_start_[0]->Stop();

    // By this point, the datatype should have invoked the start callback,
    // triggering FinishStop to be called, and the state to reach STOPPED. If we
    // aren't STOPPED, it means that a datatype controller didn't call the start
    // callback appropriately.
    DCHECK_EQ(STOPPED, state_);
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
  // Simply call the Stop() method on all running data types.
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DataTypeController* dtc = (*it).second;
    if (dtc->state() != DataTypeController::NOT_RUNNING &&
        dtc->state() != DataTypeController::STOPPING) {
      dtc->Stop();
      DVLOG(1) << "Stopped " << dtc->name();
    }
  }

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
                         error_list);
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
      UMA_HISTOGRAM_TIMES("Sync.ConfigureTime.OK",
                          configure_time_delta_);
      break;
    case DataTypeManager::ABORTED:
      DVLOG(1) << "NotifyDone called with result: ABORTED";
      UMA_HISTOGRAM_TIMES("Sync.ConfigureTime.ABORTED",
                          configure_time_delta_);
      break;
    case DataTypeManager::UNRECOVERABLE_ERROR:
      DVLOG(1) << "NotifyDone called with result: UNRECOVERABLE_ERROR";
      UMA_HISTOGRAM_TIMES("Sync.ConfigureTime.UNRECOVERABLE_ERROR",
                          configure_time_delta_);
      break;
    case DataTypeManager::PARTIAL_SUCCESS:
      DVLOG(1) << "NotifyDone called with result: PARTIAL_SUCCESS";
      UMA_HISTOGRAM_TIMES("Sync.ConfigureTime.PARTIAL_SUCCESS",
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

DataTypeManager::State DataTypeManagerImpl::state() {
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
