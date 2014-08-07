// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/common/instant_types.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/search/search.h"
#include "extensions/common/constants.h"
#include "ui/gfx/rect.h"

namespace {

// Returns the AutocompleteMatch that the InstantController should prefetch, if
// any.
//
// The SearchProvider may mark some suggestions to be prefetched based on
// instructions from the suggest server. If such a match ranks sufficiently
// highly or if kAllowPrefetchNonDefaultMatch field trial is enabled, we'll
// return it.
//
// If the kAllowPrefetchNonDefaultMatch field trial is enabled we return the
// prefetch suggestion even if it is not the default match. Otherwise we only
// care about matches that are the default or the very first entry in the
// dropdown (which can happen for non-default matches only if we're hiding a top
// verbatim match) or the second entry in the dropdown (which can happen for
// non-default matches when a top verbatim match is shown); for other matches,
// we think the likelihood of the user selecting them is low enough that
// prefetching isn't worth doing.
const AutocompleteMatch* GetMatchToPrefetch(const AutocompleteResult& result) {
  if (chrome::ShouldAllowPrefetchNonDefaultMatch()) {
    const AutocompleteResult::const_iterator prefetch_match = std::find_if(
        result.begin(), result.end(), SearchProvider::ShouldPrefetch);
    return prefetch_match != result.end() ? &(*prefetch_match) : NULL;
  }

  // If the default match should be prefetched, do that.
  const AutocompleteResult::const_iterator default_match(
      result.default_match());
  if ((default_match != result.end()) &&
      SearchProvider::ShouldPrefetch(*default_match))
    return &(*default_match);

  // Otherwise, if the top match is a verbatim match and the very next match
  // is prefetchable, fetch that.
  if ((result.ShouldHideTopMatch() ||
       result.TopMatchIsStandaloneVerbatimMatch()) &&
      (result.size() > 1) &&
      SearchProvider::ShouldPrefetch(result.match_at(1)))
    return &result.match_at(1);

  return NULL;
}

}  // namespace

OmniboxController::OmniboxController(OmniboxEditModel* omnibox_edit_model,
                                     Profile* profile)
    : omnibox_edit_model_(omnibox_edit_model),
      profile_(profile),
      popup_(NULL),
      autocomplete_controller_(new AutocompleteController(profile,
          TemplateURLServiceFactory::GetForProfile(profile), this,
          AutocompleteClassifier::kDefaultOmniboxProviders)) {
}

OmniboxController::~OmniboxController() {
}

void OmniboxController::StartAutocomplete(
    const AutocompleteInput& input) const {
  ClearPopupKeywordMode();
  popup_->SetHoveredLine(OmniboxPopupModel::kNoMatch);

  // We don't explicitly clear OmniboxPopupModel::manually_selected_match, as
  // Start ends up invoking OmniboxPopupModel::OnResultChanged which clears it.
  autocomplete_controller_->Start(input);
}

void OmniboxController::OnResultChanged(bool default_match_changed) {
  const bool was_open = popup_->IsOpen();
  if (default_match_changed) {
    // The default match has changed, we need to let the OmniboxEditModel know
    // about new inline autocomplete text (blue highlight).
    const AutocompleteResult::const_iterator match(result().default_match());
    if (match != result().end()) {
      current_match_ = *match;
      if (!prerender::IsOmniboxEnabled(profile_))
        DoPreconnect(*match);
      omnibox_edit_model_->OnCurrentMatchChanged();
    } else {
      InvalidateCurrentMatch();
      popup_->OnResultChanged();
      omnibox_edit_model_->OnPopupDataChanged(base::string16(), NULL,
                                              base::string16(), false);
    }
  } else {
    popup_->OnResultChanged();
  }

  if (!popup_->IsOpen() && was_open) {
    // Accept the temporary text as the user text, because it makes little sense
    // to have temporary text when the popup is closed.
    omnibox_edit_model_->AcceptTemporaryTextAsUserText();
  }

  if (chrome::IsInstantExtendedAPIEnabled() &&
     ((default_match_changed && result().default_match() != result().end()) ||
      (chrome::ShouldAllowPrefetchNonDefaultMatch() && !result().empty()))) {
    InstantSuggestion prefetch_suggestion;
    const AutocompleteMatch* match_to_prefetch = GetMatchToPrefetch(result());
    if (match_to_prefetch) {
      prefetch_suggestion.text = match_to_prefetch->contents;
      prefetch_suggestion.metadata =
          SearchProvider::GetSuggestMetadata(*match_to_prefetch);
    }
    // Send the prefetch suggestion unconditionally to the InstantPage. If
    // there is no suggestion to prefetch, we need to send a blank query to
    // clear the prefetched results.
    omnibox_edit_model_->SetSuggestionToPrefetch(prefetch_suggestion);
  }
}

void OmniboxController::InvalidateCurrentMatch() {
  current_match_ = AutocompleteMatch();
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
          predictors::AutocompleteActionPredictor::IsPreconnectable(match));
    }
    // We could prefetch the alternate nav URL, if any, but because there
    // can be many of these as a user types an initial series of characters,
    // the OS DNS cache could suffer eviction problems for minimal gain.
  }
}
