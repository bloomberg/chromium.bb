// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/suggestions_service.h"
#include "chrome/browser/search/suggestions/suggestions_store.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace suggestions {

// static
SuggestionsService* SuggestionsServiceFactory::GetForProfile(Profile* profile) {
  if (!SuggestionsService::IsEnabled()) return NULL;

  return static_cast<SuggestionsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SuggestionsServiceFactory* SuggestionsServiceFactory::GetInstance() {
  return Singleton<SuggestionsServiceFactory>::get();
}

SuggestionsServiceFactory::SuggestionsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SuggestionsService",
          BrowserContextDependencyManager::GetInstance()) {
  // No dependencies.
}

SuggestionsServiceFactory::~SuggestionsServiceFactory() {}

content::BrowserContext* SuggestionsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* SuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  Profile* the_profile = static_cast<Profile*>(profile);
  scoped_ptr<SuggestionsStore> suggestions_store(
      new SuggestionsStore(the_profile->GetPrefs()));
  scoped_ptr<ThumbnailManager> thumbnail_manager(
      new ThumbnailManager(the_profile->GetRequestContext()));
  return new SuggestionsService(the_profile, suggestions_store.Pass(),
                                thumbnail_manager.Pass());
}

void SuggestionsServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  SuggestionsService::RegisterProfilePrefs(registry);
}

}  // namespace suggestions
