// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/origins_seen_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
OriginsSeenService* OriginsSeenServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<OriginsSeenService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
OriginsSeenServiceFactory* OriginsSeenServiceFactory::GetInstance() {
  return base::Singleton<OriginsSeenServiceFactory>::get();
}

// static
KeyedService* OriginsSeenServiceFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  return new OriginsSeenService();
}

OriginsSeenServiceFactory::OriginsSeenServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "OriginsSeenService",
          BrowserContextDependencyManager::GetInstance()) {}

OriginsSeenServiceFactory::~OriginsSeenServiceFactory() {}

content::BrowserContext* OriginsSeenServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

KeyedService* OriginsSeenServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}
