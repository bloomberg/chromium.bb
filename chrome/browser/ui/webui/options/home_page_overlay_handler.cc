// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/home_page_overlay_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "content/public/browser/web_ui.h"

namespace options {

HomePageOverlayHandler::HomePageOverlayHandler() {
}

HomePageOverlayHandler::~HomePageOverlayHandler() {
}

void HomePageOverlayHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestAutocompleteSuggestionsForHomePage",
      base::Bind(&HomePageOverlayHandler::RequestAutocompleteSuggestions,
                 base::Unretained(this)));
}

void HomePageOverlayHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  autocomplete_controller_.reset(new AutocompleteController(
      base::MakeUnique<ChromeAutocompleteProviderClient>(profile), this,
      AutocompleteClassifier::DefaultOmniboxProviders()));
}

void HomePageOverlayHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  RegisterTitle(localized_strings, "homePageOverlay",
                IDS_OPTIONS_HOMEPAGE_TITLE);
}

void HomePageOverlayHandler::RequestAutocompleteSuggestions(
    const base::ListValue* args) {
  base::string16 input;
  CHECK_EQ(args->GetSize(), 1U);
  CHECK(args->GetString(0, &input));

  autocomplete_controller_->Start(AutocompleteInput(
      input, base::string16::npos, std::string(), GURL(), base::string16(),
      metrics::OmniboxEventProto::INVALID_SPEC, true, false, false, true, false,
      ChromeAutocompleteSchemeClassifier(Profile::FromWebUI(web_ui()))));
}

void HomePageOverlayHandler::OnResultChanged(bool default_match_changed) {
  const AutocompleteResult& result = autocomplete_controller_->result();
  base::ListValue suggestions;
  OptionsUI::ProcessAutocompleteSuggestions(result, &suggestions);
  web_ui()->CallJavascriptFunctionUnsafe(
      "HomePageOverlay.updateAutocompleteSuggestions", suggestions);
}

}  // namespace options
