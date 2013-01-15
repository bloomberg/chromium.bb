// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/pref_names.h"

// static
BookmarkModel* BookmarkModelFactory::GetForProfile(Profile* profile) {
  return static_cast<BookmarkModel*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

BookmarkModel* BookmarkModelFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<BookmarkModel*>(
      GetInstance()->GetServiceForProfile(profile, false));
}

// static
BookmarkModelFactory* BookmarkModelFactory::GetInstance() {
  return Singleton<BookmarkModelFactory>::get();
}

BookmarkModelFactory::BookmarkModelFactory()
    : ProfileKeyedServiceFactory("BookmarkModel",
                                 ProfileDependencyManager::GetInstance()) {
}

BookmarkModelFactory::~BookmarkModelFactory() {}

ProfileKeyedService* BookmarkModelFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  BookmarkModel* bookmark_model = new BookmarkModel(profile);
  bookmark_model->Load();
  return bookmark_model;
}

void BookmarkModelFactory::RegisterUserPrefs(PrefServiceSyncable* prefs) {
  // Don't sync this, as otherwise, due to a limitation in sync, it
  // will cause a deadlock (see http://crbug.com/97955).  If we truly
  // want to sync the expanded state of folders, it should be part of
  // bookmark sync itself (i.e., a property of the sync folder nodes).
  prefs->RegisterListPref(prefs::kBookmarkEditorExpandedNodes, new ListValue,
                          PrefServiceSyncable::UNSYNCABLE_PREF);
}

bool BookmarkModelFactory::ServiceRedirectedInIncognito() const {
  return true;
}

bool BookmarkModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

// static
BookmarkService* BookmarkService::FromBrowserContext(
    content::BrowserContext* browser_context) {
  return BookmarkModelFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}
