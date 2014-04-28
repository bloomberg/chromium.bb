// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/default_search_manager.h"

#include <algorithm>
#include <utility>

#include "base/compiler_specific.h"
#include "base/i18n/case_conversion.h"
#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/util.h"
#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"

namespace {

// A dictionary to hold all data related to the Default Search Engine.
// Eventually, this should replace all the data stored in the
// default_search_provider.* prefs.
const char kDefaultSearchProviderData[] =
    "default_search_provider_data.template_url_data";

}  // namespace

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

DefaultSearchManager::DefaultSearchManager(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
}

DefaultSearchManager::~DefaultSearchManager() {
}

// static
void DefaultSearchManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      kDefaultSearchProviderData,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

bool DefaultSearchManager::GetDefaultSearchEngine(TemplateURLData* data) {
  const base::DictionaryValue* url_dict =
      pref_service_->GetDictionary(kDefaultSearchProviderData);

  if (url_dict->empty())
    return false;

  std::string search_url;
  base::string16 keyword;
  url_dict->GetString(kURL, &search_url);
  url_dict->GetString(kKeyword, &keyword);
  if (search_url.empty())
    return false;
  if (keyword.empty())
    keyword = TemplateURLService::GenerateKeyword(GURL(search_url));
  data->SetKeyword(keyword);
  data->SetURL(search_url);

  std::string id;
  url_dict->GetString(kID, &id);
  base::StringToInt64(id, &data->id);
  url_dict->GetString(kShortName, &data->short_name);
  url_dict->GetInteger(kPrepopulateID, &data->prepopulate_id);
  url_dict->GetString(kSyncGUID, &data->sync_guid);

  url_dict->GetString(kSuggestionsURL, &data->suggestions_url);
  url_dict->GetString(kInstantURL, &data->instant_url);
  url_dict->GetString(kImageURL, &data->image_url);
  url_dict->GetString(kNewTabURL, &data->new_tab_url);

  std::string favicon_url;
  std::string originating_url;
  url_dict->GetString(kFaviconURL, &favicon_url);
  url_dict->GetString(kOriginatingURL, &originating_url);
  data->favicon_url = GURL(favicon_url);
  data->originating_url = GURL(originating_url);

  url_dict->GetString(kSearchURLPostParams, &data->search_url_post_params);
  url_dict->GetString(kSuggestionsURLPostParams,
                      &data->suggestions_url_post_params);
  url_dict->GetString(kInstantURLPostParams, &data->instant_url_post_params);
  url_dict->GetString(kImageURLPostParams, &data->image_url_post_params);

  url_dict->GetBoolean(kSafeForAutoReplace, &data->safe_for_autoreplace);

  double date_created = 0.0;
  double last_modified = 0.0;
  url_dict->GetDouble(kDateCreated, &date_created);
  url_dict->GetDouble(kLastModified, &last_modified);
  data->date_created = base::Time::FromInternalValue(date_created);
  data->last_modified = base::Time::FromInternalValue(last_modified);

  url_dict->GetInteger(kUsageCount, &data->usage_count);

  const base::ListValue* alternate_urls = new base::ListValue;
  url_dict->GetList(kAlternateURLs, &alternate_urls);
  data->alternate_urls.clear();
  for (base::ListValue::const_iterator it = alternate_urls->begin();
       it != alternate_urls->end(); ++it) {
    std::string alternate_url;
    if ((*it)->GetAsString(&alternate_url))
      data->alternate_urls.push_back(alternate_url);
  }

  const base::ListValue* encodings = new base::ListValue;
  url_dict->GetList(kInputEncodings, &encodings);
  data->input_encodings.clear();
  for (base::ListValue::const_iterator it = encodings->begin();
       it != encodings->end(); ++it) {
    std::string encoding;
    if ((*it)->GetAsString(&encoding))
      data->input_encodings.push_back(encoding);
  }

  url_dict->GetString(kSearchTermsReplacementKey,
                      &data->search_terms_replacement_key);

  url_dict->GetBoolean(kCreatedByPolicy, &data->created_by_policy);

  data->show_in_default_list = true;

  return true;
}

void DefaultSearchManager::SetUserSelectedDefaultSearchEngine(
    const TemplateURLData& data) {
  base::DictionaryValue* url_dict = new base::DictionaryValue;
  url_dict->SetString(kID, base::Int64ToString(data.id));
  url_dict->SetString(kShortName, data.short_name);
  url_dict->SetString(kKeyword, data.keyword());
  url_dict->SetInteger(kPrepopulateID, data.prepopulate_id);
  url_dict->SetString(kSyncGUID, data.sync_guid);

  url_dict->SetString(kURL, data.url());
  url_dict->SetString(kSuggestionsURL, data.suggestions_url);
  url_dict->SetString(kInstantURL, data.instant_url);
  url_dict->SetString(kImageURL, data.image_url);
  url_dict->SetString(kNewTabURL, data.new_tab_url);
  url_dict->SetString(kFaviconURL, data.favicon_url.spec());
  url_dict->SetString(kOriginatingURL, data.originating_url.spec());

  url_dict->SetString(kSearchURLPostParams, data.search_url_post_params);
  url_dict->SetString(kSuggestionsURLPostParams,
                      data.suggestions_url_post_params);
  url_dict->SetString(kInstantURLPostParams, data.instant_url_post_params);
  url_dict->SetString(kImageURLPostParams, data.image_url_post_params);

  url_dict->SetBoolean(kSafeForAutoReplace, data.safe_for_autoreplace);

  url_dict->SetDouble(kDateCreated, data.date_created.ToInternalValue());
  url_dict->SetDouble(kLastModified, data.last_modified.ToInternalValue());
  url_dict->SetInteger(kUsageCount, data.usage_count);

  base::ListValue* alternate_urls = new base::ListValue;
  for (std::vector<std::string>::const_iterator it =
           data.alternate_urls.begin();
       it != data.alternate_urls.end(); ++it) {
    alternate_urls->AppendString(*it);
  }
  url_dict->Set(kAlternateURLs, alternate_urls);

  base::ListValue* encodings = new base::ListValue;
  for (std::vector<std::string>::const_iterator it =
           data.input_encodings.begin();
       it != data.input_encodings.end(); ++it) {
    encodings->AppendString(*it);
  }
  url_dict->Set(kInputEncodings, encodings);

  url_dict->SetString(kSearchTermsReplacementKey,
                      data.search_terms_replacement_key);

  url_dict->SetBoolean(kCreatedByPolicy, data.created_by_policy);

  pref_service_->Set(kDefaultSearchProviderData, *url_dict);
}

void DefaultSearchManager::ClearUserSelectedDefaultSearchEngine() {
  pref_service_->ClearPref(kDefaultSearchProviderData);
}
