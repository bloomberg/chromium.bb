// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/default_search_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_value_map.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/search_engines/template_url_prepopulate_data.h"

namespace {

bool g_fallback_search_engines_disabled = false;

}  // namespace

// A dictionary to hold all data related to the Default Search Engine.
// Eventually, this should replace all the data stored in the
// default_search_provider.* prefs.
const char DefaultSearchManager::kDefaultSearchProviderDataPrefName[] =
    "default_search_provider_data.template_url_data";

const char DefaultSearchManager::kID[] = "id";
const char DefaultSearchManager::kShortName[] = "short_name";
const char DefaultSearchManager::kKeyword[] = "keyword";
const char DefaultSearchManager::kPrepopulateID[] = "prepopulate_id";
const char DefaultSearchManager::kSyncGUID[] = "synced_guid";

const char DefaultSearchManager::kURL[] = "url";
const char DefaultSearchManager::kSuggestionsURL[] = "suggestions_url";
const char DefaultSearchManager::kInstantURL[] = "instant_url";
const char DefaultSearchManager::kImageURL[] = "image_url";
const char DefaultSearchManager::kNewTabURL[] = "new_tab_url";
const char DefaultSearchManager::kContextualSearchURL[] =
    "contextual_search_url";
const char DefaultSearchManager::kFaviconURL[] = "favicon_url";
const char DefaultSearchManager::kOriginatingURL[] = "originating_url";

const char DefaultSearchManager::kSearchURLPostParams[] =
    "search_url_post_params";
const char DefaultSearchManager::kSuggestionsURLPostParams[] =
    "suggestions_url_post_params";
const char DefaultSearchManager::kInstantURLPostParams[] =
    "instant_url_post_params";
const char DefaultSearchManager::kImageURLPostParams[] =
    "image_url_post_params";

const char DefaultSearchManager::kSafeForAutoReplace[] = "safe_for_autoreplace";
const char DefaultSearchManager::kInputEncodings[] = "input_encodings";

const char DefaultSearchManager::kDateCreated[] = "date_created";
const char DefaultSearchManager::kLastModified[] = "last_modified";

const char DefaultSearchManager::kUsageCount[] = "usage_count";
const char DefaultSearchManager::kAlternateURLs[] = "alternate_urls";
const char DefaultSearchManager::kSearchTermsReplacementKey[] =
    "search_terms_replacement_key";
const char DefaultSearchManager::kCreatedByPolicy[] = "created_by_policy";
const char DefaultSearchManager::kDisabledByPolicy[] = "disabled_by_policy";

DefaultSearchManager::DefaultSearchManager(
    PrefService* pref_service,
    const ObserverCallback& change_observer)
    : pref_service_(pref_service),
      change_observer_(change_observer),
      default_search_controlled_by_policy_(false) {
  if (pref_service_) {
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        kDefaultSearchProviderDataPrefName,
        base::Bind(&DefaultSearchManager::OnDefaultSearchPrefChanged,
                   base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kSearchProviderOverrides,
        base::Bind(&DefaultSearchManager::OnOverridesPrefChanged,
                   base::Unretained(this)));
  }
  LoadPrepopulatedDefaultSearch();
  LoadDefaultSearchEngineFromPrefs();
}

DefaultSearchManager::~DefaultSearchManager() {
}

// static
void DefaultSearchManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kDefaultSearchProviderDataPrefName);
}

// static
void DefaultSearchManager::AddPrefValueToMap(
    std::unique_ptr<base::DictionaryValue> value,
    PrefValueMap* pref_value_map) {
  pref_value_map->SetValue(kDefaultSearchProviderDataPrefName,
                           std::move(value));
}

// static
void DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(
    bool disabled) {
  g_fallback_search_engines_disabled = disabled;
}

TemplateURLData* DefaultSearchManager::GetDefaultSearchEngine(
    Source* source) const {
  if (default_search_controlled_by_policy_) {
    if (source)
      *source = FROM_POLICY;
    return prefs_default_search_.get();
  }
  if (extension_default_search_) {
    if (source)
      *source = FROM_EXTENSION;
    return extension_default_search_.get();
  }
  if (prefs_default_search_) {
    if (source)
      *source = FROM_USER;
    return prefs_default_search_.get();
  }

  if (source)
    *source = FROM_FALLBACK;
  return g_fallback_search_engines_disabled ?
      NULL : fallback_default_search_.get();
}

DefaultSearchManager::Source
DefaultSearchManager::GetDefaultSearchEngineSource() const {
  Source source;
  GetDefaultSearchEngine(&source);
  return source;
}

