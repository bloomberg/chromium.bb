// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_factory.h"

#include "base/command_line.h"
#include "base/deferred_sequenced_task_runner.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/bookmarks/startup_task_runner_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/startup_task_runner_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/prefs/pref_service.h"
#include "components/undo/bookmark_undo_service.h"
#include "content/public/browser/browser_thread.h"

using bookmarks::BookmarkModel;

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
  return base::Singleton<BookmarkModelFactory>::get();
}

BookmarkModelFactory::BookmarkModelFactory()
    : BrowserContextKeyedServiceFactory(
        "BookmarkModel",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(BookmarkUndoServiceFactory::GetInstance());
  DependsOn(ManagedBookmarkServiceFactory::GetInstance());
  DependsOn(StartupTaskRunnerServiceFactory::GetInstance());
}

BookmarkModelFactory::~BookmarkModelFactory() {
}

KeyedService* BookmarkModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  BookmarkModel* bookmark_model =
      new BookmarkModel(base::WrapUnique(new ChromeBookmarkClient(
          profile, ManagedBookmarkServiceFactory::GetForProfile(profile))));
  bookmark_model->Load(profile->GetPrefs(),
                       profile->GetPath(),
                       StartupTaskRunnerServiceFactory::GetForProfile(profile)
                           ->GetBookmarkTaskRunner(),
                       content::BrowserThread::GetMessageLoopProxyForThread(
                           content::BrowserThread::UI));
  bool register_bookmark_undo_service_as_observer = true;
#if !BUILDFLAG(ANDROID_JAVA_UI)
  register_bookmark_undo_service_as_observer =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBookmarkUndo);
#endif  // !BUILDFLAG(ANDROID_JAVA_UI)
  if (register_bookmark_undo_service_as_observer)
    BookmarkUndoServiceFactory::GetForProfile(profile)->Start(bookmark_model);

  return bookmark_model;
}

void BookmarkModelFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  bookmarks::RegisterProfilePrefs(registry);
}

content::BrowserContext* BookmarkModelFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool BookmarkModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
