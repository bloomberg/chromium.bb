// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/bookmark_change_processor.h"
#include "chrome/browser/sync/glue/bookmark_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "content/browser/browser_thread.h"

namespace browser_sync {

BookmarkDataTypeController::BookmarkDataTypeController(
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

BookmarkDataTypeController::~BookmarkDataTypeController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void BookmarkDataTypeController::Start(StartCallback* start_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(start_callback);
  if (state_ != NOT_RUNNING) {
    start_callback->Run(BUSY);
    delete start_callback;
    return;
  }

  start_callback_.reset(start_callback);

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If Stop() is called while Start() is waiting for the bookmark
  // model to load, abort the start.
  if (state_ == MODEL_STARTING)
    FinishStart(ABORTED);

  registrar_.RemoveAll();
  if (change_processor_ != NULL)
    sync_service_->DeactivateDataType(this, change_processor_.get());

  if (model_associator_ != NULL)
    model_associator_->DisassociateModels();

  change_processor_.reset();
  model_associator_.reset();

  state_ = NOT_RUNNING;
}

bool BookmarkDataTypeController::enabled() {
  return true;
}

syncable::ModelType BookmarkDataTypeController::type() {
  return syncable::BOOKMARKS;
}

browser_sync::ModelSafeGroup BookmarkDataTypeController::model_safe_group() {
  return browser_sync::GROUP_UI;
}

const char* BookmarkDataTypeController::name() const {
  // For logging only.
  return "bookmark";
}

DataTypeController::State BookmarkDataTypeController::state() {
  return state_;
}

void BookmarkDataTypeController::OnUnrecoverableError(
    const tracked_objects::Location& from_here, const std::string& message) {
  // The ProfileSyncService will invoke our Stop() method in response to this.
  UMA_HISTOGRAM_COUNTS("Sync.BookmarkRunFailures", 1);
  sync_service_->OnUnrecoverableError(from_here, message);
}

void BookmarkDataTypeController::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(NotificationType::BOOKMARK_MODEL_LOADED, type.value);
  registrar_.RemoveAll();
  Associate();
}

void BookmarkDataTypeController::Associate() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state_, MODEL_STARTING);
  state_ = ASSOCIATING;

  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateBookmarkSyncComponents(sync_service_, this);
  model_associator_.reset(sync_components.model_associator);
  change_processor_.reset(sync_components.change_processor);

  bool sync_has_nodes = false;
  if (!model_associator_->SyncModelHasUserCreatedNodes(&sync_has_nodes)) {
    StartFailed(UNRECOVERABLE_ERROR);
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  start_callback_->Run(result);
  start_callback_.reset();
}

void BookmarkDataTypeController::StartFailed(StartResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  model_associator_.reset();
  change_processor_.reset();
  state_ = NOT_RUNNING;
  start_callback_->Run(result);
  start_callback_.reset();
  UMA_HISTOGRAM_ENUMERATION("Sync.BookmarkStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namespace browser_sync