void DefaultSearchManager::SetUserSelectedDefaultSearchEngine(
    const TemplateURLData& data) {
  if (!pref_service_) {
    prefs_default_search_.reset(new TemplateURLData(data));
    MergePrefsDataWithPrepopulated();
    NotifyObserver();
    return;
  }
  pref_service_->Set(kDefaultSearchProviderDataPrefName,
                     *TemplateURLDataToDictionary(data));
}

void DefaultSearchManager::SetExtensionControlledDefaultSearchEngine(
    const TemplateURLData& data) {
  extension_default_search_.reset(new TemplateURLData(data));
  if (GetDefaultSearchEngineSource() == FROM_EXTENSION)
    NotifyObserver();
}

void DefaultSearchManager::ClearExtensionControlledDefaultSearchEngine() {
  Source old_source = GetDefaultSearchEngineSource();
  extension_default_search_.reset();
  if (old_source == FROM_EXTENSION)
    NotifyObserver();
}

void DefaultSearchManager::ClearUserSelectedDefaultSearchEngine() {
  if (pref_service_) {
    pref_service_->ClearPref(kDefaultSearchProviderDataPrefName);
  } else {
    prefs_default_search_.reset();
    NotifyObserver();
  }
}

void DefaultSearchManager::OnDefaultSearchPrefChanged() {
  Source source = GetDefaultSearchEngineSource();
  LoadDefaultSearchEngineFromPrefs();

  // If we were/are FROM_USER or FROM_POLICY the effective DSE may have changed.
  if (source != FROM_USER && source != FROM_POLICY)
    source = GetDefaultSearchEngineSource();
  if (source == FROM_USER || source == FROM_POLICY)
    NotifyObserver();
}

void DefaultSearchManager::OnOverridesPrefChanged() {
  LoadPrepopulatedDefaultSearch();

  TemplateURLData* effective_data = GetDefaultSearchEngine(NULL);
  if (effective_data && effective_data->prepopulate_id) {
    // A user-selected, policy-selected or fallback pre-populated engine is
    // active and may have changed with this event.
    NotifyObserver();
  }
}

void DefaultSearchManager::MergePrefsDataWithPrepopulated() {
  if (!prefs_default_search_ || !prefs_default_search_->prepopulate_id)
    return;

  size_t default_search_index;
  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(pref_service_,
                                                         &default_search_index);

  for (auto& engine : prepopulated_urls) {
    if (engine->prepopulate_id != prefs_default_search_->prepopulate_id)
      continue;

    if (!prefs_default_search_->safe_for_autoreplace) {
      engine->safe_for_autoreplace = false;
      engine->SetKeyword(prefs_default_search_->keyword());
      engine->SetShortName(prefs_default_search_->short_name());
    }
    engine->id = prefs_default_search_->id;
    engine->sync_guid = prefs_default_search_->sync_guid;
    engine->date_created = prefs_default_search_->date_created;
    engine->last_modified = prefs_default_search_->last_modified;

    prefs_default_search_ = std::move(engine);
    return;
  }
}

void DefaultSearchManager::LoadDefaultSearchEngineFromPrefs() {
  if (!pref_service_)
    return;

  prefs_default_search_.reset();
  const PrefService::Preference* pref =
      pref_service_->FindPreference(kDefaultSearchProviderDataPrefName);
  DCHECK(pref);
  default_search_controlled_by_policy_ = pref->IsManaged();

  const base::DictionaryValue* url_dict =
      pref_service_->GetDictionary(kDefaultSearchProviderDataPrefName);
  if (url_dict->empty())
    return;

  if (default_search_controlled_by_policy_) {
    bool disabled_by_policy = false;
    if (url_dict->GetBoolean(kDisabledByPolicy, &disabled_by_policy) &&
        disabled_by_policy)
      return;
  }

  auto turl_data = TemplateURLDataFromDictionary(*url_dict);
  if (!turl_data)
    return;

  prefs_default_search_ = std::move(turl_data);
  MergePrefsDataWithPrepopulated();
}

void DefaultSearchManager::LoadPrepopulatedDefaultSearch() {
  std::unique_ptr<TemplateURLData> data =
      TemplateURLPrepopulateData::GetPrepopulatedDefaultSearch(pref_service_);
  fallback_default_search_ = std::move(data);
  MergePrefsDataWithPrepopulated();
}

void DefaultSearchManager::NotifyObserver() {
  if (!change_observer_.is_null()) {
    Source source = FROM_FALLBACK;
    TemplateURLData* data = GetDefaultSearchEngine(&source);
    change_observer_.Run(data, source);
  }
}
