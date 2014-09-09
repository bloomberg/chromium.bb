// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client_factory.h"
#include "chrome/browser/history/chrome_history_client.h"
#include "chrome/browser/history/chrome_history_client_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
HistoryService* HistoryServiceFactory::GetForProfile(
    Profile* profile, Profile::ServiceAccessType sat) {
  // If saving history is disabled, only allow explicit access.
  if (profile->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled) &&
      sat != Profile::EXPLICIT_ACCESS)
    return NULL;

  return static_cast<HistoryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
HistoryService*
HistoryServiceFactory::GetForProfileIfExists(
    Profile* profile, Profile::ServiceAccessType sat) {
  // If saving history is disabled, only allow explicit access.
  if (profile->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled) &&
      sat != Profile::EXPLICIT_ACCESS)
    return NULL;

  return static_cast<HistoryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
HistoryService*
HistoryServiceFactory::GetForProfileWithoutCreating(Profile* profile) {
  return static_cast<HistoryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, false));
}

// static
HistoryServiceFactory* HistoryServiceFactory::GetInstance() {
  return Singleton<HistoryServiceFactory>::get();
}

// static
void HistoryServiceFactory::ShutdownForProfile(Profile* profile) {
  HistoryServiceFactory* factory = GetInstance();
  factory->BrowserContextDestroyed(profile);
}

HistoryServiceFactory::HistoryServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "HistoryService", BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ChromeHistoryClientFactory::GetInstance());
  DependsOn(ChromeBookmarkClientFactory::GetInstance());
}

HistoryServiceFactory::~HistoryServiceFactory() {
}

KeyedService* HistoryServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  scoped_ptr<HistoryService> history_service(new HistoryService(
      ChromeHistoryClientFactory::GetForProfile(profile), profile));
  if (!history_service->Init(profile->GetPath()))
    return NULL;
  ChromeBookmarkClientFactory::GetForProfile(profile)
      ->SetHistoryService(history_service.get());
  return history_service.release();
}

content::BrowserContext* HistoryServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool HistoryServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
