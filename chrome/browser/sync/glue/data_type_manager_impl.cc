// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <functional>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/glue/sync_backend_host.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"

namespace browser_sync {

namespace {

static const syncable::ModelType kStartOrder[] = {
  syncable::BOOKMARKS,
  syncable::PREFERENCES,
  syncable::AUTOFILL,
  syncable::AUTOFILL_PROFILE,
  syncable::THEMES,
  syncable::TYPED_URLS,
  syncable::PASSWORDS,
  syncable::SESSIONS,
};

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

DataTypeManagerImpl::DataTypeManagerImpl(
    SyncBackendHost* backend,
    const DataTypeController::TypeMap& controllers)
    : backend_(backend),
      controllers_(controllers),
      state_(DataTypeManager::STOPPED),
      syncer_paused_(false),
      needs_reconfigure_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(backend_);
  DCHECK_GT(arraysize(kStartOrder), 0U);
  // Ensure all data type controllers are stopped.
  for (DataTypeController::TypeMap::const_iterator it = controllers_.begin();
       it != controllers_.end(); ++it) {
    DCHECK_EQ(DataTypeController::NOT_RUNNING, (*it).second->state());
  }

  // Build a ModelType -> order map for sorting.
  for (int i = 0; i < static_cast<int>(arraysize(kStartOrder)); i++)
    start_order_[kStartOrder[i]] = i;
}

DataTypeManagerImpl::~DataTypeManagerImpl() {
}

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

  // If there were new types needing download, a nudge will have been sent and
  // we should be in DOWNLOAD_PENDING.  In that case we start the syncer thread
  // (which is idempotent) to fetch these updates.
  // However, we could actually be in PAUSE_PENDING here as if no new types
  // were needed, our DownloadReady callback will have fired and we will have
  // requested a pause already (so starting the syncer thread will still not
  // let it make forward progress as the pause needs to be resumed by us).
  // Because both the pause and start syncing commands are posted FCFS to the
  // core thread, there is no race between the pause and the start; the pause
  // will always win, so we will always start paused if we don't need to
  // download new types.  Thus, in almost all cases, the syncer thread DOES NOT
  // start before model association. But...
  //
  // TODO(tim): Bug 47957. There is still subtle badness here. If we just
  // restarted the browser and were upgraded in between, we may be in a state
  // where a bunch of data types do have initial sync ended, but a new guy
  // does not.  In this case, what we really want is to _only_ download updates
  // for that new type and not the ones that have already finished and we've
  // presumably associated before.  What happens now is the syncer is nudged
  // and it does a sync from timestamp 0 for only the new types, and sends the
  // OnSyncCycleCompleted event, which is how we get the DownloadReady call.
  // We request a pause at this point, but it is done asynchronously. So in
  // theory, the syncer could charge forward with another sync (for _all_
  // types) before the pause is serviced, which could be bad for associating
  // models as we'll merge the present cloud with the immediate past, which
  // opens the door to bugs like "bookmark came back from dead".  A lot more
  // stars have to align now for this to happen, but it's still there.
  backend_->StartSyncingWithServer();
}

void DataTypeManagerImpl::DownloadReady() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state_ == DOWNLOAD_PENDING);

  if (syncer_paused_) {
    state_ = CONFIGURING;
    StartNextType();
    return;
  }

  // Pause the sync backend before starting the data types.
  state_ = PAUSE_PENDING;
  PauseSyncer();
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
    state_ = BLOCKED;
    needs_reconfigure_ = false;
    VLOG(1) << "Reconfiguring due to previous configure attempt occuring while"
            << " busy.";
    Configure(last_requested_types_);
    return;
  }

  // Do a fresh calculation to see if controllers need starting to account for
  // things like encryption, which may still need to be sorted out before we
  // can announce we're "Done" configuration entirely.
  if (GetControllersNeedingStart(NULL)) {
    state_ = BLOCKED;
    return;
  }

  // If no more data types need starting, we're done.  Resume the sync backend
  // to finish.
  state_ = RESUME_PENDING;
  ResumeSyncer();
}

