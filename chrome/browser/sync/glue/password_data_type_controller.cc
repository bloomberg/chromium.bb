// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/password_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/password_change_processor.h"
#include "chrome/browser/sync/glue/password_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory.h"

namespace browser_sync {

PasswordDataTypeController::PasswordDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : profile_sync_factory_(profile_sync_factory),
      profile_(profile),
      sync_service_(sync_service),
      state_(NOT_RUNNING) {
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
    start_callback->Run(BUSY);
    delete start_callback;
    return;
  }

  password_store_ = profile_->GetPasswordStore(Profile::EXPLICIT_ACCESS);
  if (!password_store_.get()) {
    LOG(ERROR) << "PasswordStore not initialized, password datatype controller"
        << " aborting.";
    state_ = NOT_RUNNING;
    start_callback->Run(ABORTED);
    delete start_callback;
    return;
  }

  if (!sync_service_->IsCryptographerReady()) {
    start_callback->Run(NEEDS_CRYPTO);
    delete start_callback;
    return;
  }

  start_callback_.reset(start_callback);
  set_state(ASSOCIATING);
  password_store_->ScheduleTask(
      NewRunnableMethod(this, &PasswordDataTypeController::StartImpl));
}

void PasswordDataTypeController::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (change_processor_ != NULL)
    sync_service_->DeactivateDataType(this, change_processor_.get());

  if (model_associator_ != NULL)
    model_associator_->DisassociateModels();

  set_state(NOT_RUNNING);
  DCHECK(password_store_.get());
  password_store_->ScheduleTask(
      NewRunnableMethod(this, &PasswordDataTypeController::StopImpl));
}

bool PasswordDataTypeController::enabled() {
  return true;
}

syncable::ModelType PasswordDataTypeController::type() {
  return syncable::PASSWORDS;
}

browser_sync::ModelSafeGroup PasswordDataTypeController::model_safe_group() {
  return browser_sync::GROUP_PASSWORD;
}

const char* PasswordDataTypeController::name() const {
  // For logging only.
  return "password";
}

DataTypeController::State PasswordDataTypeController::state() {
  return state_;
}

void PasswordDataTypeController::StartImpl() {
  // No additional services need to be started before we can proceed
  // with model association.
  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory_->CreatePasswordSyncComponents(
          sync_service_,
          password_store_.get(),
          this);
  model_associator_.reset(sync_components.model_associator);
  change_processor_.reset(sync_components.change_processor);

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
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          NewRunnableMethod(
                              this,
                              &PasswordDataTypeController::StartDoneImpl,
                              result,
                              new_state));
}

void PasswordDataTypeController::StartDoneImpl(
    DataTypeController::StartResult result,
    DataTypeController::State new_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  set_state(new_state);
  start_callback_->Run(result);
  start_callback_.reset();
}

void PasswordDataTypeController::StopImpl() {
  change_processor_.reset();
  model_associator_.reset();

  state_ = NOT_RUNNING;
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
