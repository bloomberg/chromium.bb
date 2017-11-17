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

namespace vr {

AutocompleteController::AutocompleteController(BrowserUiInterface* ui)
    : profile_(ProfileManager::GetActiveUserProfile()),
      autocomplete_controller_(base::MakeUnique<::AutocompleteController>(
          base::MakeUnique<ChromeAutocompleteProviderClient>(profile_),
          this,
          AutocompleteClassifier::DefaultOmniboxProviders())),
      ui_(ui) {}

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

void AutocompleteController::OnResultChanged(bool default_match_changed) {
  auto suggestions = base::MakeUnique<OmniboxSuggestions>();
  for (const auto& match : autocomplete_controller_->result()) {
    suggestions->suggestions.emplace_back(OmniboxSuggestion(
        match.contents, match.description, match.type, match.destination_url));
  }
  ui_->SetOmniboxSuggestions(std::move(suggestions));
}

}  // namespace vr
