// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/offline_page_suggestions_provider_factory.h"

#include "base/feature_list.h"
#include "base/memory/singleton.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/offline_pages/offline_page_suggestions_provider.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::OfflinePageSuggestionsProvider;
using offline_pages::OfflinePageModel;
using offline_pages::OfflinePageModelFactory;

// static
OfflinePageSuggestionsProviderFactory*
OfflinePageSuggestionsProviderFactory::GetInstance() {
  return base::Singleton<OfflinePageSuggestionsProviderFactory>::get();
}

// static
ntp_snippets::OfflinePageSuggestionsProvider*
OfflinePageSuggestionsProviderFactory::GetForProfile(Profile* profile) {
  DCHECK(!profile->IsOffTheRecord());
  return static_cast<ntp_snippets::OfflinePageSuggestionsProvider*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

OfflinePageSuggestionsProviderFactory::OfflinePageSuggestionsProviderFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflinePageSuggestionsProvider",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ContentSuggestionsServiceFactory::GetInstance());
  DependsOn(OfflinePageModelFactory::GetInstance());
}

OfflinePageSuggestionsProviderFactory::
    ~OfflinePageSuggestionsProviderFactory() {}

KeyedService* OfflinePageSuggestionsProviderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  ContentSuggestionsService* content_suggestions_service =
      ContentSuggestionsServiceFactory::GetForProfile(profile);
  if (content_suggestions_service->state() ==
      ContentSuggestionsService::State::DISABLED) {
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(
          chrome::android::kNTPOfflinePageSuggestionsFeature)) {
    return nullptr;
  }

  OfflinePageModel* offline_page_model =
      OfflinePageModelFactory::GetForBrowserContext(profile);

  OfflinePageSuggestionsProvider* offline_page_suggestions_provider =
      new OfflinePageSuggestionsProvider(offline_page_model);
  content_suggestions_service->RegisterProvider(
      offline_page_suggestions_provider);
  return offline_page_suggestions_provider;
}
