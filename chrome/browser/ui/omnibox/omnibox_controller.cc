// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/search/instant_controller.h"
#include "extensions/common/constants.h"
#include "ui/gfx/rect.h"

using predictors::AutocompleteActionPredictor;


OmniboxController::OmniboxController(OmniboxEditModel* omnibox_edit_model,
                                     Profile* profile)
    : omnibox_edit_model_(omnibox_edit_model),
      profile_(profile) {
  autocomplete_controller_.reset(new AutocompleteController(profile, this,
      chrome::IsInstantExtendedAPIEnabled() ?
          AutocompleteClassifier::kInstantExtendedOmniboxProviders :
          AutocompleteClassifier::kDefaultOmniboxProviders));
}

OmniboxController::~OmniboxController() {
}

void OmniboxController::OnResultChanged(bool default_match_changed) {
  // TODO(beaudoin): There should be no need to access the popup when using
  // instant extended, remove this reference.
  const bool was_open = popup_->IsOpen();
  if (default_match_changed) {
    // The default match has changed, we need to let the OmniboxEditModel know
    // about new inline autocomplete text (blue highlight).
    string16 inline_autocomplete_text;
    string16 keyword;
    bool is_keyword_hint = false;
    const AutocompleteResult& result = this->result();
    const AutocompleteResult::const_iterator match(result.default_match());
    if (match != result.end()) {
      if ((match->inline_autocomplete_offset != string16::npos) &&
          (match->inline_autocomplete_offset <
           match->fill_into_edit.length())) {
        inline_autocomplete_text =
            match->fill_into_edit.substr(match->inline_autocomplete_offset);
      }

      if (!prerender::IsOmniboxEnabled(profile_))
        DoPreconnect(*match);

      // We could prefetch the alternate nav URL, if any, but because there
      // can be many of these as a user types an initial series of characters,
      // the OS DNS cache could suffer eviction problems for minimal gain.

      match->GetKeywordUIState(profile_, &keyword, &is_keyword_hint);
    }

    popup_->OnResultChanged();
    omnibox_edit_model_->OnPopupDataChanged(inline_autocomplete_text, NULL,
                                            keyword, is_keyword_hint);
  } else {
    popup_->OnResultChanged();
  }

  if (popup_->IsOpen()) {
    // The popup size may have changed, let instant know.
    OnPopupBoundsChanged(popup_->view()->GetTargetBounds());

    InstantController* instant =
        omnibox_edit_model_->controller()->GetInstant();
    if (instant && !omnibox_edit_model_->in_revert()) {
      instant->HandleAutocompleteResults(
          *autocomplete_controller()->providers(),
          autocomplete_controller()->result());
    }
  } else if (was_open) {
    // Accept the temporary text as the user text, because it makes little sense
    // to have temporary text when the popup is closed.
    omnibox_edit_model_->AcceptTemporaryTextAsUserText();
    // The popup has been closed, let instant know.
    OnPopupBoundsChanged(gfx::Rect());
  }
}

void OmniboxController::ClearPopupKeywordMode() const {
  if (popup_->IsOpen() &&
      popup_->selected_line_state() == OmniboxPopupModel::KEYWORD)
    popup_->SetSelectedLineState(OmniboxPopupModel::NORMAL);
}

void OmniboxController::DoPreconnect(const AutocompleteMatch& match) {
  if (!match.destination_url.SchemeIs(extensions::kExtensionScheme)) {
    // Warm up DNS Prefetch cache, or preconnect to a search service.
    UMA_HISTOGRAM_ENUMERATION("Autocomplete.MatchType", match.type,
                              AutocompleteMatchType::NUM_TYPES);
    if (profile_->GetNetworkPredictor()) {
      profile_->GetNetworkPredictor()->AnticipateOmniboxUrl(
          match.destination_url,
          AutocompleteActionPredictor::IsPreconnectable(match));
    }
    // We could prefetch the alternate nav URL, if any, but because there
    // can be many of these as a user types an initial series of characters,
    // the OS DNS cache could suffer eviction problems for minimal gain.
  }
}

void OmniboxController::OnPopupBoundsChanged(const gfx::Rect& bounds) {
  InstantController* instant = omnibox_edit_model_->controller()->GetInstant();
  if (instant)
    instant->SetPopupBounds(bounds);
}
