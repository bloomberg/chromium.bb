// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/histogram.h"
#include "base/logging.h"
#include "base/time.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/glue/preference_change_processor.h"
#include "chrome/browser/sync/glue/preference_data_type_controller.h"
#include "chrome/browser/sync/glue/preference_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/unrecoverable_error_handler.h"

namespace browser_sync {

PreferenceDataTypeController::PreferenceDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    ProfileSyncService* sync_service)
    : profile_sync_factory_(profile_sync_factory),
      sync_service_(sync_service),
      state_(NOT_RUNNING),
      unrecoverable_error_detected_(false) {
  DCHECK(profile_sync_factory);
  DCHECK(sync_service);
}

PreferenceDataTypeController::~PreferenceDataTypeController() {
}

void PreferenceDataTypeController::Start(bool merge_allowed,
                                         StartCallback* start_callback) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(start_callback);
  unrecoverable_error_detected_ = false;
  if (state_ != NOT_RUNNING) {
    start_callback->Run(BUSY);
    delete start_callback;
    return;
  }

  start_callback_.reset(start_callback);

  // No additional services need to be started before we can proceed
  // with model association.
  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory_->CreatePreferenceSyncComponents(sync_service_,
                                                            this);
  model_associator_.reset(sync_components.model_associator);
  change_processor_.reset(sync_components.change_processor);

  bool chrome_has_nodes = false;
  if (!model_associator_->ChromeModelHasUserCreatedNodes(&chrome_has_nodes)) {
    StartFailed(UNRECOVERABLE_ERROR);
    return;
  }
  bool sync_has_nodes = false;
  if (!model_associator_->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    StartFailed(UNRECOVERABLE_ERROR);
    return;
  }

  if (chrome_has_nodes && sync_has_nodes && !merge_allowed) {
    StartFailed(NEEDS_MERGE);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  bool merge_success = model_associator_->AssociateModels();
  UMA_HISTOGRAM_TIMES("Sync.PreferenceAssociationTime",
                      base::TimeTicks::Now() - start_time);
  if (!merge_success) {
    StartFailed(ASSOCIATION_FAILED);
    return;
  }

  sync_service_->ActivateDataType(this, change_processor_.get());
  state_ = RUNNING;
  FinishStart(!sync_has_nodes ? OK_FIRST_RUN : OK);
}

void PreferenceDataTypeController::Stop() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (unrecoverable_error_detected_) {
    FinishStart(UNRECOVERABLE_ERROR);
  }

  if (change_processor_ != NULL)
    sync_service_->DeactivateDataType(this, change_processor_.get());

  if (model_associator_ != NULL)
    model_associator_->DisassociateModels();

  change_processor_.reset();
  model_associator_.reset();

  state_ = NOT_RUNNING;
}

void PreferenceDataTypeController::OnUnrecoverableError() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  unrecoverable_error_detected_ = true;
  sync_service_->OnUnrecoverableError();
}

void PreferenceDataTypeController::FinishStart(StartResult result) {
  start_callback_->Run(result);
  start_callback_.reset();
}

void PreferenceDataTypeController::StartFailed(StartResult result) {
  model_associator_.reset();
  change_processor_.reset();
  start_callback_->Run(result);
  start_callback_.reset();
}

}  // namespace browser_sync
