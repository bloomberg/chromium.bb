// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/suggestions_internals/suggestions_internals_ui_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/ntp/suggestions_combiner.h"
#include "content/public/browser/web_ui.h"

namespace {

const size_t kSuggestionsCount = 100;

}  // namespace

SuggestionsInternalsUIHandler::SuggestionsInternalsUIHandler(Profile* profile) {
}

SuggestionsInternalsUIHandler::~SuggestionsInternalsUIHandler() {}

void SuggestionsInternalsUIHandler::OnSuggestionsReady() {
  if (suggestions_combiner_->GetPageValues()) {
    web_ui()->CallJavascriptFunction("suggestionsInternals.setSuggestions",
                                     *suggestions_combiner_->GetPageValues());
  }
}

void SuggestionsInternalsUIHandler::RegisterMessages() {
  // Setup the suggestions sources.
  suggestions_combiner_.reset(SuggestionsCombiner::Create(this));
  suggestions_combiner_->SetSuggestionsCount(kSuggestionsCount);

  web_ui()->RegisterMessageCallback("getSuggestions",
      base::Bind(&SuggestionsInternalsUIHandler::HandleGetSuggestions,
                 base::Unretained(this)));
}

void SuggestionsInternalsUIHandler::HandleGetSuggestions(
    const base::ListValue* one_element_input_string) {
  suggestions_combiner_->FetchItems(Profile::FromWebUI(web_ui()));
}
