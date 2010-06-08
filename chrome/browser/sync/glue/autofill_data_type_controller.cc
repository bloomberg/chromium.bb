// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/histogram.h"
#include "base/logging.h"
#include "base/task.h"
#include "base/time.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/autofill_change_processor.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/autofill_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/common/notification_service.h"

namespace browser_sync {

AutofillDataTypeController::AutofillDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : profile_sync_factory_(profile_sync_factory),
      profile_(profile),
      sync_service_(sync_service),
      state_(NOT_RUNNING),
      personal_data_(NULL),
      abort_association_(false),
      abort_association_complete_(false, false) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(profile_sync_factory);
  DCHECK(profile);
  DCHECK(sync_service);
}

AutofillDataTypeController::~AutofillDataTypeController() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

void AutofillDataTypeController::Start(StartCallback* start_callback) {
  LOG(INFO) << "Starting autofill data controller.";
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(start_callback);
  if (state() != NOT_RUNNING) {
    start_callback->Run(BUSY);
    delete start_callback;
    return;
  }

  start_callback_.reset(start_callback);
  abort_association_ = false;

  // Waiting for the personal data is subtle:  we do this as the PDM resets
  // its cache of unique IDs once it gets loaded. If we were to proceed with
  // association, the local ids in the mappings would wind up colliding.
  personal_data_ = profile_->GetPersonalDataManager();
  if (!personal_data_->IsDataLoaded()) {
    set_state(MODEL_STARTING);
    personal_data_->SetObserver(this);
    return;
  }

  ContinueStartAfterPersonalDataLoaded();
}

void AutofillDataTypeController::ContinueStartAfterPersonalDataLoaded() {
  web_data_service_ = profile_->GetWebDataService(Profile::IMPLICIT_ACCESS);
  if (web_data_service_.get() && web_data_service_->IsDatabaseLoaded()) {
    set_state(ASSOCIATING);
    ChromeThread::PostTask(ChromeThread::DB, FROM_HERE,
                           NewRunnableMethod(
                               this,
                               &AutofillDataTypeController::StartImpl));
  } else {
    set_state(MODEL_STARTING);
    notification_registrar_.Add(this, NotificationType::WEB_DATABASE_LOADED,
                                NotificationService::AllSources());
  }
}

void AutofillDataTypeController::OnPersonalDataLoaded() {
  DCHECK_EQ(state_, MODEL_STARTING);
  personal_data_->RemoveObserver(this);
  ContinueStartAfterPersonalDataLoaded();
}

void AutofillDataTypeController::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  LOG(INFO) << "Web database loaded observed.";
  notification_registrar_.RemoveAll();
  set_state(ASSOCIATING);
  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE,
                         NewRunnableMethod(
                             this,
                             &AutofillDataTypeController::StartImpl));
}

void AutofillDataTypeController::Stop() {
  LOG(INFO) << "Stopping autofill data type controller.";
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // If Stop() is called while Start() is waiting for association to
  // complete, we need to abort the association and wait for the DB
  // thread to finish the StartImpl() task.
  if (state_ == ASSOCIATING) {
    {
      AutoLock lock(abort_association_lock_);
      abort_association_ = true;
      if (model_associator_.get())
        model_associator_->AbortAssociation();
    }
    // Wait for the model association to abort.
    abort_association_complete_.Wait();
    StartDoneImpl(ABORTED, STOPPING);
  }

  // If Stop() is called while Start() is waiting for the personal
  // data manager or web data service to load, abort the start.
  if (state_ == MODEL_STARTING)
    StartDoneImpl(ABORTED, STOPPING);

  // Note that we are doing most of the stop work here (deactivate and
  // disassociate) on the UI thread even though the associate &
  // activate were done on the DB thread.  This is because Stop() must
  // be synchronous.
  notification_registrar_.RemoveAll();
  personal_data_->RemoveObserver(this);
  if (change_processor_ != NULL && change_processor_->IsRunning())
    sync_service_->DeactivateDataType(this, change_processor_.get());

  if (model_associator_ != NULL)
    model_associator_->DisassociateModels();

  set_state(NOT_RUNNING);
  ChromeThread::PostTask(ChromeThread::DB, FROM_HERE,
                         NewRunnableMethod(
                             this,
                             &AutofillDataTypeController::StopImpl));
}

void AutofillDataTypeController::StartImpl() {
  LOG(INFO) << "Autofill data type controller StartImpl called.";
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  // No additional services need to be started before we can proceed
  // with model association.
  {
    AutoLock lock(abort_association_lock_);
    if (abort_association_) {
      abort_association_complete_.Signal();
      return;
    }
    ProfileSyncFactory::SyncComponents sync_components =
        profile_sync_factory_->CreateAutofillSyncComponents(
            sync_service_,
            web_data_service_->GetDatabase(),
            profile_->GetPersonalDataManager(),
            this);
    model_associator_.reset(sync_components.model_associator);
    change_processor_.reset(sync_components.change_processor);
  }

  bool sync_has_nodes = false;
  if (!model_associator_->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    StartFailed(UNRECOVERABLE_ERROR);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  bool merge_success = model_associator_->AssociateModels();
  UMA_HISTOGRAM_TIMES("Sync.AutofillAssociationTime",
                      base::TimeTicks::Now() - start_time);
  if (!merge_success) {
    StartFailed(ASSOCIATION_FAILED);
    return;
  }

  sync_service_->ActivateDataType(this, change_processor_.get());
  StartDone(!sync_has_nodes ? OK_FIRST_RUN : OK, RUNNING);
}

void AutofillDataTypeController::StartDone(
    DataTypeController::StartResult result,
    DataTypeController::State new_state) {
  LOG(INFO) << "Autofill data type controller StartDone called.";
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));

  abort_association_complete_.Signal();
  AutoLock lock(abort_association_lock_);
  if (!abort_association_) {
    ChromeThread::PostTask(ChromeThread::UI, FROM_HERE,
                           NewRunnableMethod(
                               this,
                               &AutofillDataTypeController::StartDoneImpl,
                               result,
                               new_state));
  }
}

void AutofillDataTypeController::StartDoneImpl(
    DataTypeController::StartResult result,
    DataTypeController::State new_state) {
  LOG(INFO) << "Autofill data type controller StartDoneImpl called.";
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  set_state(new_state);
  start_callback_->Run(result);
  start_callback_.reset();

  if (result == UNRECOVERABLE_ERROR || result == ASSOCIATION_FAILED) {
    UMA_HISTOGRAM_ENUMERATION("Sync.AutofillStartFailures",
                              result,
                              MAX_START_RESULT);
  }
}

void AutofillDataTypeController::StopImpl() {
  LOG(INFO) << "Autofill data type controller StopImpl called.";
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));

  change_processor_.reset();
  model_associator_.reset();
}

void AutofillDataTypeController::StartFailed(StartResult result) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  change_processor_.reset();
  model_associator_.reset();
  StartDone(result, NOT_RUNNING);
}

void AutofillDataTypeController::OnUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::DB));
  ChromeThread::PostTask(
    ChromeThread::UI, FROM_HERE,
    NewRunnableMethod(this,
                      &AutofillDataTypeController::OnUnrecoverableErrorImpl,
                      from_here, message));
  UMA_HISTOGRAM_COUNTS("Sync.AutofillRunFailures", 1);
}

void AutofillDataTypeController::OnUnrecoverableErrorImpl(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  sync_service_->OnUnrecoverableError(from_here, message);
}

}  // namespace browser_sync
