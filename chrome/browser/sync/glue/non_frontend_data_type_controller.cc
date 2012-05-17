// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/syncable/model_type.h"
#include "sync/util/data_type_histogram.h"

using content::BrowserThread;

namespace browser_sync {

NonFrontendDataTypeController::NonFrontendDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : profile_sync_factory_(profile_sync_factory),
      profile_(profile),
      profile_sync_service_(sync_service),
      state_(NOT_RUNNING),
      abort_association_(false),
      abort_association_complete_(false, false),
      start_association_called_(true, false),
      start_models_failed_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_sync_factory_);
  DCHECK(profile_);
  DCHECK(profile_sync_service_);
}

void NonFrontendDataTypeController::Start(const StartCallback& start_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!start_callback.is_null());
  start_association_called_.Reset();
  start_models_failed_ = false;
  if (state_ != NOT_RUNNING) {
    start_callback.Run(BUSY, SyncError());
    return;
  }

  start_callback_ = start_callback;
  abort_association_ = false;

  state_ = MODEL_STARTING;
  if (!StartModels()) {
    start_models_failed_ = true;
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early.
    DCHECK(state_ == NOT_RUNNING || state_ == MODEL_STARTING
           || state_ == DISABLED);
    return;
  }

  // Kick off association on the thread the datatype resides on.
  state_ = ASSOCIATING;
  if (!StartAssociationAsync()) {
    SyncError error(FROM_HERE, "Failed to post StartAssociation", type());
    StartDoneImpl(ASSOCIATION_FAILED, DISABLED, error);
  }
}

void NonFrontendDataTypeController::StopWhileAssociating() {
  state_ = STOPPING;
  {
    base::AutoLock lock(abort_association_lock_);
    abort_association_ = true;
    if (model_associator_.get())
      model_associator_->AbortAssociation();
    if (!start_association_called_.IsSignaled()) {
      StartDoneImpl(ABORTED, NOT_RUNNING, SyncError());
      return; // There is nothing more for us to do.
    }
  }

  // Wait for the model association to abort.
  if (start_association_called_.IsSignaled()) {
    LOG(INFO) << "Stopping after |StartAssocation| is called.";
    if (start_models_failed_) {
      LOG(INFO) << "Start models failed";
      abort_association_complete_.Wait();
    } else {
      LOG(INFO) << "Start models succeeded";
      abort_association_complete_.Wait();
    }
  } else {
    LOG(INFO) << "Stopping before |StartAssocation| is called.";
    if (start_models_failed_) {
      LOG(INFO) << "Start models failed";
      abort_association_complete_.Wait();
    } else {
      LOG(INFO) << "Start models succeeded";
      abort_association_complete_.Wait();
    }

  }

  StartDoneImpl(ABORTED, STOPPING, SyncError());
}

namespace {
// Helper function that signals the UI thread once the StopAssociation task
// has finished completing (this task is queued after the StopAssociation task).
void SignalCallback(base::WaitableEvent* wait_event) {
  wait_event->Signal();
}
}  // namespace

