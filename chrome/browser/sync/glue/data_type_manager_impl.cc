// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/data_type_manager_impl.h"

#include <algorithm>
#include <functional>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"

namespace browser_sync {

namespace {

static const syncable::ModelType kStartOrder[] = {
  syncable::NIGORI,  //  Listed for completeness.
  syncable::BOOKMARKS,
  syncable::PREFERENCES,
  syncable::AUTOFILL,
  syncable::AUTOFILL_PROFILE,
  syncable::EXTENSIONS,
  syncable::APPS,
  syncable::THEMES,
  syncable::TYPED_URLS,
  syncable::PASSWORDS,
  syncable::SESSIONS,
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
    const DataTypeController::TypeMap& controllers)
    : backend_(backend),
      controllers_(controllers),
      state_(DataTypeManager::STOPPED),
      needs_reconfigure_(false),
      method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(backend_);
  // Ensure all data type controllers are stopped.
  for (DataTypeController::TypeMap::const_iterator it = controllers_.begin();
       it != controllers_.end(); ++it) {
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
    DataTypeController::TypeMap::const_iterator dtc = controllers_.find(*it);
    if (dtc != controllers_.end() &&
        (dtc->second->state() == DataTypeController::NOT_RUNNING ||
         dtc->second->state() == DataTypeController::STOPPING)) {
      found_any = true;
      if (needs_start)
        needs_start->push_back(dtc->second.get());
    }
  }
  return found_any;
}

void DataTypeManagerImpl::Configure(const TypeSet& desired_types) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (state_ == STOPPING) {
    // You can not set a configuration while stopping.
    LOG(ERROR) << "Configuration set while stopping.";
    return;
  }

  last_requested_types_ = desired_types;
  // Only proceed if we're in a steady state or blocked.
  if (state_ != STOPPED && state_ != CONFIGURED && state_ != BLOCKED) {
    VLOG(1) << "Received configure request while configuration in flight. "
            << "Postponing until current configuration complete.";
    needs_reconfigure_ = true;
    return;
  }

  needs_start_.clear();
  GetControllersNeedingStart(&needs_start_);
  // Sort these according to kStartOrder.
  std::sort(needs_start_.begin(),
            needs_start_.end(),
            SortComparator(&start_order_));

