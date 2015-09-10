// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/chrome_bookmark_client_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace {

scoped_ptr<KeyedService> BuildChromeBookmarkClient(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return make_scoped_ptr(new ChromeBookmarkClient(
      profile, ManagedBookmarkServiceFactory::GetForProfile(profile)));
}

}  // namespace

// static
ChromeBookmarkClient* ChromeBookmarkClientFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ChromeBookmarkClient*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeBookmarkClientFactory* ChromeBookmarkClientFactory::GetInstance() {
  return base::Singleton<ChromeBookmarkClientFactory>::get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactoryFunction
ChromeBookmarkClientFactory::GetDefaultFactory() {
  return &BuildChromeBookmarkClient;
}

ChromeBookmarkClientFactory::ChromeBookmarkClientFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeBookmarkClient",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ManagedBookmarkServiceFactory::GetInstance());
}

ChromeBookmarkClientFactory::~ChromeBookmarkClientFactory() {
}

KeyedService* ChromeBookmarkClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildChromeBookmarkClient(context).release();
}

content::BrowserContext* ChromeBookmarkClientFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool ChromeBookmarkClientFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
