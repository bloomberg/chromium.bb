// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/network_action_predictor/network_action_predictor_dom_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/network_action_predictor.h"
#include "chrome/browser/autocomplete/network_action_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"

NetworkActionPredictorDOMHandler::NetworkActionPredictorDOMHandler(
  Profile* profile) {
  network_action_predictor_ =
      NetworkActionPredictorFactory::GetForProfile(profile);
}

NetworkActionPredictorDOMHandler::~NetworkActionPredictorDOMHandler() { }

void NetworkActionPredictorDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestNetworkActionPredictorDb",
      base::Bind(
          &NetworkActionPredictorDOMHandler::RequestNetworkActionPredictorDb,
          base::Unretained(this)));
}

void NetworkActionPredictorDOMHandler::RequestNetworkActionPredictorDb(
    const base::ListValue* args) {
  const bool enabled = (network_action_predictor_ != NULL);
  base::DictionaryValue dict;
  dict.SetBoolean("enabled", enabled);

  if (enabled) {
    base::ListValue* db = new base::ListValue();
    for (NetworkActionPredictor::DBCacheMap::const_iterator it =
             network_action_predictor_->db_cache_.begin();
         it != network_action_predictor_->db_cache_.end();
         ++it) {
      base::DictionaryValue* entry = new base::DictionaryValue();
      entry->SetString("user_text", it->first.user_text);
      entry->SetString("url", it->first.url.spec());
      entry->SetInteger("hit_count", it->second.number_of_hits);
      entry->SetInteger("miss_count", it->second.number_of_misses);
      entry->SetDouble("confidence",
          network_action_predictor_->CalculateConfidenceForDbEntry(it));
      db->Append(entry);
    }
    dict.Set("db", db);
    dict.SetDouble("hit_weight", NetworkActionPredictor::get_hit_weight());
  }

  web_ui()->CallJavascriptFunction("updateDatabaseTable", dict);
}
