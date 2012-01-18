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
  bool enabled = (network_action_predictor_ != NULL);
  base::ListValue list;

  if (enabled) {
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
      list.Append(entry);
    }
  }

  base::FundamentalValue enabled_value(enabled);
  web_ui()->CallJavascriptFunction("updateDatabaseTable", enabled_value, list);
}
