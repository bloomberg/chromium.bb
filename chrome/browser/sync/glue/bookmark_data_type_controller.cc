// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

using content::BrowserThread;

namespace browser_sync {

BookmarkDataTypeController::BookmarkDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : FrontendDataTypeController(profile_sync_factory,
                                 profile,
                                 sync_service) {
}

BookmarkDataTypeController::~BookmarkDataTypeController() {
}

// We want to start the bookmark model before we begin associating.
bool BookmarkDataTypeController::StartModels() {
  // If the bookmarks model is loaded, continue with association.
  BookmarkModel* bookmark_model = profile_->GetBookmarkModel();
  if (bookmark_model && bookmark_model->IsLoaded()) {
    return true;  // Continue to Associate().
  }

  // Add an observer and continue when the bookmarks model is loaded.
  registrar_.Add(this, chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED,
                 content::Source<Profile>(sync_service_->profile()));
  return false;  // Don't continue Start.
}

// Cleanup for our extra registrar usage.
void BookmarkDataTypeController::CleanUpState() {
  registrar_.RemoveAll();
}

void BookmarkDataTypeController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED, type);
  registrar_.RemoveAll();
  DCHECK_EQ(state_, MODEL_STARTING);
  state_ = ASSOCIATING;
  Associate();
}

syncable::ModelType BookmarkDataTypeController::type() const {
  return syncable::BOOKMARKS;
}

void BookmarkDataTypeController::CreateSyncComponents() {
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateBookmarkSyncComponents(sync_service_,
                                                          this);
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

void BookmarkDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  UMA_HISTOGRAM_COUNTS("Sync.BookmarkRunFailures", 1);
}

void BookmarkDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  UMA_HISTOGRAM_TIMES("Sync.BookmarkAssociationTime", time);
}

void BookmarkDataTypeController::RecordStartFailure(StartResult result) {
  UMA_HISTOGRAM_ENUMERATION("Sync.BookmarkStartFailures",
                            result,
                            MAX_START_RESULT);
}

}  // namespace browser_sync
