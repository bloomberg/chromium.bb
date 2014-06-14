// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/chrome_bookmark_client_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
ChromeBookmarkClient* ChromeBookmarkClientFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ChromeBookmarkClient*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeBookmarkClientFactory* ChromeBookmarkClientFactory::GetInstance() {
  return Singleton<ChromeBookmarkClientFactory>::get();
}

ChromeBookmarkClientFactory::ChromeBookmarkClientFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeBookmarkClient",
          BrowserContextDependencyManager::GetInstance()) {
}

ChromeBookmarkClientFactory::~ChromeBookmarkClientFactory() {
}

KeyedService* ChromeBookmarkClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ChromeBookmarkClient(static_cast<Profile*>(context));
}

content::BrowserContext* ChromeBookmarkClientFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool ChromeBookmarkClientFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
