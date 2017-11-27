// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/saved_files_service_factory.h"

#include "apps/saved_files_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace apps {

// static
SavedFilesService* SavedFilesServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SavedFilesService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SavedFilesService* SavedFilesServiceFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<SavedFilesService*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
SavedFilesServiceFactory* SavedFilesServiceFactory::GetInstance() {
  return base::Singleton<SavedFilesServiceFactory>::get();
}

SavedFilesServiceFactory::SavedFilesServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SavedFilesService",
          BrowserContextDependencyManager::GetInstance()) {}

SavedFilesServiceFactory::~SavedFilesServiceFactory() = default;

KeyedService* SavedFilesServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SavedFilesService(context);
}

}  // namespace apps
