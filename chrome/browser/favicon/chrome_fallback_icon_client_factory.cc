// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/chrome_fallback_icon_client_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/favicon/chrome_fallback_icon_client.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

ChromeFallbackIconClientFactory::ChromeFallbackIconClientFactory()
    : BrowserContextKeyedServiceFactory(
        "ChromeFallbackIconClient",
        BrowserContextDependencyManager::GetInstance()) {
}

ChromeFallbackIconClientFactory::~ChromeFallbackIconClientFactory() {
}

// static
favicon::FallbackIconClient*
ChromeFallbackIconClientFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<favicon::FallbackIconClient*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ChromeFallbackIconClientFactory*
ChromeFallbackIconClientFactory::GetInstance() {
  return base::Singleton<ChromeFallbackIconClientFactory>::get();
}

KeyedService* ChromeFallbackIconClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChromeFallbackIconClient();
}