void DataTypeManagerImpl::TypeStartCallback(
    DataTypeController::StartResult result,
    const tracked_objects::Location& location) {
  // When the data type controller invokes this callback, it must be
  // on the UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We're done with the data type at the head of the list -- remove it.
  DataTypeController* started_dtc = needs_start_[0];
  DCHECK(needs_start_.size());
  DCHECK_EQ(needs_start_[0], started_dtc);
  needs_start_.erase(needs_start_.begin());

  // If we reach this callback while stopping, this means that
  // DataTypeManager::Stop() was called while the current data type
  // was starting.  Now that it has finished starting, we can finish
  // stopping the DataTypeManager.  This is considered an ABORT.
  if (state_ == STOPPING) {
    FinishStopAndNotify(ABORTED, FROM_HERE);
    return;
  }

  // If our state_ is STOPPED, we have already stopped all of the data
  // types.  We should not be getting callbacks from stopped data types.
  if (state_ == STOPPED) {
    LOG(ERROR) << "Start callback called by stopped data type!";
    return;
  }

  // If the type is waiting for the cryptographer, continue to the next type.
  // Once the cryptographer is ready, we'll attempt to restart this type.
  if (result == DataTypeController::NEEDS_CRYPTO) {
    VLOG(1) << "Waiting for crypto " << started_dtc->name();
    StartNextType();
    return;
  }

  // If the type started normally, continue to the next type.
  if (result == DataTypeController::OK ||
      result == DataTypeController::OK_FIRST_RUN) {
    VLOG(1) << "Started " << started_dtc->name();
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

  // If Stop() is called while waiting for pause or resume, we no
  // longer care about this.
  bool aborted = false;
  if (state_ == PAUSE_PENDING) {
    RemoveObserver(NotificationType::SYNC_PAUSED);
    aborted = true;
  }
  if (state_ == RESUME_PENDING) {
    RemoveObserver(NotificationType::SYNC_RESUMED);
    aborted = true;
  }

  // If Stop() is called while waiting for download, cancel all
  // outstanding tasks.
  if (state_ == DOWNLOAD_PENDING) {
    method_factory_.RevokeAll();
    aborted = true;
  }

  state_ = STOPPING;
  if (aborted)
    FinishStopAndNotify(ABORTED, FROM_HERE);
  else
    FinishStop();
}

const DataTypeController::TypeMap& DataTypeManagerImpl::controllers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return controllers_;
}

DataTypeManager::State DataTypeManagerImpl::state() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return state_;
}

void DataTypeManagerImpl::FinishStop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state_== CONFIGURING ||
         state_ == STOPPING ||
         state_ == PAUSE_PENDING ||
         state_ == RESUME_PENDING ||
         state_ == BLOCKED);
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FinishStop();
  NotifyDone(result, location);
}

void DataTypeManagerImpl::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (type.value) {
    case NotificationType::SYNC_PAUSED:
      DCHECK(state_ == PAUSE_PENDING);
      DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
      syncer_paused_ = true;
      RemoveObserver(NotificationType::SYNC_PAUSED);

      state_ = CONFIGURING;
      StartNextType();
      break;
    case NotificationType::SYNC_RESUMED:
      DCHECK(state_ == RESUME_PENDING);
      RemoveObserver(NotificationType::SYNC_RESUMED);
      syncer_paused_ = false;

      if (needs_reconfigure_) {
        // An attempt was made to reconfigure while we were already configuring.
        // This can be because a passphrase was accepted or the user changed the
        // set of desired types. Either way, |last_requested_types_| will
        // contain the most recent set of desired types, so we just call
        // configure.
        // Note: we do this whether or not GetControllersNeedingStart is true,
        // because we may need to stop datatypes.
        state_ = BLOCKED;
        needs_reconfigure_ = false;
        VLOG(1) << "Reconfiguring due to previous configure attempt occuring "
                << " while busy.";
        Configure(last_requested_types_);
        return;
      }

      state_ = CONFIGURED;
      NotifyDone(OK, FROM_HERE);
      break;
    default:
      NOTREACHED();
  }
}

void DataTypeManagerImpl::AddObserver(NotificationType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  notification_registrar_.Add(this,
                              type,
                              NotificationService::AllSources());
}

void DataTypeManagerImpl::RemoveObserver(NotificationType type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  notification_registrar_.Remove(this,
                                 type,
                                 NotificationService::AllSources());
}

void DataTypeManagerImpl::NotifyStart() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NotificationService::current()->Notify(
      NotificationType::SYNC_CONFIGURE_START,
      Source<DataTypeManager>(this),
      NotificationService::NoDetails());
}

void DataTypeManagerImpl::NotifyDone(ConfigureResult result,
    const tracked_objects::Location& location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ConfigureResultWithErrorLocation result_with_location(result, location);
  NotificationService::current()->Notify(
      NotificationType::SYNC_CONFIGURE_DONE,
      Source<DataTypeManager>(this),
      Details<ConfigureResultWithErrorLocation>(&result_with_location));
}

void DataTypeManagerImpl::ResumeSyncer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AddObserver(NotificationType::SYNC_RESUMED);
  if (!backend_->RequestResume()) {
    RemoveObserver(NotificationType::SYNC_RESUMED);
    FinishStopAndNotify(UNRECOVERABLE_ERROR, FROM_HERE);
  }
}

void DataTypeManagerImpl::PauseSyncer() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AddObserver(NotificationType::SYNC_PAUSED);
  if (!backend_->RequestPause()) {
    RemoveObserver(NotificationType::SYNC_PAUSED);
    FinishStopAndNotify(UNRECOVERABLE_ERROR, FROM_HERE);
  }
}

}  // namespace browser_sync
