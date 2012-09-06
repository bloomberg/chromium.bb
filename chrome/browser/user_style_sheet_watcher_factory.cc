// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/user_style_sheet_watcher_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/user_style_sheet_watcher.h"

// static
scoped_refptr<UserStyleSheetWatcher>
UserStyleSheetWatcherFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UserStyleSheetWatcher*>(
      GetInstance()->GetServiceForProfile(profile, true).get());
}

// static
UserStyleSheetWatcherFactory* UserStyleSheetWatcherFactory::GetInstance() {
  return Singleton<UserStyleSheetWatcherFactory>::get();
}

UserStyleSheetWatcherFactory::UserStyleSheetWatcherFactory()
    : RefcountedProfileKeyedServiceFactory(
          "UserStyleSheetWatcher", ProfileDependencyManager::GetInstance()) {
}

UserStyleSheetWatcherFactory::~UserStyleSheetWatcherFactory() {
}

scoped_refptr<RefcountedProfileKeyedService>
UserStyleSheetWatcherFactory::BuildServiceInstanceFor(Profile* profile) const {
  scoped_refptr<UserStyleSheetWatcher> user_style_sheet_watcher(
      new UserStyleSheetWatcher(profile, profile->GetPath()));
  user_style_sheet_watcher->Init();
  return user_style_sheet_watcher;
}

bool UserStyleSheetWatcherFactory::ServiceRedirectedInIncognito() const {
  return true;
}

bool UserStyleSheetWatcherFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