void NonFrontendDataTypeController::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_NE(state_, NOT_RUNNING);

  // TODO(sync): Blocking the UI thread at shutdown is bad. The new API avoids
  // this. Once all non-frontend datatypes use the new API, we can get rid of this
  // locking (see implementation in AutofillProfileDataTypeController).
  // http://crbug.com/19757
  base::ThreadRestrictions::ScopedAllowWait allow_wait;

  // If Stop() is called while Start() is waiting for association to
  // complete, we need to abort the association and wait for the DB
  // thread to finish the StartImpl() task.
  switch (state_) {
    case ASSOCIATING:
      if (type() == syncable::PASSWORDS) {
        LOG(INFO) << " Type is Passwords";
        StopWhileAssociating();
      } else if (type() == syncable::TYPED_URLS) {
        LOG(INFO) << " Type is TypedUrl";
        StopWhileAssociating();
      } else if (type() == syncable::APPS) {
        LOG(INFO) << "Type is Apps";
        StopWhileAssociating();
      } else if (type() == syncable::EXTENSIONS) {
        LOG(INFO) << "Type is Extension";
        StopWhileAssociating();
      } else if (type() == syncable::PREFERENCES) {
        LOG(INFO) << "Type is Preferences(Does not belong to non-frontend)";
        StopWhileAssociating();
      } else if (type() == syncable::EXTENSION_SETTINGS) {
        LOG(INFO) << "Type is Extension Settings";
        StopWhileAssociating();
      } else if (type() == syncable::APP_SETTINGS) {
        LOG(INFO) << "Type is App Settings.";
        StopWhileAssociating();
      } else if (type() == syncable::BOOKMARKS) {
        LOG(INFO) << "Type is BOOKMARKS.";
        StopWhileAssociating();
      } else if (type() == syncable::AUTOFILL_PROFILE) {
        LOG(INFO) << "Type is AUTOFILL_PROFILE.";
        StopWhileAssociating();
      } else if (type() == syncable::AUTOFILL) {
        LOG(INFO) << "Type is AUTOFILL.";
        StopWhileAssociating();
      } else if (type() == syncable::THEMES) {
        LOG(INFO) << "Type is THEMES.";
        StopWhileAssociating();
      } else if (type() == syncable::NIGORI) {
        LOG(INFO) << "Type is NIGORI.";
        StopWhileAssociating();
      } else if (type() == syncable::SEARCH_ENGINES) {
        LOG(INFO) << "Type is SEARCH_ENGINES.";
        StopWhileAssociating();
      } else if (type() == syncable::SESSIONS) {
        LOG(INFO) << "Type is SESSIONS.";
        StopWhileAssociating();
      } else if (type() == syncable::APP_NOTIFICATIONS) {
        LOG(INFO) << "Type is APP_NOTIFICATIONS.";
        StopWhileAssociating();
      } else {
        LOG(INFO) << "Type is unknown";
        StopWhileAssociating();
      }

      // TODO(sync) : This should be cleaned up. Once we move to the new api
      // this should not be a problem.
      if (!start_association_called_.IsSignaled()) {
        // If datatype's thread has not even picked up executing it is safe
        // to bail out now. We have no more state cleanups to do.
        // The risk of waiting is that the datatype thread might not respond.
        return;
      }
      break;
    case MODEL_STARTING:
      state_ = STOPPING;
      // If Stop() is called while Start() is waiting for the models to start,
      // abort the start. We don't need to continue on since it means we haven't
      // kicked off the association, and once we call StopModels, we never will.
      StartDoneImpl(ABORTED, NOT_RUNNING, SyncError());
      return;
    case DISABLED:
      state_ = NOT_RUNNING;
      StopModels();
      return;
    default:
      DCHECK_EQ(state_, RUNNING);
      state_ = STOPPING;
      StopModels();
      break;
  }
  DCHECK(start_callback_.is_null());

  // Deactivate the change processor on the UI thread. We dont want to listen
  // for any more changes or process them from server.
  profile_sync_service_->DeactivateDataType(type());

  if (!StopAssociationAsync()) {
    // We do DFATAL here because this will eventually lead to a failed CHECK
    // when the change processor gets destroyed on the wrong thread.
    LOG(DFATAL) << "Failed to destroy datatype " << name();
  }
  state_ = NOT_RUNNING;
}

std::string NonFrontendDataTypeController::name() const {
  // For logging only.
  return syncable::ModelTypeToString(type());
}

DataTypeController::State NonFrontendDataTypeController::state() const {
  return state_;
}

void NonFrontendDataTypeController::OnUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  RecordUnrecoverableError(from_here, message);
  BrowserThread::PostTask(BrowserThread::UI, from_here,
      base::Bind(&NonFrontendDataTypeController::OnUnrecoverableErrorImpl,
                 this,
                 from_here,
                 message));
}

void NonFrontendDataTypeController::OnSingleDatatypeUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  RecordUnrecoverableError(from_here, message);
  BrowserThread::PostTask(BrowserThread::UI, from_here,
      base::Bind(&NonFrontendDataTypeController::DisableImpl,
                 this,
                 from_here,
                 message));
}

NonFrontendDataTypeController::NonFrontendDataTypeController()
    : profile_sync_factory_(NULL),
      profile_(NULL),
      profile_sync_service_(NULL),
      state_(NOT_RUNNING),
      abort_association_(false),
      abort_association_complete_(false, false),
      start_association_called_(true, false) {
}

NonFrontendDataTypeController::~NonFrontendDataTypeController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool NonFrontendDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association, so do nothing.
  return true;
}

void NonFrontendDataTypeController::StartFailed(StartResult result,
                                                const SyncError& error) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (IsUnrecoverableResult(result))
    RecordUnrecoverableError(FROM_HERE, "StartFailed");
  StopAssociation();
  StartDone(result,
            result == ASSOCIATION_FAILED ? DISABLED : NOT_RUNNING,
            error);
}

void NonFrontendDataTypeController::StartDone(
    DataTypeController::StartResult result,
    DataTypeController::State new_state,
    const SyncError& error) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  abort_association_complete_.Signal();
  base::AutoLock lock(abort_association_lock_);
  if (!abort_association_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&NonFrontendDataTypeController::StartDoneImpl,
                   this,
                   result,
                   new_state,
                   error));
  }
}

