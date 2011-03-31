// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/password_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/password_change_processor.h"
#include "chrome/browser/sync/glue/password_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "content/browser/browser_thread.h"

namespace browser_sync {

PasswordDataTypeController::PasswordDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : profile_sync_factory_(profile_sync_factory),
      profile_(profile),
      sync_service_(sync_service),
      state_(NOT_RUNNING),
      abort_association_(false),
      abort_association_complete_(false, false),
      datatype_stopped_(false, false) {
  DCHECK(profile_sync_factory);
  DCHECK(profile);
  DCHECK(sync_service);
}

PasswordDataTypeController::~PasswordDataTypeController() {
}

void PasswordDataTypeController::Start(StartCallback* start_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(start_callback);
  if (state_ != NOT_RUNNING) {
    start_callback->Run(BUSY, FROM_HERE);
    delete start_callback;
    return;
  }

  password_store_ = profile_->GetPasswordStore(Profile::EXPLICIT_ACCESS);
  if (!password_store_.get()) {
    LOG(ERROR) << "PasswordStore not initialized, password datatype controller"
               << " aborting.";
    state_ = NOT_RUNNING;
    start_callback->Run(ABORTED, FROM_HERE);
    delete start_callback;
    return;
  }

  start_callback_.reset(start_callback);
  abort_association_ = false;
  set_state(ASSOCIATING);
  password_store_->ScheduleTask(
      NewRunnableMethod(this, &PasswordDataTypeController::StartImpl));
}

// TODO(sync): Blocking the UI thread at shutdown is bad. If we had a way of
// distinguishing chrome shutdown from sync shutdown, we should be able to avoid
// this (http://crbug.com/55662). Further, all this functionality should be
// abstracted to a higher layer, where we could ensure all datatypes are doing
// the same thing (http://crbug.com/76232).
void PasswordDataTypeController::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // If Stop() is called while Start() is waiting for association to
  // complete, we need to abort the association and wait for the PASSWORD
  // thread to finish the StartImpl() task.
  if (state_ == ASSOCIATING) {
    {
      base::AutoLock lock(abort_association_lock_);
      abort_association_ = true;
      if (model_associator_.get())
        model_associator_->AbortAssociation();
    }
    // Wait for the model association to abort.
    abort_association_complete_.Wait();
    StartDoneImpl(ABORTED, STOPPING);
  }

  // If Stop() is called while Start() is waiting for another service to load,
  // abort the start.
  if (state_ == MODEL_STARTING)
    StartDoneImpl(ABORTED, STOPPING);

  DCHECK(!start_callback_.get());

  if (change_processor_ != NULL)
    sync_service_->DeactivateDataType(this, change_processor_.get());

  set_state(NOT_RUNNING);
  DCHECK(password_store_.get());
  password_store_->ScheduleTask(
      NewRunnableMethod(this, &PasswordDataTypeController::StopImpl));
  datatype_stopped_.Wait();
}

bool PasswordDataTypeController::enabled() {
  return true;
}

syncable::ModelType PasswordDataTypeController::type() const {
  return syncable::PASSWORDS;
}

browser_sync::ModelSafeGroup PasswordDataTypeController::model_safe_group()
    const {
  return browser_sync::GROUP_PASSWORD;
}

std::string PasswordDataTypeController::name() const {
  // For logging only.
  return "password";
}

DataTypeController::State PasswordDataTypeController::state() const {
  return state_;
}

void PasswordDataTypeController::StartImpl() {
  // No additional services need to be started before we can proceed
  // with model association.
  {
    base::AutoLock lock(abort_association_lock_);
    if (abort_association_) {
      abort_association_complete_.Signal();
      return;
    }
    ProfileSyncFactory::SyncComponents sync_components =
        profile_sync_factory_->CreatePasswordSyncComponents(
            sync_service_,
            password_store_.get(),
            this);
    model_associator_.reset(sync_components.model_associator);
    change_processor_.reset(sync_components.change_processor);
  }

  if (!model_associator_->CryptoReadyIfNecessary()) {
    StartFailed(NEEDS_CRYPTO);
    return;
  }

  bool sync_has_nodes = false;
  if (!model_associator_->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    StartFailed(UNRECOVERABLE_ERROR);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  bool merge_success = model_associator_->AssociateModels();
  UMA_HISTOGRAM_TIMES("Sync.PasswordAssociationTime",
                      base::TimeTicks::Now() - start_time);
  if (!merge_success) {
    StartFailed(ASSOCIATION_FAILED);
    return;
  }

  sync_service_->ActivateDataType(this, change_processor_.get());
  StartDone(!sync_has_nodes ? OK_FIRST_RUN : OK, RUNNING);
}

void PasswordDataTypeController::StartDone(
    DataTypeController::StartResult result,
    DataTypeController::State new_state) {
  abort_association_complete_.Signal();
  base::AutoLock lock(abort_association_lock_);
  if (!abort_association_) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            NewRunnableMethod(
                                this,
                                &PasswordDataTypeController::StartDoneImpl,
                                result,
                                new_state));
  }
}

void PasswordDataTypeController::StartDoneImpl(
    DataTypeController::StartResult result,
    DataTypeController::State new_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  set_state(new_state);
  start_callback_->Run(result, FROM_HERE);
  start_callback_.reset();
}

void PasswordDataTypeController::StopImpl() {
  if (model_associator_ != NULL)
    model_associator_->DisassociateModels();

  change_processor_.reset();
  model_associator_.reset();

  datatype_stopped_.Signal();
}

void PasswordDataTypeController::StartFailed(StartResult result) {
  change_processor_.reset();
  model_associator_.reset();
  StartDone(result, NOT_RUNNING);
}

void PasswordDataTypeController::OnUnrecoverableError(
    const tracked_objects::Location& from_here, const std::string& message) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &PasswordDataTypeController::OnUnrecoverableErrorImpl,
                        from_here, message));
}

void PasswordDataTypeController::OnUnrecoverableErrorImpl(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_service_->OnUnrecoverableError(from_here, message);
}

}  // namespace browser_sync
