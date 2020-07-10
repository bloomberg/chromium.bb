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
const char DefaultSearchManager::kImageURL[] = "image_url";
const char DefaultSearchManager::kNewTabURL[] = "new_tab_url";
const char DefaultSearchManager::kContextualSearchURL[] =
    "contextual_search_url";
const char DefaultSearchManager::kFaviconURL[] = "favicon_url";
const char DefaultSearchManager::kLogoURL[] = "logo_url";
const char DefaultSearchManager::kDoodleURL[] = "doodle_url";
const char DefaultSearchManager::kOriginatingURL[] = "originating_url";

const char DefaultSearchManager::kSearchURLPostParams[] =
    "search_url_post_params";
const char DefaultSearchManager::kSuggestionsURLPostParams[] =
    "suggestions_url_post_params";
const char DefaultSearchManager::kImageURLPostParams[] =
    "image_url_post_params";

const char DefaultSearchManager::kSafeForAutoReplace[] = "safe_for_autoreplace";
const char DefaultSearchManager::kInputEncodings[] = "input_encodings";

const char DefaultSearchManager::kDateCreated[] = "date_created";
const char DefaultSearchManager::kLastModified[] = "last_modified";
const char DefaultSearchManager::kLastVisited[] = "last_visited";

const char DefaultSearchManager::kUsageCount[] = "usage_count";
const char DefaultSearchManager::kAlternateURLs[] = "alternate_urls";
const char DefaultSearchManager::kCreatedByPolicy[] = "created_by_policy";
const char DefaultSearchManager::kDisabledByPolicy[] = "disabled_by_policy";
const char DefaultSearchManager::kCreatedFromPlayAPI[] =
    "created_from_play_api";

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
  DCHECK(value);
  pref_value_map->SetValue(kDefaultSearchProviderDataPrefName,
                           base::Value::FromUniquePtrValue(std::move(value)));
}

// static
void DefaultSearchManager::SetFallbackSearchEnginesDisabledForTesting(
    bool disabled) {
  g_fallback_search_engines_disabled = disabled;
}

const TemplateURLData* DefaultSearchManager::GetDefaultSearchEngine(
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
  return GetFallbackSearchEngine();
}

DefaultSearchManager::Source
DefaultSearchManager::GetDefaultSearchEngineSource() const {
  Source source;
  GetDefaultSearchEngine(&source);
  return source;
}

const TemplateURLData* DefaultSearchManager::GetFallbackSearchEngine() const {
  return g_fallback_search_engines_disabled ? nullptr
                                            : fallback_default_search_.get();
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

void DefaultSearchManager::ClearUserSelectedDefaultSearchEngine() {
  if (pref_service_) {
    pref_service_->ClearPref(kDefaultSearchProviderDataPrefName);
  } else {
    prefs_default_search_.reset();
    NotifyObserver();
  }
}

void DefaultSearchManager::OnDefaultSearchPrefChanged() {
  bool source_was_fallback = GetDefaultSearchEngineSource() == FROM_FALLBACK;

  LoadDefaultSearchEngineFromPrefs();

  // The effective DSE may have changed unless we were using the fallback source
  // both before and after the above load.
  if (!source_was_fallback || (GetDefaultSearchEngineSource() != FROM_FALLBACK))
    NotifyObserver();
}

void DefaultSearchManager::OnOverridesPrefChanged() {
  LoadPrepopulatedDefaultSearch();

  const TemplateURLData* effective_data = GetDefaultSearchEngine(nullptr);
  if (effective_data && effective_data->prepopulate_id) {
    // A user-selected, policy-selected or fallback pre-populated engine is
    // active and may have changed with this event.
    NotifyObserver();
  }
}

void DefaultSearchManager::MergePrefsDataWithPrepopulated() {
  if (!prefs_default_search_ || !prefs_default_search_->prepopulate_id)
    return;

  // TODO(crbug.com/1049784): Parameters for search engine created from play api
  // should be preserved even if corresponding prepopulated search engine
  // exists. This logic will be revisited as part of implementation of
  // crbug.com/1049784, which will enable updating play api search engine
  // parameters with prepopulated data.
  if (prefs_default_search_->created_from_play_api)
    return;

  std::vector<std::unique_ptr<TemplateURLData>> prepopulated_urls =
      TemplateURLPrepopulateData::GetPrepopulatedEngines(pref_service_,
                                                         nullptr);

  auto default_engine = std::find_if(
      prepopulated_urls.begin(), prepopulated_urls.end(),
      [&](const std::unique_ptr<TemplateURLData>& url) {
        return url->prepopulate_id == prefs_default_search_->prepopulate_id;
      });

  if (default_engine == prepopulated_urls.end())
    return;

  auto& engine = *default_engine;

  if (!prefs_default_search_->safe_for_autoreplace) {
    engine->safe_for_autoreplace = false;
    engine->SetKeyword(prefs_default_search_->keyword());
    engine->SetShortName(prefs_default_search_->short_name());
  }

  engine->id = prefs_default_search_->id;
  engine->sync_guid = prefs_default_search_->sync_guid;
  engine->date_created = prefs_default_search_->date_created;
  engine->last_modified = prefs_default_search_->last_modified;
  engine->last_visited = prefs_default_search_->last_visited;
  engine->favicon_url = prefs_default_search_->favicon_url;

  prefs_default_search_ = std::move(engine);
}

void DefaultSearchManager::LoadDefaultSearchEngineFromPrefs() {
  if (!pref_service_)
    return;

  prefs_default_search_.reset();
  extension_default_search_.reset();
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

  // Check if default search preference is overriden by extension.
  if (pref->IsExtensionControlled()) {
    extension_default_search_ = std::move(turl_data);
  } else {
    prefs_default_search_ = std::move(turl_data);
    MergePrefsDataWithPrepopulated();
  }
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
    const TemplateURLData* data = GetDefaultSearchEngine(&source);
    change_observer_.Run(data, source);
  }
}
