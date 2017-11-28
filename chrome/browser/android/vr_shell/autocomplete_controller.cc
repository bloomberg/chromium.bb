// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/autocomplete_controller.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/search_engines/util.h"

namespace vr_shell {

AutocompleteController::AutocompleteController(vr::BrowserUiInterface* ui)
    : profile_(ProfileManager::GetActiveUserProfile()), ui_(ui) {
  auto client = base::MakeUnique<ChromeAutocompleteProviderClient>(profile_);
  client_ = client.get();
  autocomplete_controller_ = base::MakeUnique<::AutocompleteController>(
      std::move(client), this,
      AutocompleteClassifier::DefaultOmniboxProviders());
}

AutocompleteController::~AutocompleteController() = default;

void AutocompleteController::Start(const base::string16& text) {
  metrics::OmniboxEventProto::PageClassification page_classification =
      metrics::OmniboxEventProto::OTHER;
  autocomplete_controller_->Start(AutocompleteInput(
      text, page_classification, ChromeAutocompleteSchemeClassifier(profile_)));
}

void AutocompleteController::Stop() {
  autocomplete_controller_->Stop(true);
}

GURL AutocompleteController::GetUrlFromVoiceInput(const base::string16& input) {
  AutocompleteMatch match;
  base::string16 culled_input;
  base::RemoveChars(input, base::ASCIIToUTF16(" "), &culled_input);
  client_->Classify(culled_input, false, false,
                    metrics::OmniboxEventProto::INVALID_SPEC, &match, nullptr);
  if (match.destination_url.is_valid() &&
      (match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
       match.type == AutocompleteMatchType::HISTORY_URL ||
       match.type == AutocompleteMatchType::NAVSUGGEST)) {
    return match.destination_url;
  }
  return GURL(GetDefaultSearchURLForSearchTerms(
      client_->GetTemplateURLService(), input));
}

void AutocompleteController::OnResultChanged(bool default_match_changed) {
  auto suggestions = base::MakeUnique<vr::OmniboxSuggestions>();
  for (const auto& match : autocomplete_controller_->result()) {
    suggestions->suggestions.emplace_back(vr::OmniboxSuggestion(
        match.contents, match.description, match.type, match.destination_url));
  }
  ui_->SetOmniboxSuggestions(std::move(suggestions));
}

}  // namespace vr_shell