  // Add any data type controllers into that needs_stop_ list that are
  // currently MODEL_STARTING, ASSOCIATING, or RUNNING.
  needs_stop_.clear();
  for (DataTypeController::TypeMap::const_iterator it = controllers_.begin();
       it != controllers_.end(); ++it) {
    DataTypeController* dtc = (*it).second;
    if (desired_types.count(dtc->type()) == 0 && (
            dtc->state() == DataTypeController::MODEL_STARTING ||
            dtc->state() == DataTypeController::ASSOCIATING ||
            dtc->state() == DataTypeController::RUNNING)) {
      needs_stop_.push_back(dtc);
      VLOG(1) << "Will stop " << dtc->name();
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
  Restart();
}

void DataTypeManagerImpl::Restart() {
  VLOG(1) << "Restarting...";

  DCHECK(state_ == STOPPED || state_ == CONFIGURED || state_ == BLOCKED);

  // Starting from a "steady state" (stopped or configured) state
  // should send a start notification.
  if (state_ == STOPPED || state_ == CONFIGURED)
    NotifyStart();

  // Stop requested data types.
  for (size_t i = 0; i < needs_stop_.size(); ++i) {
    VLOG(1) << "Stopping " << needs_stop_[i]->name();
    needs_stop_[i]->Stop();
  }
  needs_stop_.clear();

  // Tell the backend about the new set of data types we wish to sync.
  // The task will be invoked when updates are downloaded.
  state_ = DOWNLOAD_PENDING;
  backend_->ConfigureDataTypes(
      controllers_,
      last_requested_types_,
      method_factory_.NewRunnableMethod(&DataTypeManagerImpl::DownloadReady));
}

void DataTypeManagerImpl::DownloadReady() {
  DCHECK(state_ == DOWNLOAD_PENDING);

  state_ = CONFIGURING;
  StartNextType();
}

void DataTypeManagerImpl::StartNextType() {
  // If there are any data types left to start, start the one at the
  // front of the list.
  if (!needs_start_.empty()) {
    VLOG(1) << "Starting " << needs_start_[0]->name();
    needs_start_[0]->Start(
        NewCallback(this, &DataTypeManagerImpl::TypeStartCallback));
    return;
  }

  DCHECK_EQ(state_, CONFIGURING);

  if (needs_reconfigure_) {
    // An attempt was made to reconfigure while we were already configuring.
    // This can be because a passphrase was accepted or the user changed the
    // set of desired types. Either way, |last_requested_types_| will contain
    // the most recent set of desired types, so we just call configure.
    // Note: we do this whether or not GetControllersNeedingStart is true,
    // because we may need to stop datatypes.
    SetBlockedAndNotify();
    needs_reconfigure_ = false;
    VLOG(1) << "Reconfiguring due to previous configure attempt occuring while"
            << " busy.";

    // Unwind the stack before executing configure. The method configure and its
    // callees are not re-entrant.
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(&DataTypeManagerImpl::Configure,
            last_requested_types_));
    return;
  }

  // Do a fresh calculation to see if controllers need starting to account for
  // things like encryption, which may still need to be sorted out before we
  // can announce we're "Done" configuration entirely.
  if (GetControllersNeedingStart(NULL)) {
    SetBlockedAndNotify();
    return;
  }

  // If no more data types need starting, we're done.
  state_ = CONFIGURED;
  NotifyDone(OK, FROM_HERE);
}

void DataTypeManagerImpl::TypeStartCallback(
    DataTypeController::StartResult result,
    const tracked_objects::Location& location) {
  // When the data type controller invokes this callback, it must be
  // on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (state_ == STOPPING) {
    // If we reach this callback while stopping, this means that
    // DataTypeManager::Stop() was called while the current data type
    // was starting.  Now that it has finished starting, we can finish
    // stopping the DataTypeManager.  This is considered an ABORT.
    FinishStopAndNotify(ABORTED, FROM_HERE);
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

  if (result == DataTypeController::NEEDS_CRYPTO) {

  }
  // If the type started normally, continue to the next type.
  // If the type is waiting for the cryptographer, continue to the next type.
  // Once the cryptographer is ready, we'll attempt to restart this type.
  if (result == DataTypeController::NEEDS_CRYPTO ||
      result == DataTypeController::OK ||
      result == DataTypeController::OK_FIRST_RUN) {
    StartNextType();
    return;
  }

  // Any other result is a fatal error.  Shut down any types we've
  // managed to start up to this point and pass the result to the
  // callback.
  VLOG(1) << "Failed " << started_dtc->name();
  ConfigureResult configure_result = DataTypeManager::ABORTED;
  switch (result) {
    case DataTypeController::ABORTED:
      configure_result = DataTypeManager::ABORTED;
      break;
    case DataTypeController::ASSOCIATION_FAILED:
      configure_result = DataTypeManager::ASSOCIATION_FAILED;
      break;
    case DataTypeController::UNRECOVERABLE_ERROR:
      configure_result = DataTypeManager::UNRECOVERABLE_ERROR;
      break;
    default:
      NOTREACHED();
      break;
  }
  FinishStopAndNotify(configure_result, location);
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
    method_factory_.RevokeAll();
    FinishStopAndNotify(ABORTED, FROM_HERE);
    return;
  }

  FinishStop();
}

void DataTypeManagerImpl::FinishStop() {
  DCHECK(state_== CONFIGURING || state_ == STOPPING || state_ == BLOCKED);
  // Simply call the Stop() method on all running data types.
  for (DataTypeController::TypeMap::const_iterator it = controllers_.begin();
       it != controllers_.end(); ++it) {
    DataTypeController* dtc = (*it).second;
    if (dtc->state() != DataTypeController::NOT_RUNNING &&
        dtc->state() != DataTypeController::STOPPING) {
      dtc->Stop();
      VLOG(1) << "Stopped " << dtc->name();
    }
  }
  state_ = STOPPED;
}

void DataTypeManagerImpl::FinishStopAndNotify(ConfigureResult result,
    const tracked_objects::Location& location) {
  FinishStop();
  NotifyDone(result, location);
}

void DataTypeManagerImpl::NotifyStart() {
  NotificationService::current()->Notify(
      NotificationType::SYNC_CONFIGURE_START,
      Source<DataTypeManager>(this),
      NotificationService::NoDetails());
}

void DataTypeManagerImpl::NotifyDone(ConfigureResult result,
    const tracked_objects::Location& location) {
  ConfigureResultWithErrorLocation result_with_location(result, location,
                                                        last_requested_types_);
  NotificationService::current()->Notify(
      NotificationType::SYNC_CONFIGURE_DONE,
      Source<DataTypeManager>(this),
      Details<ConfigureResultWithErrorLocation>(&result_with_location));
}

const DataTypeController::TypeMap& DataTypeManagerImpl::controllers() {
  return controllers_;
}

DataTypeManager::State DataTypeManagerImpl::state() {
  return state_;
}

void DataTypeManagerImpl::SetBlockedAndNotify() {
  state_ = BLOCKED;
  NotificationService::current()->Notify(
      NotificationType::SYNC_CONFIGURE_BLOCKED,
      Source<DataTypeManager>(this),
      NotificationService::NoDetails());
}

}  // namespace browser_sync
