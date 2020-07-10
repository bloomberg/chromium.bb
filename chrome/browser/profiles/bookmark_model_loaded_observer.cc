// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"

#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"

using bookmarks::BookmarkModel;

BookmarkModelLoadedObserver::BookmarkModelLoadedObserver(Profile* profile)
    : profile_(profile) {
}

void BookmarkModelLoadedObserver::BookmarkModelChanged() {
}

void BookmarkModelLoadedObserver::BookmarkModelLoaded(BookmarkModel* model,
                                                      bool ids_reassigned) {
  // Causes lazy-load if sync is enabled.
  ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  model->RemoveObserver(this);
  delete this;
}

void BookmarkModelLoadedObserver::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  model->RemoveObserver(this);
  delete this;
}
