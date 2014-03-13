// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_files_service_factory.h"

#include "apps/saved_files_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace apps {

// static
SavedFilesService* SavedFilesServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<SavedFilesService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SavedFilesServiceFactory* SavedFilesServiceFactory::GetInstance() {
  return Singleton<SavedFilesServiceFactory>::get();
}

SavedFilesServiceFactory::SavedFilesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SavedFilesService",
          BrowserContextDependencyManager::GetInstance()) {}

SavedFilesServiceFactory::~SavedFilesServiceFactory() {}

KeyedService* SavedFilesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SavedFilesService(static_cast<Profile*>(profile));
}

}  // namespace apps
