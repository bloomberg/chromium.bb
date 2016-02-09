// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/fallback_icon_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/favicon/chrome_fallback_icon_client_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/favicon/core/fallback_icon_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

// static
favicon::FallbackIconService* FallbackIconServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<favicon::FallbackIconService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
FallbackIconServiceFactory* FallbackIconServiceFactory::GetInstance() {
  return base::Singleton<FallbackIconServiceFactory>::get();
}

FallbackIconServiceFactory::FallbackIconServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "FallbackIconService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ChromeFallbackIconClientFactory::GetInstance());
}

FallbackIconServiceFactory::~FallbackIconServiceFactory() {}

content::BrowserContext* FallbackIconServiceFactory::GetBrowserContextToUse(
      content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* FallbackIconServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  favicon::FallbackIconClient* fallback_icon_client =
      ChromeFallbackIconClientFactory::GetForBrowserContext(context);
  return new favicon::FallbackIconService(fallback_icon_client);
}

bool FallbackIconServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
