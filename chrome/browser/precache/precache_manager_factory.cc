// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/precache/precache_manager_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/precache/content/precache_manager.h"
#include "content/public/browser/browser_context.h"

namespace precache {

// static
PrecacheManager* PrecacheManagerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<PrecacheManager*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
PrecacheManagerFactory* PrecacheManagerFactory::GetInstance() {
  return base::Singleton<PrecacheManagerFactory>::get();
}

PrecacheManagerFactory::PrecacheManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "PrecacheManager",
          BrowserContextDependencyManager::GetInstance()) {
}

PrecacheManagerFactory::~PrecacheManagerFactory() {
}

KeyedService* PrecacheManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new PrecacheManager(
      browser_context,
      ProfileSyncServiceFactory::GetSyncServiceForBrowserContext(
          browser_context),
      HistoryServiceFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context),
          ServiceAccessType::IMPLICIT_ACCESS));
}

}  // namespace precache
