// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_factory.h"

#include "base/command_line.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/memory/singleton.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/startup_task_runner_service.h"
#include "chrome/browser/profiles/startup_task_runner_service_factory.h"
#include "chrome/browser/undo/bookmark_undo_service.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"

// static
BookmarkModel* BookmarkModelFactory::GetForProfile(Profile* profile) {
  return static_cast<BookmarkModel*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BookmarkModel* BookmarkModelFactory::GetForProfileIfExists(Profile* profile) {
  return static_cast<BookmarkModel*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
BookmarkModelFactory* BookmarkModelFactory::GetInstance() {
  return Singleton<BookmarkModelFactory>::get();
}

BookmarkModelFactory::BookmarkModelFactory()
    : BrowserContextKeyedServiceFactory(
        "BookmarkModel",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ChromeBookmarkClientFactory::GetInstance());
  DependsOn(StartupTaskRunnerServiceFactory::GetInstance());
}

BookmarkModelFactory::~BookmarkModelFactory() {
}

KeyedService* BookmarkModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  ChromeBookmarkClient* bookmark_client =
      ChromeBookmarkClientFactory::GetForProfile(profile);
  BookmarkModel* bookmark_model = new BookmarkModel(bookmark_client);
  bookmark_client->Init(bookmark_model);
  bookmark_model->Load(profile->GetPrefs(),
                       profile->GetPrefs()->GetString(prefs::kAcceptLanguages),
                       profile->GetPath(),
                       StartupTaskRunnerServiceFactory::GetForProfile(profile)
                           ->GetBookmarkTaskRunner(),
                       content::BrowserThread::GetMessageLoopProxyForThread(
                           content::BrowserThread::UI));
  bool register_bookmark_undo_service_as_observer = true;
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  register_bookmark_undo_service_as_observer =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBookmarkUndo);
#endif  // !defined(OS_IOS)
  if (register_bookmark_undo_service_as_observer) {
    bookmark_model->AddObserver(
        BookmarkUndoServiceFactory::GetForProfile(profile));
  }
  return bookmark_model;
}

void BookmarkModelFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Don't sync this, as otherwise, due to a limitation in sync, it
  // will cause a deadlock (see http://crbug.com/97955).  If we truly
  // want to sync the expanded state of folders, it should be part of
  // bookmark sync itself (i.e., a property of the sync folder nodes).
  registry->RegisterListPref(bookmarks::prefs::kBookmarkEditorExpandedNodes,
                             new base::ListValue,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      bookmarks::prefs::kManagedBookmarks,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

content::BrowserContext* BookmarkModelFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool BookmarkModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
