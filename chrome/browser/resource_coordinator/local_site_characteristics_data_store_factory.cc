// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_non_recording_data_store.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace resource_coordinator {

// static
SiteCharacteristicsDataStore*
LocalSiteCharacteristicsDataStoreFactory::GetForProfile(Profile* profile) {
  if (base::FeatureList::IsEnabled(features::kProactiveTabDiscarding)) {
    return static_cast<LocalSiteCharacteristicsDataStore*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }
  return nullptr;
}

LocalSiteCharacteristicsDataStoreFactory*
LocalSiteCharacteristicsDataStoreFactory::GetInstance() {
  static base::NoDestructor<LocalSiteCharacteristicsDataStoreFactory> instance;
  return instance.get();
}

LocalSiteCharacteristicsDataStoreFactory::
    LocalSiteCharacteristicsDataStoreFactory()
    : BrowserContextKeyedServiceFactory(
          "LocalSiteCharacteristicsDataStore",
          BrowserContextDependencyManager::GetInstance()) {}

LocalSiteCharacteristicsDataStoreFactory::
    ~LocalSiteCharacteristicsDataStoreFactory() = default;

SiteCharacteristicsDataStore*
LocalSiteCharacteristicsDataStoreFactory::GetExistingDataStoreForContext(
    content::BrowserContext* context) const {
  DCHECK(context);
  // Remove the const attribute from |this| and calls
  // |GetServiceForBrowserContext| with the |create| parameter set to false.
  // This is basically a getter that returns the data store associated with a
  // context without creating it if it doesn't exits.
  SiteCharacteristicsDataStore* data_store =
      static_cast<SiteCharacteristicsDataStore*>(
          const_cast<LocalSiteCharacteristicsDataStoreFactory*>(this)
              ->GetServiceForBrowserContext(context, false /* create */));
  DCHECK(data_store);
  return data_store;
}

KeyedService* LocalSiteCharacteristicsDataStoreFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  SiteCharacteristicsDataStore* data_store = nullptr;
  if (context->IsOffTheRecord()) {
    content::BrowserContext* parent_context =
        chrome::GetBrowserContextRedirectedInIncognito(context);
    DCHECK(parent_context);
    // Off the record profiles correspond to incognito profile and are derived
    // from a parent profile that is on record.
    DCHECK(!parent_context->IsOffTheRecord());
    SiteCharacteristicsDataStore* data_store_for_readers =
        GetExistingDataStoreForContext(parent_context);
    DCHECK(data_store_for_readers);
    data_store = new LocalSiteCharacteristicsNonRecordingDataStore(
        data_store_for_readers);
  } else {
    Profile* profile = Profile::FromBrowserContext(context);
    DCHECK(profile);
    data_store = new LocalSiteCharacteristicsDataStore(profile);
  }
  DCHECK(data_store);
  return data_store;
}

content::BrowserContext*
LocalSiteCharacteristicsDataStoreFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool LocalSiteCharacteristicsDataStoreFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return base::FeatureList::IsEnabled(features::kProactiveTabDiscarding);
}

bool LocalSiteCharacteristicsDataStoreFactory::ServiceIsNULLWhileTesting()
    const {
  return true;
}

}  // namespace resource_coordinator
