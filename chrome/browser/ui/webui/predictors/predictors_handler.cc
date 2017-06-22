// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/predictors/predictors_handler.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/resource_prefetch_predictor_tables.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/resource_type.h"

using predictors::AutocompleteActionPredictor;
using predictors::ResourcePrefetchPredictor;
using predictors::ResourcePrefetchPredictorTables;

namespace {

using predictors::ResourceData;

std::string ConvertResourceType(ResourceData::ResourceType type) {
  switch (type) {
    case ResourceData::RESOURCE_TYPE_IMAGE:
      return "Image";
    case ResourceData::RESOURCE_TYPE_STYLESHEET:
      return "Stylesheet";
    case ResourceData::RESOURCE_TYPE_SCRIPT:
      return "Script";
    case ResourceData::RESOURCE_TYPE_FONT_RESOURCE:
      return "Font";
    default:
      return "Unknown";
  }
}

}  // namespace

PredictorsHandler::PredictorsHandler(Profile* profile) {
  autocomplete_action_predictor_ =
      predictors::AutocompleteActionPredictorFactory::GetForProfile(profile);
  loading_predictor_ =
      predictors::LoadingPredictorFactory::GetForProfile(profile);
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
    auto db = base::MakeUnique<base::ListValue>();
    for (AutocompleteActionPredictor::DBCacheMap::const_iterator it =
             autocomplete_action_predictor_->db_cache_.begin();
         it != autocomplete_action_predictor_->db_cache_.end();
         ++it) {
      std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
      entry->SetString("user_text", it->first.user_text);
      entry->SetString("url", it->first.url.spec());
      entry->SetInteger("hit_count", it->second.number_of_hits);
      entry->SetInteger("miss_count", it->second.number_of_misses);
      entry->SetDouble("confidence",
          autocomplete_action_predictor_->CalculateConfidenceForDbEntry(it));
      db->Append(std::move(entry));
    }
    dict.Set("db", std::move(db));
  }

  web_ui()->CallJavascriptFunctionUnsafe("updateAutocompleteActionPredictorDb",
                                         dict);
}

void PredictorsHandler::RequestResourcePrefetchPredictorDb(
    const base::ListValue* args) {
  const bool enabled = (loading_predictor_ != nullptr);
  base::DictionaryValue dict;
  dict.SetBoolean("enabled", enabled);

  if (enabled) {
    auto* resource_prefetch_predictor =
        loading_predictor_->resource_prefetch_predictor();
    // URL table cache.
    auto db = base::MakeUnique<base::ListValue>();
    AddPrefetchDataMapToListValue(
        *resource_prefetch_predictor->url_resource_data_->data_cache_,
        db.get());
    dict.Set("url_db", std::move(db));

    // Host table cache.
    db = base::MakeUnique<base::ListValue>();
    AddPrefetchDataMapToListValue(
        *resource_prefetch_predictor->host_resource_data_->data_cache_,
        db.get());
    dict.Set("host_db", std::move(db));

    // Origin table cache.
    db = base::MakeUnique<base::ListValue>();
    AddOriginDataMapToListValue(
        *resource_prefetch_predictor->origin_data_->data_cache_, db.get());
    dict.Set("origin_db", std::move(db));
  }

  web_ui()->CallJavascriptFunctionUnsafe("updateResourcePrefetchPredictorDb",
                                         dict);
}

void PredictorsHandler::AddPrefetchDataMapToListValue(
    const std::map<std::string, predictors::PrefetchData>& data_map,
    base::ListValue* db) const {
  for (const auto& p : data_map) {
    auto main = base::MakeUnique<base::DictionaryValue>();
    main->SetString("main_frame_url", p.first);
    auto resources = base::MakeUnique<base::ListValue>();
    for (const predictors::ResourceData& r : p.second.resources()) {
      auto resource = base::MakeUnique<base::DictionaryValue>();
      resource->SetString("resource_url", r.resource_url());
      resource->SetString("resource_type",
                          ConvertResourceType(r.resource_type()));
      resource->SetInteger("number_of_hits", r.number_of_hits());
      resource->SetInteger("number_of_misses", r.number_of_misses());
      resource->SetInteger("consecutive_misses", r.consecutive_misses());
      resource->SetDouble("position", r.average_position());
      resource->SetDouble(
          "score", ResourcePrefetchPredictorTables::ComputeResourceScore(r));
      resource->SetBoolean("before_first_contentful_paint",
                           r.before_first_contentful_paint());
      auto* resource_prefetch_predictor =
          loading_predictor_->resource_prefetch_predictor();
      resource->SetBoolean(
          "is_prefetchable",
          resource_prefetch_predictor->IsResourcePrefetchable(r));
      resources->Append(std::move(resource));
    }
    main->Set("resources", std::move(resources));
    db->Append(std::move(main));
  }
}

void PredictorsHandler::AddOriginDataMapToListValue(
    const std::map<std::string, predictors::OriginData>& data_map,
    base::ListValue* db) const {
  for (const auto& p : data_map) {
    auto main = base::MakeUnique<base::DictionaryValue>();
    main->SetString("main_frame_host", p.first);
    auto origins = base::MakeUnique<base::ListValue>();
    for (const predictors::OriginStat& o : p.second.origins()) {
      auto origin = base::MakeUnique<base::DictionaryValue>();
      origin->SetString("origin", o.origin());
      origin->SetInteger("number_of_hits", o.number_of_hits());
      origin->SetInteger("number_of_misses", o.number_of_misses());
      origin->SetInteger("consecutive_misses", o.consecutive_misses());
      origin->SetDouble("position", o.average_position());
      origin->SetBoolean("always_access_network", o.always_access_network());
      origin->SetBoolean("accessed_network", o.accessed_network());
      origin->SetDouble("score",
                        ResourcePrefetchPredictorTables::ComputeOriginScore(o));
      origins->Append(std::move(origin));
    }
    main->Set("origins", std::move(origins));
    db->Append(std::move(main));
  }
}
