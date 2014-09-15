// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/predictors/predictors_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/resource_type.h"

using predictors::AutocompleteActionPredictor;
using predictors::ResourcePrefetchPredictor;
using predictors::ResourcePrefetchPredictorTables;

namespace {

std::string ConvertResourceType(content::ResourceType type) {
  switch (type) {
    case content::RESOURCE_TYPE_IMAGE:
      return "Image";
    case content::RESOURCE_TYPE_STYLESHEET:
      return "Stylesheet";
    case content::RESOURCE_TYPE_SCRIPT:
      return "Script";
    default:
      return "Unknown";
  }
}

}  // namespace

PredictorsHandler::PredictorsHandler(Profile* profile) {
  autocomplete_action_predictor_ =
      predictors::AutocompleteActionPredictorFactory::GetForProfile(profile);
  resource_prefetch_predictor_ =
      predictors::ResourcePrefetchPredictorFactory::GetForProfile(profile);
}

PredictorsHandler::~PredictorsHandler() { }

void PredictorsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestAutocompleteActionPredictorDb",
      base::Bind(&PredictorsHandler::RequestAutocompleteActionPredictorDb,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("requestResourcePrefetchPredictorDb",
      base::Bind(&PredictorsHandler::RequestResourcePrefetchPredictorDb,
                 base::Unretained(this)));
}

void PredictorsHandler::RequestAutocompleteActionPredictorDb(
    const base::ListValue* args) {
  const bool enabled = (autocomplete_action_predictor_ != NULL);
  base::DictionaryValue dict;
  dict.SetBoolean("enabled", enabled);
  if (enabled) {
    base::ListValue* db = new base::ListValue();
    for (AutocompleteActionPredictor::DBCacheMap::const_iterator it =
             autocomplete_action_predictor_->db_cache_.begin();
         it != autocomplete_action_predictor_->db_cache_.end();
         ++it) {
      base::DictionaryValue* entry = new base::DictionaryValue();
      entry->SetString("user_text", it->first.user_text);
      entry->SetString("url", it->first.url.spec());
      entry->SetInteger("hit_count", it->second.number_of_hits);
      entry->SetInteger("miss_count", it->second.number_of_misses);
      entry->SetDouble("confidence",
          autocomplete_action_predictor_->CalculateConfidenceForDbEntry(it));
      db->Append(entry);
    }
    dict.Set("db", db);
  }

  web_ui()->CallJavascriptFunction("updateAutocompleteActionPredictorDb", dict);
}

void PredictorsHandler::RequestResourcePrefetchPredictorDb(
    const base::ListValue* args) {
  const bool enabled = (resource_prefetch_predictor_ != NULL);
  base::DictionaryValue dict;
  dict.SetBoolean("enabled", enabled);

  if (enabled) {
    // Url Database cache.
    base::ListValue* db = new base::ListValue();
    AddPrefetchDataMapToListValue(
        *resource_prefetch_predictor_->url_table_cache_, db);
    dict.Set("url_db", db);

    db = new base::ListValue();
    AddPrefetchDataMapToListValue(
        *resource_prefetch_predictor_->host_table_cache_, db);
    dict.Set("host_db", db);
  }

  web_ui()->CallJavascriptFunction("updateResourcePrefetchPredictorDb", dict);
}

void PredictorsHandler::AddPrefetchDataMapToListValue(
    const ResourcePrefetchPredictor::PrefetchDataMap& data_map,
    base::ListValue* db) const {
  for (ResourcePrefetchPredictor::PrefetchDataMap::const_iterator it =
       data_map.begin(); it != data_map.end(); ++it) {
    base::DictionaryValue* main = new base::DictionaryValue();
    main->SetString("main_frame_url", it->first);
    base::ListValue* resources = new base::ListValue();
    for (ResourcePrefetchPredictor::ResourceRows::const_iterator
         row = it->second.resources.begin();
         row != it->second.resources.end(); ++row) {
      base::DictionaryValue* resource = new base::DictionaryValue();
      resource->SetString("resource_url", row->resource_url.spec());
      resource->SetString("resource_type",
                          ConvertResourceType(row->resource_type));
      resource->SetInteger("number_of_hits", row->number_of_hits);
      resource->SetInteger("number_of_misses", row->number_of_misses);
      resource->SetInteger("consecutive_misses", row->consecutive_misses);
      resource->SetDouble("position", row->average_position);
      resource->SetDouble("score", row->score);
      resources->Append(resource);
    }
    main->Set("resources", resources);
    db->Append(main);
  }
}
