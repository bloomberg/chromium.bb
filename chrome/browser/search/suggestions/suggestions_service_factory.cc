// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_service_factory.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/suggestions_service.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"

namespace suggestions {

// static
SuggestionsService* SuggestionsServiceFactory::GetForProfile(Profile* profile) {
  if (!SuggestionsService::IsEnabled())
    return NULL;

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

SuggestionsServiceFactory::~SuggestionsServiceFactory() {
}

content::BrowserContext* SuggestionsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

BrowserContextKeyedService* SuggestionsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new SuggestionsService(static_cast<Profile*>(profile));
}

}  // namespace suggestions
