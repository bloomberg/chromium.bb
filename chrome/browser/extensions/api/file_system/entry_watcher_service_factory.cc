// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/entry_watcher_service_factory.h"

#include "chrome/browser/extensions/api/file_system/entry_watcher_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

EntryWatcherServiceFactory* EntryWatcherServiceFactory::GetInstance() {
  return Singleton<EntryWatcherServiceFactory>::get();
}

EntryWatcherServiceFactory::EntryWatcherServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "EntryWatcherService",
          BrowserContextDependencyManager::GetInstance()) {
}

EntryWatcherServiceFactory::~EntryWatcherServiceFactory() {
}

KeyedService* EntryWatcherServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new EntryWatcherService(Profile::FromBrowserContext(context));
}

bool EntryWatcherServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Required to restore persistent watchers as soon as the profile is loaded.
  return true;
}

}  // namespace extensions
