// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_metrics_service.h"

#include <map>

#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

using TemplateURLPrepopulateData::kMaxPrepopulatedEngineID;

namespace {

// Use a map to convert domains to their histogram identifiers. Ids are defined
// in tools/metrics/histograms/histograms.xml and (usually) also in
// chrome/browser/search_engines/prepopulated_engines.json.
typedef std::map<std::string, int> DomainIdMap;

// Converts a host name into a domain name for easier matching.
std::string GetDomain(const std::string& host) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      host,
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

void AddDomain(const std::string& domain,
               int domain_id,
               DomainIdMap* domain_id_map) {
  domain_id_map->insert(std::make_pair(domain, domain_id));
}

// Builds a map that associates domain name strings with histogram enum values,
// for prepopulated DSEs and select non-prepopulated ones.
void BuildDomainIdMap(Profile* profile, DomainIdMap* domain_id_map) {
  // Add prepopulated search engine hosts to the map.
  ScopedVector<TemplateURL> prepopulated_urls;
  size_t default_search_index;  // unused
  TemplateURLPrepopulateData::GetPrepopulatedEngines(profile,
      &prepopulated_urls.get(), &default_search_index);
  for (size_t i = 0; i < prepopulated_urls.size(); ++i) {
    AddDomain(GetDomain(prepopulated_urls[i]->url_ref().GetHost()),
              prepopulated_urls[i]->prepopulate_id(),
              domain_id_map);
  }
  // Add some common search engine domains that are not prepopulated. Assign
  // these domains id numbers 102-114 which extend the prepopulated engines
  // histogram enum.
  AddDomain("searchnu.com", 102, domain_id_map);
  AddDomain("babylon.com", 103, domain_id_map);
  AddDomain("delta-search.com", 104, domain_id_map);
  AddDomain("iminent.com", 105, domain_id_map);
  AddDomain("hao123.com", 106, domain_id_map);
  AddDomain("sweetim.com", 107, domain_id_map);
  AddDomain("snap.do", 108, domain_id_map);
  AddDomain("snapdo.com", 109, domain_id_map);
  AddDomain("softonic.com", 110, domain_id_map);
  AddDomain("searchfunmoods.com", 111, domain_id_map);
  AddDomain("incredibar.com", 112, domain_id_map);
  AddDomain("sweetpacks.com", 113, domain_id_map);
  AddDomain("imesh.net", 114, domain_id_map);
  // IMPORTANT: If you add more domains here, be sure to update the
  // kMaxPrepopulatedEngineID and available ids in prepopulated_engines.json.

  // The following hosts may not be prepopulated, depending on the country
  // settings. Add them here, using their existing ids. See histograms.xml.
  AddDomain("conduit.com", 36, domain_id_map);
  AddDomain("avg.com", 50, domain_id_map);
  AddDomain("mail.ru", 83, domain_id_map);
}

// Maps a host name to a histogram enum value. The enum value '0' represents
// 'Unknown', i.e. an unrecognized host.
int MapHostToId(const DomainIdMap& domain_id_map, const std::string& host) {
  DomainIdMap::const_iterator it = domain_id_map.find(GetDomain(host));
  if (it != domain_id_map.end())
    return it->second;
  return 0;
}

}  // namespace

PrefMetricsService::PrefMetricsService(Profile* profile)
    : profile_(profile) {
  RecordLaunchPrefs();
}

PrefMetricsService::~PrefMetricsService() {
}

void PrefMetricsService::RecordLaunchPrefs() {
  PrefService* prefs = profile_->GetPrefs();
  const bool show_home_button = prefs->GetBoolean(prefs::kShowHomeButton);
  const bool home_page_is_ntp = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);

  UMA_HISTOGRAM_BOOLEAN("Settings.ShowHomeButton", show_home_button);
  UMA_HISTOGRAM_BOOLEAN("Settings.HomePageIsNewTabPage", home_page_is_ntp);

  int restore_on_startup = profile_->GetPrefs()->GetInteger(
      prefs::kRestoreOnStartup);
  UMA_HISTOGRAM_ENUMERATION("Settings.StartupPageLoadSettings",
      restore_on_startup, SessionStartupPref::kPrefValueMax);
  if (restore_on_startup == SessionStartupPref::kPrefValueURLs) {
    const int url_list_size = profile_->GetPrefs()->GetList(
        prefs::kURLsToRestoreOnStartup)->GetSize();
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Settings.StartupPageLoadURLs", url_list_size, 1, 50, 20);
  }

  DomainIdMap domain_id_map;
  BuildDomainIdMap(profile_, &domain_id_map);

  // Record the default search engine id.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (template_url_service) {
    TemplateURL* template_url =
        template_url_service->GetDefaultSearchProvider();
    if (template_url) {
      const int domain_id =
          MapHostToId(domain_id_map, template_url->url_ref().GetHost());
      UMA_HISTOGRAM_ENUMERATION("Settings.DefaultSearchProvider",
                                domain_id, kMaxPrepopulatedEngineID);
    }
  }
  // If the home page isn't the NTP, record the home page domain id.
  if (!home_page_is_ntp) {
    GURL homepage_url(prefs->GetString(prefs::kHomePage));
    if (homepage_url.is_valid()) {
      const int domain_id = MapHostToId(domain_id_map, homepage_url.host());
      UMA_HISTOGRAM_ENUMERATION("Settings.HomePageDomain",
                                domain_id, kMaxPrepopulatedEngineID);
    }
  }
  // If startup pages are set, record all startup page domain ids.
  if (restore_on_startup == SessionStartupPref::kPrefValueURLs) {
    const ListValue* url_list = prefs->GetList(prefs::kURLsToRestoreOnStartup);
    for (size_t i = 0; i < url_list->GetSize(); i++) {
      std::string url_text;
      if (url_list->GetString(i, &url_text)) {
        GURL start_url(url_text);
        if (start_url.is_valid()) {
          const int domain_id = MapHostToId(domain_id_map, start_url.host());
          UMA_HISTOGRAM_ENUMERATION("Settings.StartupPageDomains",
                                    domain_id, kMaxPrepopulatedEngineID);
        }
      }
    }
  }
}

// static
PrefMetricsService::Factory* PrefMetricsService::Factory::GetInstance() {
  return Singleton<PrefMetricsService::Factory>::get();
}

// static
PrefMetricsService* PrefMetricsService::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<PrefMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

PrefMetricsService::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
        "PrefMetricsService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

PrefMetricsService::Factory::~Factory() {
}

BrowserContextKeyedService*
PrefMetricsService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new PrefMetricsService(static_cast<Profile*>(profile));
}

bool PrefMetricsService::Factory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool PrefMetricsService::Factory::ServiceIsNULLWhileTesting() const {
  return false;
}

content::BrowserContext* PrefMetricsService::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
