// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/predictors/predictors_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

using predictors::AutocompleteActionPredictor;

PredictorsHandler::PredictorsHandler(Profile* profile) {
  autocomplete_action_predictor_ =
      predictors::AutocompleteActionPredictorFactory::GetForProfile(profile);
}

PredictorsHandler::~PredictorsHandler() { }

void PredictorsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestAutocompleteActionPredictorDb",
      base::Bind(&PredictorsHandler::RequestAutocompleteActionPredictorDb,
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
    dict.SetDouble("hit_weight", AutocompleteActionPredictor::get_hit_weight());
  }

  web_ui()->CallJavascriptFunction("updateDatabaseTable", dict);
}
