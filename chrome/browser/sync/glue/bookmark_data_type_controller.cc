// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
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
                                 sync_service),
      bookmark_model_(NULL),
      installed_bookmark_observer_(false) {
}

syncer::ModelType BookmarkDataTypeController::type() const {
  return syncer::BOOKMARKS;
}

void BookmarkDataTypeController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state_, MODEL_STARTING);
  DCHECK_EQ(chrome::NOTIFICATION_HISTORY_LOADED, type);

  if (!DependentsLoaded())
    return;

  bookmark_model_->RemoveObserver(this);
  installed_bookmark_observer_ = false;

  registrar_.RemoveAll();
  OnModelLoaded();
}

BookmarkDataTypeController::~BookmarkDataTypeController() {
  if (installed_bookmark_observer_ && bookmark_model_) {
    DCHECK(profile_);
    bookmark_model_->RemoveObserver(this);
  }
}

bool BookmarkDataTypeController::StartModels() {
  bookmark_model_ = BookmarkModelFactory::GetForProfile(profile_);
  if (!DependentsLoaded()) {
    bookmark_model_->AddObserver(this);
    installed_bookmark_observer_ = true;

    registrar_.Add(this, chrome::NOTIFICATION_HISTORY_LOADED,
                   content::Source<Profile>(sync_service_->profile()));
    return false;
  }
  return true;
}

// Cleanup for our extra registrar usage.
void BookmarkDataTypeController::CleanUpState() {
  registrar_.RemoveAll();
}

void BookmarkDataTypeController::CreateSyncComponents() {
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory_->CreateBookmarkSyncComponents(sync_service_,
                                                          this);
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

void BookmarkDataTypeController::BookmarkModelChanged() {
}

void BookmarkDataTypeController::Loaded(BookmarkModel* model,
                                        bool ids_reassigned) {
  DCHECK(model->IsLoaded());
  model->RemoveObserver(this);
  installed_bookmark_observer_ = false;

  if (!DependentsLoaded())
    return;

  registrar_.RemoveAll();
  OnModelLoaded();
}

void BookmarkDataTypeController::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  installed_bookmark_observer_ = false;
}

// Check that both the bookmark model and the history service (for favicons)
// are loaded.
bool BookmarkDataTypeController::DependentsLoaded() {
  if (!bookmark_model_ || !bookmark_model_->IsLoaded())
    return false;

  HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  if (!history || !history->BackendLoaded())
    return false;

  // All necessary services are loaded.
  return true;
}

}  // namespace browser_sync
