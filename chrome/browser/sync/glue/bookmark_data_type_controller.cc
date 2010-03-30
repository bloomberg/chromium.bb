// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/histogram.h"
#include "base/logging.h"
#include "base/time.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/bookmark_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"

namespace browser_sync {

BookmarkDataTypeController::BookmarkDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : profile_sync_factory_(profile_sync_factory),
      profile_(profile),
      sync_service_(sync_service),
      state_(NOT_RUNNING),
      merge_allowed_(false),
      unrecoverable_error_detected_(false) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(profile_sync_factory);
  DCHECK(profile);
  DCHECK(sync_service);
}

BookmarkDataTypeController::~BookmarkDataTypeController() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

void BookmarkDataTypeController::Start(bool merge_allowed,
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
  merge_allowed_ = merge_allowed;

  if (!enabled()) {
    FinishStart(NOT_ENABLED);
    return;
  }

  state_ = MODEL_STARTING;

  // If the bookmarks model is loaded, continue with association.
  BookmarkModel* bookmark_model = profile_->GetBookmarkModel();
  if (bookmark_model && bookmark_model->IsLoaded()) {
    Associate();
    return;
  }

  // Add an observer and continue when the bookmarks model is loaded.
  registrar_.Add(this, NotificationType::BOOKMARK_MODEL_LOADED,
                 Source<Profile>(sync_service_->profile()));
}

void BookmarkDataTypeController::Stop() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  // If Stop() is called while Start() is waiting for the bookmark
  // model to load, abort the start.
  if (unrecoverable_error_detected_) {
    FinishStart(UNRECOVERABLE_ERROR);
  } else if (state_ == MODEL_STARTING) {
    FinishStart(ABORTED);
  }

  registrar_.RemoveAll();
  if (change_processor_ != NULL)
    sync_service_->DeactivateDataType(this, change_processor_.get());

  if (model_associator_ != NULL)
    model_associator_->DisassociateModels();

  change_processor_.reset();
  model_associator_.reset();

  state_ = NOT_RUNNING;
  merge_allowed_ = false;
}

void BookmarkDataTypeController::OnUnrecoverableError() {
  unrecoverable_error_detected_ = true;
  // The ProfileSyncService will invoke our Stop() method in response to this.
  sync_service_->OnUnrecoverableError();
}

void BookmarkDataTypeController::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK_EQ(NotificationType::BOOKMARK_MODEL_LOADED, type.value);
  registrar_.RemoveAll();
  Associate();
}

void BookmarkDataTypeController::Associate() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK_EQ(state_, MODEL_STARTING);
  state_ = ASSOCIATING;

  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateBookmarkSyncComponents(sync_service_, this);
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

  if (chrome_has_nodes && sync_has_nodes && !merge_allowed_) {
    StartFailed(NEEDS_MERGE);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  bool merge_success = model_associator_->AssociateModels();
  UMA_HISTOGRAM_TIMES("Sync.BookmarkAssociationTime",
                      base::TimeTicks::Now() - start_time);
  if (!merge_success) {
    StartFailed(ASSOCIATION_FAILED);
    return;
  }

  sync_service_->ActivateDataType(this, change_processor_.get());
  state_ = RUNNING;
  FinishStart(!sync_has_nodes ? OK_FIRST_RUN : OK);
}

void BookmarkDataTypeController::FinishStart(StartResult result) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  start_callback_->Run(result);
  start_callback_.reset();
}

void BookmarkDataTypeController::StartFailed(StartResult result) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  model_associator_.reset();
  change_processor_.reset();
  state_ = NOT_RUNNING;
  start_callback_->Run(result);
  start_callback_.reset();
}

}  // namespace browser_sync
