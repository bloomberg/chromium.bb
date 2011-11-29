// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/api/sync_error.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace browser_sync {

NonFrontendDataTypeController::NonFrontendDataTypeController()
    : profile_sync_factory_(NULL),
      profile_(NULL),
      profile_sync_service_(NULL),
      state_(NOT_RUNNING),
      abort_association_(false),
      abort_association_complete_(false, false),
      datatype_stopped_(false, false) {}

NonFrontendDataTypeController::NonFrontendDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
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
    start_callback->Run(BUSY, SyncError());
    delete start_callback;
    return;
  }

  start_callback_.reset(start_callback);
  abort_association_ = false;

  state_ = MODEL_STARTING;
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early.
    DCHECK(state_ == NOT_RUNNING || state_ == MODEL_STARTING);
    return;
  }

  // Kick off association on the thread the datatype resides on.
  state_ = ASSOCIATING;
  if (!StartAssociationAsync()) {
    SyncError error(FROM_HERE, "Failed to post StartAssociation", type());
    StartDoneImpl(ASSOCIATION_FAILED, DISABLED, error);
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
  bool merge_success = model_associator_->AssociateModels(&error);
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (!merge_success) {
    StartFailed(ASSOCIATION_FAILED, error);
    return;
  }

  profile_sync_service_->ActivateDataType(type(), model_safe_group(),
                                          change_processor());
  StartDone(!sync_has_nodes ? OK_FIRST_RUN : OK, RUNNING, SyncError());
}

void NonFrontendDataTypeController::StartFailed(StartResult result,
                                                const SyncError& error) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
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
    DCHECK(!start_callback_.get());
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
  scoped_ptr<StartCallback> callback(start_callback_.release());
  callback->Run(result, error);
}

// TODO(sync): Blocking the UI thread at shutdown is bad. The new API avoids
// this. Once all non-frontend datatypes use the new API, we can get rid of this
// locking (see implementation in AutofillProfileDataTypeController).
void NonFrontendDataTypeController::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_NE(state_, NOT_RUNNING);
  // If Stop() is called while Start() is waiting for association to
  // complete, we need to abort the association and wait for the DB
  // thread to finish the StartImpl() task.
  switch (state_) {
    case ASSOCIATING:
      state_ = STOPPING;
      {
        base::AutoLock lock(abort_association_lock_);
        abort_association_ = true;
        if (model_associator_.get())
          model_associator_->AbortAssociation();
      }
      // Wait for the model association to abort.
      abort_association_complete_.Wait();
      StartDoneImpl(ABORTED, STOPPING, SyncError());
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
  DCHECK(!start_callback_.get());

  // Deactivate the change processor on the UI thread. We dont want to listen
  // for any more changes or process them from server.
  profile_sync_service_->DeactivateDataType(type());

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
  DCHECK(state_ == STOPPING || state_ == NOT_RUNNING || state_ == DISABLED);
  DVLOG(1) << "NonFrontendDataTypeController::StopModels(): State = " << state_;
  // Do nothing by default.
}

void NonFrontendDataTypeController::StopAssociation() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (model_associator_.get()) {
    SyncError error;  // Not used.
    model_associator_->DisassociateModels(&error);
  }
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
  BrowserThread::PostTask(BrowserThread::UI, from_here,
      base::Bind(&NonFrontendDataTypeController::OnUnrecoverableErrorImpl,
                 this,
                 from_here,
                 message));
}

void NonFrontendDataTypeController::OnUnrecoverableErrorImpl(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_sync_service_->OnUnrecoverableError(from_here, message);
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
    StartCallback* callback) {
  start_callback_.reset(callback);
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

}  // namespace browser_sync
