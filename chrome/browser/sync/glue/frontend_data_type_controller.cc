// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/frontend_data_type_controller.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/change_processor.h"
#include "chrome/browser/sync/glue/model_associator.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "content/browser/browser_thread.h"

namespace browser_sync {

FrontendDataTypeController::FrontendDataTypeController()
    : profile_sync_factory_(NULL),
      profile_(NULL),
      sync_service_(NULL) {}

FrontendDataTypeController::FrontendDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : profile_sync_factory_(profile_sync_factory),
      profile_(profile),
      sync_service_(sync_service),
      state_(NOT_RUNNING) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_sync_factory);
  DCHECK(profile);
  DCHECK(sync_service);
}

FrontendDataTypeController::~FrontendDataTypeController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FrontendDataTypeController::Start(StartCallback* start_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(start_callback);
  if (state_ != NOT_RUNNING) {
    start_callback->Run(BUSY, FROM_HERE);
    delete start_callback;
    return;
  }

  start_callback_.reset(start_callback);

  state_ = MODEL_STARTING;
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early. state_ will control
    // what we perform next.
    DCHECK(state_ == NOT_RUNNING || state_ == MODEL_STARTING);
    return;
  }

  state_ = ASSOCIATING;
  if (!Associate()) {
    // We failed to associate and are aborting.
    DCHECK_EQ(state_, NOT_RUNNING);
    return;
  }
  DCHECK_EQ(state_, RUNNING);
}

bool FrontendDataTypeController::StartModels() {
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association.
  return true;
}

bool FrontendDataTypeController::Associate() {
  DCHECK_EQ(state_, ASSOCIATING);
  CreateSyncComponents();

  if (!model_associator()->CryptoReadyIfNecessary()) {
    StartFailed(NEEDS_CRYPTO, FROM_HERE);
    return false;
  }

  bool sync_has_nodes = false;
  if (!model_associator()->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    StartFailed(UNRECOVERABLE_ERROR, FROM_HERE);
    return false;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  bool merge_success = model_associator()->AssociateModels();
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (!merge_success) {
    StartFailed(ASSOCIATION_FAILED, FROM_HERE);
    return false;
  }

  sync_service_->ActivateDataType(this, change_processor_.get());
  state_ = RUNNING;
  FinishStart(!sync_has_nodes ? OK_FIRST_RUN : OK, FROM_HERE);
  return true;
}

void FrontendDataTypeController::StartFailed(StartResult result,
    const tracked_objects::Location& location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CleanUpState();
  set_model_associator(NULL);
  change_processor_.reset();
  state_ = NOT_RUNNING;
  RecordStartFailure(result);

  // We have to release the callback before we call it, since it's possible
  // invoking the callback will trigger a call to STOP(), which will get
  // confused by the non-NULL start_callback_.
  scoped_ptr<StartCallback> callback(start_callback_.release());
  callback->Run(result, location);
}

void FrontendDataTypeController::FinishStart(StartResult result,
    const tracked_objects::Location& location) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We have to release the callback before we call it, since it's possible
  // invoking the callback will trigger a call to STOP(), which will get
  // confused by the non-NULL start_callback_.
  scoped_ptr<StartCallback> callback(start_callback_.release());
  callback->Run(result, location);
}

void FrontendDataTypeController::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If Stop() is called while Start() is waiting for the datatype model to
  // load, abort the start.
  if (state_ == MODEL_STARTING) {
    StartFailed(ABORTED, FROM_HERE);
    // We can just return here since we haven't performed association if we're
    // still in MODEL_STARTING.
    return;
  }
  DCHECK(!start_callback_.get());

  CleanUpState();

  if (change_processor_.get())
    sync_service_->DeactivateDataType(this, change_processor_.get());

  if (model_associator())
    model_associator()->DisassociateModels();

  set_model_associator(NULL);
  change_processor_.reset();

  state_ = NOT_RUNNING;
}

void FrontendDataTypeController::CleanUpState() {
  // Do nothing by default.
}

browser_sync::ModelSafeGroup FrontendDataTypeController::model_safe_group()
    const {
  return browser_sync::GROUP_UI;
}

std::string FrontendDataTypeController::name() const {
  // For logging only.
  return syncable::ModelTypeToString(type());
}

DataTypeController::State FrontendDataTypeController::state() const {
  return state_;
}

void FrontendDataTypeController::OnUnrecoverableError(
    const tracked_objects::Location& from_here, const std::string& message) {
  // The ProfileSyncService will invoke our Stop() method in response to this.
  RecordUnrecoverableError(from_here, message);
  sync_service_->OnUnrecoverableError(from_here, message);
}

AssociatorInterface* FrontendDataTypeController::model_associator() const {
  return model_associator_.get();
}

void FrontendDataTypeController::set_model_associator(
    AssociatorInterface* model_associator) {
  model_associator_.reset(model_associator);
}

ChangeProcessor* FrontendDataTypeController::change_processor() const {
  return change_processor_.get();
}

void FrontendDataTypeController::set_change_processor(
    ChangeProcessor* change_processor) {
  change_processor_.reset(change_processor);
}

}  // namespace browser_sync
