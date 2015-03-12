// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/browser/browser_thread.h"

using bookmarks::BookmarkModel;
using content::BrowserThread;

namespace browser_sync {

BookmarkDataTypeController::BookmarkDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : FrontendDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          profile_sync_factory,
          profile,
          sync_service),
      history_service_observer_(this),
      bookmark_model_observer_(this) {
}

syncer::ModelType BookmarkDataTypeController::type() const {
  return syncer::BOOKMARKS;
}

BookmarkDataTypeController::~BookmarkDataTypeController() {
}

bool BookmarkDataTypeController::StartModels() {
  if (!DependentsLoaded()) {
    BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForProfile(profile_);
    bookmark_model_observer_.Add(bookmark_model);
    HistoryService* history_service = HistoryServiceFactory::GetForProfile(
        profile_, ServiceAccessType::EXPLICIT_ACCESS);
    history_service_observer_.Add(history_service);
    return false;
  }
  return true;
}

void BookmarkDataTypeController::CleanUpState() {
  history_service_observer_.RemoveAll();
  bookmark_model_observer_.RemoveAll();
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

void BookmarkDataTypeController::BookmarkModelLoaded(BookmarkModel* model,
                                                     bool ids_reassigned) {
  DCHECK(model->loaded());
  bookmark_model_observer_.RemoveAll();

  if (!DependentsLoaded())
    return;

  history_service_observer_.RemoveAll();
  OnModelLoaded();
}

void BookmarkDataTypeController::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  CleanUpState();
}

// Check that both the bookmark model and the history service (for favicons)
// are loaded.
bool BookmarkDataTypeController::DependentsLoaded() {
  BookmarkModel* bookmark_model = BookmarkModelFactory::GetForProfile(profile_);
  if (!bookmark_model || !bookmark_model->loaded())
    return false;

  HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (!history || !history->BackendLoaded())
    return false;

  // All necessary services are loaded.
  return true;
}

void BookmarkDataTypeController::OnHistoryServiceLoaded(
    HistoryService* service) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state_, MODEL_STARTING);
  history_service_observer_.RemoveAll();

  if (!DependentsLoaded())
    return;

  bookmark_model_observer_.RemoveAll();
  OnModelLoaded();
}

void BookmarkDataTypeController::HistoryServiceBeingDeleted(
    HistoryService* history_service) {
  CleanUpState();
}

}  // namespace browser_sync
