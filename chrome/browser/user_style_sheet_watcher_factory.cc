// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_style_sheet_watcher_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/user_style_sheet_watcher.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

// static
scoped_refptr<UserStyleSheetWatcher>
UserStyleSheetWatcherFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UserStyleSheetWatcher*>(
      GetInstance()->GetServiceForBrowserContext(profile, true).get());
}

// static
UserStyleSheetWatcherFactory* UserStyleSheetWatcherFactory::GetInstance() {
  return Singleton<UserStyleSheetWatcherFactory>::get();
}

UserStyleSheetWatcherFactory::UserStyleSheetWatcherFactory()
    : RefcountedBrowserContextKeyedServiceFactory(
        "UserStyleSheetWatcher",
        BrowserContextDependencyManager::GetInstance()) {
}

UserStyleSheetWatcherFactory::~UserStyleSheetWatcherFactory() {
}

scoped_refptr<RefcountedBrowserContextKeyedService>
UserStyleSheetWatcherFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  scoped_refptr<UserStyleSheetWatcher> user_style_sheet_watcher(
      new UserStyleSheetWatcher(profile, profile->GetPath()));
  user_style_sheet_watcher->Init();
  return user_style_sheet_watcher;
}

content::BrowserContext* UserStyleSheetWatcherFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool UserStyleSheetWatcherFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
