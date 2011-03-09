// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/data_type_manager_impl2.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
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

DataTypeManagerImpl2::DataTypeManagerImpl2(SyncBackendHost* backend,
    const DataTypeController::TypeMap& controllers)
    : backend_(backend),
      controllers_(controllers),
      state_(DataTypeManager::STOPPED),
      current_dtc_(NULL),
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

DataTypeManagerImpl2::~DataTypeManagerImpl2() {}

void DataTypeManagerImpl2::Configure(const TypeSet& desired_types) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (state_ == STOPPING) {
    // You can not set a configuration while stopping.
    LOG(ERROR) << "Configuration set while stopping.";
    return;
  }

  last_requested_types_ = desired_types;
  // Add any data type controllers into the needs_start_ list that are
  // currently NOT_RUNNING or STOPPING.
  needs_start_.clear();
  for (TypeSet::const_iterator it = desired_types.begin();
       it != desired_types.end(); ++it) {
    DataTypeController::TypeMap::const_iterator dtc = controllers_.find(*it);
    if (dtc != controllers_.end() &&
        (dtc->second->state() == DataTypeController::NOT_RUNNING ||
         dtc->second->state() == DataTypeController::STOPPING)) {
      needs_start_.push_back(dtc->second.get());
      VLOG(1) << "Will start " << dtc->second->name();
    }
  }
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

  // If nothing changed, we're done.
  if (needs_start_.empty() && needs_stop_.empty()) {
    state_ = CONFIGURED;
    NotifyStart();
    NotifyDone(OK);
    return;
  }

  Restart();
}

void DataTypeManagerImpl2::Restart() {
  VLOG(1) << "Restarting...";

  // If we are currently waiting for an asynchronous process to
  // complete, change our state to RESTARTING so those processes know
  // that we want to start over when they finish.
  if (state_ == DOWNLOAD_PENDING || state_ == CONFIGURING) {
    state_ = RESTARTING;
    return;
  }

  DCHECK(state_ == STOPPED || state_ == RESTARTING || state_ == CONFIGURED);
  current_dtc_ = NULL;

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
      method_factory_.NewRunnableMethod(&DataTypeManagerImpl2::DownloadReady));
}

void DataTypeManagerImpl2::DownloadReady() {
  DCHECK(state_ == DOWNLOAD_PENDING || state_ == RESTARTING);

  // If we had a restart while waiting for downloads, just restart.
  // Note: Restart() can cause DownloadReady to be directly invoked, so we post
  // a task to avoid re-entrancy issues.
  if (state_ == RESTARTING) {
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(&DataTypeManagerImpl2::Restart));
    return;
  }

  state_ = CONFIGURING;
  StartNextType();
}

void DataTypeManagerImpl2::StartNextType() {
  // If there are any data types left to start, start the one at the
  // front of the list.
  if (!needs_start_.empty()) {
    current_dtc_ = needs_start_[0];
    VLOG(1) << "Starting " << current_dtc_->name();
    current_dtc_->Start(
        NewCallback(this, &DataTypeManagerImpl2::TypeStartCallback));
    return;
  }

  // If no more data types need starting, we're done.
  DCHECK_EQ(state_, CONFIGURING);
  state_ = CONFIGURED;
  NotifyDone(OK);
}

void DataTypeManagerImpl2::TypeStartCallback(
    DataTypeController::StartResult result) {
  // When the data type controller invokes this callback, it must be
  // on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(current_dtc_);

  if (state_ == RESTARTING) {
    // If configuration changed while this data type was starting, we
    // need to reset.
    Restart();
    return;
  } else if (state_ == STOPPING) {
    // If we reach this callback while stopping, this means that
    // DataTypeManager::Stop() was called while the current data type
    // was starting.  Now that it has finished starting, we can finish
    // stopping the DataTypeManager.  This is considered an ABORT.
    FinishStopAndNotify(ABORTED);
    return;
  } else if (state_ == STOPPED) {
    // If our state_ is STOPPED, we have already stopped all of the data
    // types.  We should not be getting callbacks from stopped data types.
    LOG(ERROR) << "Start callback called by stopped data type!";
    return;
  }

  // We're done with the data type at the head of the list -- remove it.
  DataTypeController* started_dtc = current_dtc_;
  DCHECK(needs_start_.size());
  DCHECK_EQ(needs_start_[0], started_dtc);
  needs_start_.erase(needs_start_.begin());
  current_dtc_ = NULL;

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
  FinishStopAndNotify(configure_result);
}

void DataTypeManagerImpl2::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (state_ == STOPPED)
    return;

  // If we are currently configuring, then the current type is in a
  // partially started state.  Abort the startup of the current type,
  // which will synchronously invoke the start callback.
  if (state_ == CONFIGURING) {
    state_ = STOPPING;
    current_dtc_->Stop();
    return;
  }

  const bool download_pending = state_ == DOWNLOAD_PENDING;
  state_ = STOPPING;
  if (download_pending) {
    // If Stop() is called while waiting for download, cancel all
    // outstanding tasks.
    method_factory_.RevokeAll();
    FinishStopAndNotify(ABORTED);
    return;
  }

  FinishStop();
}

void DataTypeManagerImpl2::FinishStop() {
  DCHECK(state_== CONFIGURING || state_ == STOPPING);
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

void DataTypeManagerImpl2::FinishStopAndNotify(ConfigureResult result) {
  FinishStop();
  NotifyDone(result);
}

void DataTypeManagerImpl2::NotifyStart() {
  NotificationService::current()->Notify(
      NotificationType::SYNC_CONFIGURE_START,
      Source<DataTypeManager>(this),
      NotificationService::NoDetails());
}

void DataTypeManagerImpl2::NotifyDone(ConfigureResult result) {
  NotificationService::current()->Notify(
      NotificationType::SYNC_CONFIGURE_DONE,
      Source<DataTypeManager>(this),
      Details<ConfigureResult>(&result));
}

const DataTypeController::TypeMap& DataTypeManagerImpl2::controllers() {
  return controllers_;
}

DataTypeManager::State DataTypeManagerImpl2::state() {
  return state_;
}

}  // namespace browser_sync
