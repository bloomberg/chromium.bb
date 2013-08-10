// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_metrics_service.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/prefs/synced_pref_change_registrar.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace {

// Converts a host name into a domain name for easier matching.
std::string GetDomainFromHost(const std::string& host) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      host,
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

const int kSessionStartupPrefValueMax = SessionStartupPref::kPrefValueMax;

}  // namespace

PrefMetricsService::PrefMetricsService(Profile* profile)
    : profile_(profile) {
  RecordLaunchPrefs();

  PrefServiceSyncable* prefs = PrefServiceSyncable::FromProfile(profile_);
  synced_pref_change_registrar_.reset(new SyncedPrefChangeRegistrar(prefs));

  RegisterSyncedPrefObservers();
}

PrefMetricsService::~PrefMetricsService() {
}

void PrefMetricsService::RecordLaunchPrefs() {
  PrefService* prefs = profile_->GetPrefs();
  bool show_home_button = prefs->GetBoolean(prefs::kShowHomeButton);
  bool home_page_is_ntp = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);
  UMA_HISTOGRAM_BOOLEAN("Settings.ShowHomeButton", show_home_button);
  if (show_home_button) {
    UMA_HISTOGRAM_BOOLEAN("Settings.GivenShowHomeButton_HomePageIsNewTabPage",
        home_page_is_ntp);
  }
  int restore_on_startup = prefs->GetInteger(prefs::kRestoreOnStartup);
  UMA_HISTOGRAM_ENUMERATION("Settings.StartupPageLoadSettings",
      restore_on_startup, kSessionStartupPrefValueMax);
  if (restore_on_startup == SessionStartupPref::kPrefValueURLs) {
    const int url_list_size = prefs->GetList(
        prefs::kURLsToRestoreOnStartup)->GetSize();
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Settings.StartupPageLoadURLs", url_list_size, 1, 50, 20);
  }
}

void PrefMetricsService::RegisterSyncedPrefObservers() {
  LogHistogramValueCallback booleanHandler = base::Bind(
      &PrefMetricsService::LogBooleanPrefChange, base::Unretained(this));

  AddPrefObserver(prefs::kShowHomeButton, "ShowHomeButton", booleanHandler);
  AddPrefObserver(prefs::kHomePageIsNewTabPage, "HomePageIsNewTabPage",
                  booleanHandler);

  AddPrefObserver(prefs::kRestoreOnStartup, "StartupPageLoadSettings",
                  base::Bind(&PrefMetricsService::LogIntegerPrefChange,
                             base::Unretained(this),
                             kSessionStartupPrefValueMax));
}

void PrefMetricsService::AddPrefObserver(
    const std::string& path,
    const std::string& histogram_name_prefix,
    const LogHistogramValueCallback& callback) {
  synced_pref_change_registrar_->Add(path.c_str(),
      base::Bind(&PrefMetricsService::OnPrefChanged,
                 base::Unretained(this),
                 histogram_name_prefix, callback));
}

void PrefMetricsService::OnPrefChanged(
    const std::string& histogram_name_prefix,
    const LogHistogramValueCallback& callback,
    const std::string& path,
    bool from_sync) {
  PrefServiceSyncable* prefs = PrefServiceSyncable::FromProfile(profile_);
  const PrefService::Preference* pref = prefs->FindPreference(path.c_str());
  DCHECK(pref);
  std::string source_name(
      from_sync ? ".PulledFromSync" : ".PushedToSync");
  std::string histogram_name("Settings." + histogram_name_prefix + source_name);
  callback.Run(histogram_name, pref->GetValue());
};

void PrefMetricsService::LogBooleanPrefChange(const std::string& histogram_name,
                                              const Value* value) {
  bool boolean_value = false;
  if (!value->GetAsBoolean(&boolean_value))
    return;
  base::HistogramBase* histogram = base::BooleanHistogram::FactoryGet(
      histogram_name, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(boolean_value);
}

void PrefMetricsService::LogIntegerPrefChange(int boundary_value,
                                              const std::string& histogram_name,
                                              const Value* value) {
  int integer_value = 0;
  if (!value->GetAsInteger(&integer_value))
    return;
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      histogram_name,
      1,
      boundary_value,
      boundary_value + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(integer_value);
}

void PrefMetricsService::LogListPrefChange(
    const LogHistogramValueCallback& item_callback,
    const std::string& histogram_name,
    const Value* value) {
  const ListValue* items = NULL;
  if (!value->GetAsList(&items))
    return;
  for (size_t i = 0; i < items->GetSize(); ++i) {
    const Value *item_value = NULL;
    if (items->Get(i, &item_value))
      item_callback.Run(histogram_name, item_value);
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
