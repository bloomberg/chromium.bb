// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/undo/bookmark_undo_service_factory.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/undo/bookmark_undo_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
BookmarkUndoService* BookmarkUndoServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BookmarkUndoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BookmarkUndoServiceFactory* BookmarkUndoServiceFactory::GetInstance() {
  return Singleton<BookmarkUndoServiceFactory>::get();
}

BookmarkUndoServiceFactory::BookmarkUndoServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "BookmarkUndoService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(BookmarkModelFactory::GetInstance());
}

BookmarkUndoServiceFactory::~BookmarkUndoServiceFactory() {
}

KeyedService* BookmarkUndoServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new BookmarkUndoService(profile);
}
