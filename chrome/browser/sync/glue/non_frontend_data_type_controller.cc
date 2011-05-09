// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "content/browser/browser_thread.h"

namespace browser_sync {

NonFrontendDataTypeController::NonFrontendDataTypeController()
    : profile_sync_factory_(NULL),
      profile_(NULL),
      profile_sync_service_(NULL),
      abort_association_(false),
      abort_association_complete_(false, false),
      datatype_stopped_(false, false) {}

NonFrontendDataTypeController::NonFrontendDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile)
    : profile_sync_factory_(profile_sync_factory),
      profile_(profile),
      profile_sync_service_(profile->GetProfileSyncService()),
      state_(NOT_RUNNING),
      abort_association_(false),
      abort_association_complete_(false, false),
      datatype_stopped_(false, false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_sync_factory_);
  DCHECK(profile_);
  DCHECK(profile_sync_service_);
}

NonFrontendDataTypeController::~NonFrontendDataTypeController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void NonFrontendDataTypeController::Start(StartCallback* start_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(start_callback);
  if (state_ != NOT_RUNNING) {
    start_callback->Run(BUSY, FROM_HERE);
    delete start_callback;
    return;
  }

  start_callback_.reset(start_callback);
  abort_association_ = false;

  state_ = MODEL_STARTING;
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early. state_ will control
    // what we perform next.
    DCHECK(state_ == NOT_RUNNING || state_ == MODEL_STARTING);
    return;
  }

  // Kick off association on the thread the datatype resides on.
  state_ = ASSOCIATING;
  if (!StartAssociationAsync()) {
    StartDoneImpl(ASSOCIATION_FAILED, NOT_RUNNING, FROM_HERE);
  }
}

bool NonFrontendDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association, so do nothing.
  return true;
}

void NonFrontendDataTypeController::StartAssociation() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state_, ASSOCIATING);
  {
    base::AutoLock lock(abort_association_lock_);
    if (abort_association_) {
      abort_association_complete_.Signal();
      return;
    }
    CreateSyncComponents();
  }

  if (!model_associator_->CryptoReadyIfNecessary()) {
    StartFailed(NEEDS_CRYPTO, FROM_HERE);
    return;
  }

  bool sync_has_nodes = false;
  if (!model_associator_->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    StartFailed(UNRECOVERABLE_ERROR, FROM_HERE);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  bool merge_success = model_associator_->AssociateModels();
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (!merge_success) {
    StartFailed(ASSOCIATION_FAILED, FROM_HERE);
    return;
  }

  profile_sync_service_->ActivateDataType(this, change_processor_.get());
  StartDone(!sync_has_nodes ? OK_FIRST_RUN : OK, RUNNING, FROM_HERE);
}

void NonFrontendDataTypeController::StartFailed(StartResult result,
    const tracked_objects::Location& location) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  model_associator_.reset();
  change_processor_.reset();
  StartDone(result, NOT_RUNNING, location);
}

void NonFrontendDataTypeController::StartDone(
    DataTypeController::StartResult result,
    DataTypeController::State new_state,
    const tracked_objects::Location& location) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  abort_association_complete_.Signal();
  base::AutoLock lock(abort_association_lock_);
  if (!abort_association_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(
            this,
            &NonFrontendDataTypeController::StartDoneImpl,
            result,
            new_state,
            location));
  }
}

void NonFrontendDataTypeController::StartDoneImpl(
    DataTypeController::StartResult result,
    DataTypeController::State new_state,
    const tracked_objects::Location& location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  state_ = new_state;
  if (state_ != RUNNING) {
    // Start failed.
    StopModels();
    RecordStartFailure(result);
  }

  // We have to release the callback before we call it, since it's possible
  // invoking the callback will trigger a call to STOP(), which will get
  // confused by the non-NULL start_callback_.
  scoped_ptr<StartCallback> callback(start_callback_.release());
  callback->Run(result, location);
}

// TODO(sync): Blocking the UI thread at shutdown is bad. If we had a way of
// distinguishing chrome shutdown from sync shutdown, we should be able to avoid
// this (http://crbug.com/55662).
void NonFrontendDataTypeController::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If Stop() is called while Start() is waiting for association to
  // complete, we need to abort the association and wait for the DB
  // thread to finish the StartImpl() task.
  if (state_ == ASSOCIATING) {
    state_ = STOPPING;
    {
      base::AutoLock lock(abort_association_lock_);
      abort_association_ = true;
      if (model_associator_.get())
        model_associator_->AbortAssociation();
    }
    // Wait for the model association to abort.
    abort_association_complete_.Wait();
    StartDoneImpl(ABORTED, STOPPING, FROM_HERE);
  } else if (state_ == MODEL_STARTING) {
    state_ = STOPPING;
    // If Stop() is called while Start() is waiting for the models to start,
    // abort the start. We don't need to continue on since it means we haven't
    // kicked off the association, and once we call StopModels, we never will.
    StartDoneImpl(ABORTED, NOT_RUNNING, FROM_HERE);
    return;
  } else {
    state_ = STOPPING;

    StopModels();
  }
  DCHECK(!start_callback_.get());

  // Deactivate the change processor on the UI thread. We dont want to listen
  // for any more changes or process them from server.
  if (change_processor_.get())
    profile_sync_service_->DeactivateDataType(this, change_processor_.get());

  if (StopAssociationAsync()) {
    datatype_stopped_.Wait();
  } else {
    // We do DFATAL here because this will eventually lead to a failed CHECK
    // when the change processor gets destroyed on the wrong thread.
    LOG(DFATAL) << "Failed to destroy datatype " << name();
  }
  state_ = NOT_RUNNING;
}

void NonFrontendDataTypeController::StopModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state_ == STOPPING || state_ == NOT_RUNNING);
  // Do nothing by default.
}

void NonFrontendDataTypeController::StopAssociation() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (model_associator_.get())
    model_associator_->DisassociateModels();
  model_associator_.reset();
  change_processor_.reset();
  datatype_stopped_.Signal();
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
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, NewRunnableMethod(this,
      &NonFrontendDataTypeController::OnUnrecoverableErrorImpl, from_here,
      message));
}

void NonFrontendDataTypeController::OnUnrecoverableErrorImpl(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_sync_service_->OnUnrecoverableError(from_here, message);
}

ProfileSyncFactory* NonFrontendDataTypeController::profile_sync_factory()
    const {
  return profile_sync_factory_;
}

Profile* NonFrontendDataTypeController::profile() const {
  return profile_;
}

ProfileSyncService* NonFrontendDataTypeController::profile_sync_service()
    const {
  return profile_sync_service_;
}

void NonFrontendDataTypeController::set_state(State state) {
  state_ = state;
}

void NonFrontendDataTypeController::set_model_associator(
    AssociatorInterface* associator) {
  model_associator_.reset(associator);
}

void NonFrontendDataTypeController::set_change_processor(
    ChangeProcessor* change_processor) {
  change_processor_.reset(change_processor);
}

}  // namespace browser_sync
