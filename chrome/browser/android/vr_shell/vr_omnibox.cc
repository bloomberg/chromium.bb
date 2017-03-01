// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_omnibox.h"

#include <string>
#include <utility>

#include "base/strings/string16.h"
#include "chrome/browser/android/vr_shell/ui_interface.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_input.h"

namespace vr_shell {

VrOmnibox::VrOmnibox(UiInterface* ui)
    : ui_(ui),
      profile_(ProfileManager::GetLastUsedProfile()),
      autocomplete_controller_(base::MakeUnique<AutocompleteController>(
          base::MakeUnique<ChromeAutocompleteProviderClient>(profile_),
          this,
          AutocompleteClassifier::DefaultOmniboxProviders())) {}

VrOmnibox::~VrOmnibox() = default;

void VrOmnibox::HandleInput(const base::DictionaryValue& dict) {
  base::string16 text;
  CHECK(dict.GetString("text", &text));

  // TODO(crbug.com/683344): Scrub and appropriately tune these parameters.
  GURL current_url;
  size_t cursor_pos = base::string16::npos;
  std::string desired_tld;
  metrics::OmniboxEventProto::PageClassification page_classification =
      metrics::OmniboxEventProto::OTHER;
  bool prevent_inline_autocomplete = false;
  bool prefer_keyword = false;
  bool allow_exact_keyword_match = false;
  bool want_asynchronous_matches = true;
  bool from_omnibox_focus = false;

  autocomplete_controller_->Start(AutocompleteInput(
      text, cursor_pos, desired_tld, current_url, page_classification,
      prevent_inline_autocomplete, prefer_keyword, allow_exact_keyword_match,
      want_asynchronous_matches, from_omnibox_focus,
      ChromeAutocompleteSchemeClassifier(profile_)));
}

void VrOmnibox::OnResultChanged(bool default_match_changed) {
  const AutocompleteResult& result = autocomplete_controller_->result();
  auto suggestions = base::MakeUnique<base::ListValue>();

  for (const AutocompleteMatch& match : result) {
    auto entry = base::MakeUnique<base::DictionaryValue>();
    entry->SetString("description", match.contents);
    entry->SetString("url", match.destination_url.spec());
    suggestions->Append(std::move(entry));
  }

  ui_->SetOmniboxSuggestions(std::move(suggestions));
}

}  // namespace vr_shell