void NonFrontendDataTypeController::StartDoneImpl(
    DataTypeController::StartResult result,
    DataTypeController::State new_state,
    const SyncError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // It's possible to have StartDoneImpl called first from the UI thread
  // (due to Stop being called) and then posted from the non-UI thread. In
  // this case, we drop the second call because we've already been stopped.
  if (state_ == NOT_RUNNING) {
    DCHECK(start_callback_.is_null());
    return;
  }

  state_ = new_state;
  if (state_ != RUNNING) {
    // Start failed.
    StopModels();
    RecordStartFailure(result);
  }

  // We have to release the callback before we call it, since it's possible
  // invoking the callback will trigger a call to STOP(), which will get
  // confused by the non-NULL start_callback_.
  StartCallback callback = start_callback_;
  start_callback_.Reset();
  callback.Run(result, error);
}

void NonFrontendDataTypeController::StopModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state_ == STOPPING || state_ == NOT_RUNNING || state_ == DISABLED);
  DVLOG(1) << "NonFrontendDataTypeController::StopModels(): State = " << state_;
  // Do nothing by default.
}

void NonFrontendDataTypeController::OnUnrecoverableErrorImpl(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_sync_service_->OnUnrecoverableError(from_here, message);
}

void NonFrontendDataTypeController::DisableImpl(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_sync_service_->OnDisableDatatype(type(), from_here, message);
}

void NonFrontendDataTypeController::RecordAssociationTime(
    base::TimeDelta time) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_TIMES("Sync." type_str "AssociationTime", time);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void NonFrontendDataTypeController::RecordStartFailure(StartResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures", type(),
                            syncable::MODEL_TYPE_COUNT);
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_ENUMERATION("Sync." type_str "StartFailure", result, \
                              MAX_START_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

bool NonFrontendDataTypeController::StartAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  return PostTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&NonFrontendDataTypeController::StartAssociation, this));
}

ProfileSyncComponentsFactory*
    NonFrontendDataTypeController::profile_sync_factory() const {
  return profile_sync_factory_;
}

Profile* NonFrontendDataTypeController::profile() const {
  return profile_;
}

ProfileSyncService* NonFrontendDataTypeController::profile_sync_service()
    const {
  return profile_sync_service_;
}

void NonFrontendDataTypeController::set_start_callback(
    const StartCallback& callback) {
  start_callback_ = callback;
}

void NonFrontendDataTypeController::set_state(State state) {
  state_ = state;
}

AssociatorInterface* NonFrontendDataTypeController::associator() const {
  return model_associator_.get();
}

void NonFrontendDataTypeController::set_model_associator(
    AssociatorInterface* associator) {
  model_associator_.reset(associator);
}

ChangeProcessor* NonFrontendDataTypeController::change_processor() const {
  return change_processor_.get();
}

void NonFrontendDataTypeController::set_change_processor(
    ChangeProcessor* change_processor) {
  change_processor_.reset(change_processor);
}

void NonFrontendDataTypeController::StartAssociation() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock lock(abort_association_lock_);
    if (abort_association_) {
      abort_association_complete_.Signal();
      return;
    }
    start_association_called_.Signal();
    CreateSyncComponents();
  }

  DCHECK_EQ(state_, ASSOCIATING);

  if (!model_associator_->CryptoReadyIfNecessary()) {
    StartFailed(NEEDS_CRYPTO, SyncError());
    return;
  }

  bool sync_has_nodes = false;
  if (!model_associator_->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    SyncError error(FROM_HERE, "Failed to load sync nodes", type());
    StartFailed(UNRECOVERABLE_ERROR, error);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  SyncError error;
  error = model_associator_->AssociateModels();
  // TODO(lipalani): crbug.com/122690 - handle abort.
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (error.IsSet()) {
    StartFailed(ASSOCIATION_FAILED, error);
    return;
  }

  profile_sync_service_->ActivateDataType(type(), model_safe_group(),
                                          change_processor());
  StartDone(!sync_has_nodes ? OK_FIRST_RUN : OK, RUNNING, SyncError());
}

bool NonFrontendDataTypeController::StopAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), STOPPING);
  if (PostTaskOnBackendThread(
          FROM_HERE,
          base::Bind(
              &NonFrontendDataTypeController::StopAssociation, this))) {
    // The remote thread will hold on to a reference to this object until
    // the StopAssociation task finishes running. We want to make sure that we
    // do not return from this routine until there are no more references to
    // this object on the remote thread, so we queue up the SignalCallback
    // task below - this task does not maintain a reference to the DTC, so
    // when it signals this thread, we know that the previous task has executed
    // and there are no more lingering remote references to the DTC.
    // This fixes the race described in http://crbug.com/127706.
    base::WaitableEvent datatype_stopped(false, false);
    if (PostTaskOnBackendThread(
            FROM_HERE,
            base::Bind(&SignalCallback, &datatype_stopped))) {
      datatype_stopped.Wait();
      return true;
    }
  }
  return false;
}

void NonFrontendDataTypeController::StopAssociation() {
  DCHECK(!HasOneRef());
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (model_associator_.get()) {
    SyncError error;  // Not used.
    error = model_associator_->DisassociateModels();
  }
  model_associator_.reset();
  change_processor_.reset();
}

}  // namespace browser_sync
