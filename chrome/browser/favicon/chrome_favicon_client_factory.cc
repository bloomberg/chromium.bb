// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/chrome_favicon_client_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

ChromeFaviconClientFactory::ChromeFaviconClientFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeFaviconClient",
          BrowserContextDependencyManager::GetInstance()) {
}

ChromeFaviconClientFactory::~ChromeFaviconClientFactory() {
}

// static
FaviconClient* ChromeFaviconClientFactory::GetForProfile(Profile* profile) {
  if (!profile->IsOffTheRecord()) {
    return static_cast<FaviconClient*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }
  // Profile must be OffTheRecord in this case.
  return static_cast<FaviconClient*>(GetInstance()->GetServiceForBrowserContext(
      profile->GetOriginalProfile(), true));
}

// static
ChromeFaviconClientFactory* ChromeFaviconClientFactory::GetInstance() {
  return Singleton<ChromeFaviconClientFactory>::get();
}

KeyedService* ChromeFaviconClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  ChromeFaviconClient* client =
      new ChromeFaviconClient(Profile::FromBrowserContext(context));
  return client;
}

bool ChromeFaviconClientFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
