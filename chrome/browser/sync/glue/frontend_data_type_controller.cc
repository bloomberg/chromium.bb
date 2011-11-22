// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/frontend_data_type_controller.h"

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

FrontendDataTypeController::FrontendDataTypeController()
    : profile_sync_factory_(NULL),
      profile_(NULL),
      sync_service_(NULL),
      state_(NOT_RUNNING) {
}

FrontendDataTypeController::FrontendDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
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
    start_callback->Run(BUSY, SyncError());
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
    DCHECK(state_ == DISABLED || state_ == NOT_RUNNING);
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
    StartFailed(NEEDS_CRYPTO, SyncError());
    return false;
  }

  bool sync_has_nodes = false;
  if (!model_associator()->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    SyncError error(FROM_HERE, "Failed to load sync nodes", type());
    StartFailed(UNRECOVERABLE_ERROR, error);
    return false;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  SyncError error;
  bool merge_success = model_associator()->AssociateModels(&error);
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (!merge_success) {
    StartFailed(ASSOCIATION_FAILED, error);
    return false;
  }

  sync_service_->ActivateDataType(type(), model_safe_group(),
                                  change_processor());
  state_ = RUNNING;
  // FinishStart() invokes the DataTypeManager callback, which can lead to a
  // call to Stop() if one of the other data types being started generates an
  // error.
  FinishStart(!sync_has_nodes ? OK_FIRST_RUN : OK);
  // Return false if we're not in the RUNNING state (due to Stop() being called
  // from FinishStart()).
  // TODO(zea/atwilson): Should we maybe move the call to FinishStart() out of
  // Associate() and into Start(), so we don't need this logic here? It seems
  // cleaner to call FinishStart() from Start().
  return state_ == RUNNING;
}

void FrontendDataTypeController::StartFailed(StartResult result,
                                             const SyncError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CleanUpState();
  set_model_associator(NULL);
  change_processor_.reset();
  if (result == ASSOCIATION_FAILED) {
    state_ = DISABLED;
  } else {
    state_ = NOT_RUNNING;
  }
  RecordStartFailure(result);

  // We have to release the callback before we call it, since it's possible
  // invoking the callback will trigger a call to STOP(), which will get
  // confused by the non-NULL start_callback_.
  scoped_ptr<StartCallback> callback(start_callback_.release());
  callback->Run(result, error);
}

void FrontendDataTypeController::FinishStart(StartResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We have to release the callback before we call it, since it's possible
  // invoking the callback will trigger a call to STOP(), which will get
  // confused by the non-NULL start_callback_.
  scoped_ptr<StartCallback> callback(start_callback_.release());
  callback->Run(result, SyncError());
}

void FrontendDataTypeController::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If Stop() is called while Start() is waiting for the datatype model to
  // load, abort the start.
  if (state_ == MODEL_STARTING) {
    StartFailed(ABORTED, SyncError());
    // We can just return here since we haven't performed association if we're
    // still in MODEL_STARTING.
    return;
  }
  DCHECK(!start_callback_.get());

  CleanUpState();

  sync_service_->DeactivateDataType(type());

  if (model_associator()) {
    SyncError error;  // Not used.
    model_associator()->DisassociateModels(&error);
  }

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

  // We dont know the current state of the caller. Posting a task will allow
  // the caller to unwind the stack before we process unrecoverable error.
  MessageLoop::current()->PostTask(from_here,
      base::Bind(&ProfileSyncService::OnUnrecoverableError,
                 sync_service_->AsWeakPtr(),
                 from_here,
                 message));
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
