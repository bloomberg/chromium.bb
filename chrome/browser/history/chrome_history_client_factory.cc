// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/chrome_history_client_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/chrome_history_client.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
ChromeHistoryClient* ChromeHistoryClientFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ChromeHistoryClient*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ChromeHistoryClientFactory* ChromeHistoryClientFactory::GetInstance() {
  return Singleton<ChromeHistoryClientFactory>::get();
}

ChromeHistoryClientFactory::ChromeHistoryClientFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeHistoryClient",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(BookmarkModelFactory::GetInstance());
}

ChromeHistoryClientFactory::~ChromeHistoryClientFactory() {
}

KeyedService* ChromeHistoryClientFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new ChromeHistoryClient(BookmarkModelFactory::GetForProfile(profile),
                                 profile,
                                 profile->GetTopSites());
}

content::BrowserContext* ChromeHistoryClientFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool ChromeHistoryClientFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
