// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover_factory.h"

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browsing_data/browsing_data_remover_impl.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/features/features.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/precache/precache_manager_factory.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "extensions/browser/extension_prefs_factory.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service_factory.h"
#endif

// static
BrowsingDataRemoverFactory* BrowsingDataRemoverFactory::GetInstance() {
  return base::Singleton<BrowsingDataRemoverFactory>::get();
}

// static
BrowsingDataRemover* BrowsingDataRemoverFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<BrowsingDataRemoverImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

BrowsingDataRemoverFactory::BrowsingDataRemoverFactory()
    : BrowserContextKeyedServiceFactory(
          "BrowsingDataRemover",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(autofill::PersonalDataManagerFactory::GetInstance());
  DependsOn(DataReductionProxyChromeSettingsFactory::GetInstance());
  DependsOn(domain_reliability::DomainReliabilityServiceFactory::GetInstance());
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(PasswordStoreFactory::GetInstance());
  DependsOn(prerender::PrerenderManagerFactory::GetInstance());
  DependsOn(TabRestoreServiceFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(WebDataServiceFactory::GetInstance());
  DependsOn(WebHistoryServiceFactory::GetInstance());

#if defined(OS_ANDROID)
  DependsOn(offline_pages::OfflinePageModelFactory::GetInstance());
  DependsOn(precache::PrecacheManagerFactory::GetInstance());
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  DependsOn(extensions::ActivityLog::GetFactoryInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  DependsOn(SessionServiceFactory::GetInstance());
#endif
}

BrowsingDataRemoverFactory::~BrowsingDataRemoverFactory() {}

content::BrowserContext* BrowsingDataRemoverFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // For guest profiles the browsing data is in the OTR profile.
  Profile* profile = static_cast<Profile*>(context);
  DCHECK(!profile->IsGuestSession() || profile->IsOffTheRecord());

  return profile;
}

KeyedService* BrowsingDataRemoverFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  BrowsingDataRemoverImpl* remover = new BrowsingDataRemoverImpl(context);
  remover->SetEmbedderDelegate(
      base::MakeUnique<ChromeBrowsingDataRemoverDelegate>(context));
  return remover;
}
