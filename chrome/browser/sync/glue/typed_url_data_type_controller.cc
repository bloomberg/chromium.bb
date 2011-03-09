// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/typed_url_change_processor.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"

namespace browser_sync {

class ControlTask : public HistoryDBTask {
 public:
  ControlTask(TypedUrlDataTypeController* controller, bool start)
    : controller_(controller), start_(start) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    if (start_) {
      controller_->StartImpl(backend);
    } else {
      controller_->StopImpl();
    }

    // Release the reference to the controller.  This ensures that
    // the controller isn't held past its lifetime in unit tests.
    controller_ = NULL;
    return true;
  }

  virtual void DoneRunOnMainThread() {}

 protected:
  scoped_refptr<TypedUrlDataTypeController> controller_;
  bool start_;
};

TypedUrlDataTypeController::TypedUrlDataTypeController(
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

TypedUrlDataTypeController::~TypedUrlDataTypeController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void TypedUrlDataTypeController::Start(StartCallback* start_callback) {
  VLOG(1) << "Starting typed_url data controller.";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(start_callback);
  if (state_ != NOT_RUNNING || start_callback_.get()) {
    start_callback->Run(BUSY);
    delete start_callback;
    return;
  }

  start_callback_.reset(start_callback);

  HistoryService* history = profile_->GetHistoryServiceWithoutCreating();
  if (history) {
    set_state(ASSOCIATING);
    history_service_ = history;
    history_service_->ScheduleDBTask(new ControlTask(this, true), this);
  } else {
    set_state(MODEL_STARTING);
    notification_registrar_.Add(this, NotificationType::HISTORY_LOADED,
                                NotificationService::AllSources());
  }
}

void TypedUrlDataTypeController::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  VLOG(1) << "History loaded observed.";
  notification_registrar_.Remove(this,
                                 NotificationType::HISTORY_LOADED,
                                 NotificationService::AllSources());

  history_service_ = profile_->GetHistoryServiceWithoutCreating();
  DCHECK(history_service_.get());
  history_service_->ScheduleDBTask(new ControlTask(this, true), this);
}

void TypedUrlDataTypeController::Stop() {
  VLOG(1) << "Stopping typed_url data type controller.";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (change_processor_ != NULL)
    sync_service_->DeactivateDataType(this, change_processor_.get());

  if (model_associator_ != NULL)
    model_associator_->DisassociateModels();

  set_state(NOT_RUNNING);
  DCHECK(history_service_.get());
  history_service_->ScheduleDBTask(new ControlTask(this, false), this);
}

bool TypedUrlDataTypeController::enabled() {
  return true;
}

syncable::ModelType TypedUrlDataTypeController::type() {
  return syncable::TYPED_URLS;
}

browser_sync::ModelSafeGroup TypedUrlDataTypeController::model_safe_group() {
  return browser_sync::GROUP_HISTORY;
}

const char* TypedUrlDataTypeController::name() const {
  // For logging only.
  return "typed_url";
}

DataTypeController::State TypedUrlDataTypeController::state() {
  return state_;
}

void TypedUrlDataTypeController::StartImpl(history::HistoryBackend* backend) {
  VLOG(1) << "TypedUrl data type controller StartImpl called.";
  // No additional services need to be started before we can proceed
  // with model association.
  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateTypedUrlSyncComponents(
          sync_service_,
          backend,
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
  UMA_HISTOGRAM_TIMES("Sync.TypedUrlAssociationTime",
                      base::TimeTicks::Now() - start_time);
  if (!merge_success) {
    StartFailed(ASSOCIATION_FAILED);
    return;
  }

  sync_service_->ActivateDataType(this, change_processor_.get());
  StartDone(!sync_has_nodes ? OK_FIRST_RUN : OK, RUNNING);
}

void TypedUrlDataTypeController::StartDone(
    DataTypeController::StartResult result,
    DataTypeController::State new_state) {
  VLOG(1) << "TypedUrl data type controller StartDone called.";
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                         NewRunnableMethod(
                             this,
                             &TypedUrlDataTypeController::StartDoneImpl,
                             result,
                             new_state));
}

void TypedUrlDataTypeController::StartDoneImpl(
    DataTypeController::StartResult result,
    DataTypeController::State new_state) {
  VLOG(1) << "TypedUrl data type controller StartDoneImpl called.";
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  set_state(new_state);
  start_callback_->Run(result);
  start_callback_.reset();

  if (result == UNRECOVERABLE_ERROR || result == ASSOCIATION_FAILED) {
    UMA_HISTOGRAM_ENUMERATION("Sync.TypedUrlStartFailures",
                              result,
                              MAX_START_RESULT);
  }
}

void TypedUrlDataTypeController::StopImpl() {
  VLOG(1) << "TypedUrl data type controller StopImpl called.";

  change_processor_.reset();
  model_associator_.reset();

  state_ = NOT_RUNNING;
}

void TypedUrlDataTypeController::StartFailed(StartResult result) {
  change_processor_.reset();
  model_associator_.reset();
  StartDone(result, NOT_RUNNING);
}

void TypedUrlDataTypeController::OnUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  BrowserThread::PostTask(
    BrowserThread::UI, FROM_HERE,
    NewRunnableMethod(this,
                      &TypedUrlDataTypeController::OnUnrecoverableErrorImpl,
                      from_here, message));
}

void TypedUrlDataTypeController::OnUnrecoverableErrorImpl(
  const tracked_objects::Location& from_here,
  const std::string& message) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_COUNTS("Sync.TypedUrlRunFailures", 1);
  sync_service_->OnUnrecoverableError(from_here, message);
}

}  // namespace browser_sync
