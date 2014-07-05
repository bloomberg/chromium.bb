// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/default_search_pref_migration.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "components/search_engines/default_search_manager.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"

namespace {

// Loads the user-selected DSE (if there is one, and it's not masked by policy
// or an extension) from legacy preferences.
scoped_ptr<TemplateURLData> LoadDefaultSearchProviderFromPrefs(
    PrefService* pref_service) {
  scoped_ptr<TemplateURLData> legacy_dse_from_prefs;
  bool legacy_is_managed = false;
  TemplateURLService::LoadDefaultSearchProviderFromPrefs(
      pref_service, &legacy_dse_from_prefs, &legacy_is_managed);
  return legacy_is_managed ?
      scoped_ptr<TemplateURLData>() : legacy_dse_from_prefs.Pass();
}

void ClearDefaultSearchProviderFromLegacyPrefs(PrefService* prefs) {
  prefs->ClearPref(prefs::kDefaultSearchProviderName);
  prefs->ClearPref(prefs::kDefaultSearchProviderKeyword);
  prefs->ClearPref(prefs::kDefaultSearchProviderSearchURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderSuggestURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderInstantURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderImageURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderNewTabURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderSearchURLPostParams);
  prefs->ClearPref(prefs::kDefaultSearchProviderSuggestURLPostParams);
  prefs->ClearPref(prefs::kDefaultSearchProviderInstantURLPostParams);
  prefs->ClearPref(prefs::kDefaultSearchProviderImageURLPostParams);
  prefs->ClearPref(prefs::kDefaultSearchProviderIconURL);
  prefs->ClearPref(prefs::kDefaultSearchProviderEncodings);
  prefs->ClearPref(prefs::kDefaultSearchProviderPrepopulateID);
  prefs->ClearPref(prefs::kDefaultSearchProviderAlternateURLs);
  prefs->ClearPref(prefs::kDefaultSearchProviderSearchTermsReplacementKey);
}

void MigrateDefaultSearchPref(PrefService* pref_service) {
  DCHECK(pref_service);

  scoped_ptr<TemplateURLData> legacy_dse_from_prefs =
      LoadDefaultSearchProviderFromPrefs(pref_service);
  if (!legacy_dse_from_prefs)
    return;

  DefaultSearchManager default_search_manager(
      pref_service, DefaultSearchManager::ObserverCallback());
  DefaultSearchManager::Source modern_source;
  TemplateURLData* modern_value =
      default_search_manager.GetDefaultSearchEngine(&modern_source);
  if (modern_source == DefaultSearchManager::FROM_FALLBACK) {
    // |modern_value| is the prepopulated default. If it matches the legacy DSE
    // we assume it is not a user-selected value.
    if (!modern_value ||
        legacy_dse_from_prefs->prepopulate_id != modern_value->prepopulate_id) {
      // This looks like a user-selected value, so let's migrate it.
      // TODO(erikwright): Remove this migration logic when this stat approaches
      // zero.
      UMA_HISTOGRAM_BOOLEAN("Search.MigratedPrefToDictionaryValue", true);
      default_search_manager.SetUserSelectedDefaultSearchEngine(
          *legacy_dse_from_prefs);
    }
  }

  ClearDefaultSearchProviderFromLegacyPrefs(pref_service);
}

void OnPrefsInitialized(PrefService* pref_service,
                        bool pref_service_initialization_success) {
  MigrateDefaultSearchPref(pref_service);
}

}  // namespace

void ConfigureDefaultSearchPrefMigrationToDictionaryValue(
    PrefService* pref_service) {
  if (pref_service->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_WAITING) {
    pref_service->AddPrefInitObserver(
        base::Bind(&OnPrefsInitialized, base::Unretained(pref_service)));
  } else {
    MigrateDefaultSearchPref(pref_service);
  }
}
