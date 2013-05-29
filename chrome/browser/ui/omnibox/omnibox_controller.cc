// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/search_provider.h"
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

void OmniboxController::StartAutocomplete(
    string16 user_text,
    size_t cursor_position,
    bool prevent_inline_autocomplete,
    bool prefer_keyword,
    bool allow_exact_keyword_match) const {
  ClearPopupKeywordMode();
  popup_->SetHoveredLine(OmniboxPopupModel::kNoMatch);

  InstantController* instant_controller = GetInstantController();
  if (instant_controller) {
    instant_controller->OnAutocompleteStart();
    // If the embedded page for InstantExtended is fetching its own suggestions,
    // suppress search suggestions from SearchProvider. We still need
    // SearchProvider to run for FinalizeInstantQuery.
    // TODO(dcblack): Once we are done refactoring the omnibox so we don't need
    // to use FinalizeInstantQuery anymore, we can take out this check and
    // remove this provider from kInstantExtendedOmniboxProviders.
    if (instant_controller->WillFetchCompletions())
      autocomplete_controller_->search_provider()->SuppressSearchSuggestions();
  }

  // We don't explicitly clear OmniboxPopupModel::manually_selected_match, as
  // Start ends up invoking OmniboxPopupModel::OnResultChanged which clears it.
  autocomplete_controller_->Start(AutocompleteInput(
      user_text, cursor_position, string16(), GURL(),
      prevent_inline_autocomplete, prefer_keyword, allow_exact_keyword_match,
      AutocompleteInput::ALL_MATCHES));
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

    InstantController* instant_controller = GetInstantController();
    if (instant_controller && !omnibox_edit_model_->in_revert()) {
      instant_controller->HandleAutocompleteResults(
          *autocomplete_controller_->providers(),
          autocomplete_controller_->result());
    }
  } else if (was_open) {
    // Accept the temporary text as the user text, because it makes little sense
    // to have temporary text when the popup is closed.
    omnibox_edit_model_->AcceptTemporaryTextAsUserText();
    // The popup has been closed, let instant know.
    OnPopupBoundsChanged(gfx::Rect());
  }
}

bool OmniboxController::DoInstant(const AutocompleteMatch& match,
                                  string16 user_text,
                                  string16 full_text,
                                  size_t selection_start,
                                  size_t selection_end,
                                  bool user_input_in_progress,
                                  bool in_escape_handler,
                                  bool just_deleted_text,
                                  bool keyword_is_selected) {
  InstantController* instant_controller = GetInstantController();
  if (!instant_controller)
    return false;

  // Remove "?" if we're in forced query mode.
  AutocompleteInput::RemoveForcedQueryStringIfNecessary(
      autocomplete_controller_->input().type(), &user_text);
  AutocompleteInput::RemoveForcedQueryStringIfNecessary(
      autocomplete_controller_->input().type(), &full_text);
  return instant_controller->Update(
      match, user_text, full_text, selection_start, selection_end,
      UseVerbatimInstant(just_deleted_text), user_input_in_progress,
      popup_->IsOpen(), in_escape_handler, keyword_is_selected);
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
  InstantController* instant_controller = GetInstantController();
  if (instant_controller)
    instant_controller->SetPopupBounds(bounds);
}

bool OmniboxController::UseVerbatimInstant(bool just_deleted_text) const {
#if defined(OS_MACOSX)
  // TODO(suzhe): Fix Mac port to display Instant suggest in a separated NSView,
  // so that we can display Instant suggest along with composition text.
  const AutocompleteInput& input = autocomplete_controller_->input();
  if (input.prevent_inline_autocomplete())
    return true;
#endif

  // The value of input.prevent_inline_autocomplete() is determined by the
  // following conditions:
  // 1. If the caret is at the end of the text.
  // 2. If it's in IME composition mode.
  // We send the caret position to Instant (so it can determine #1 itself), and
  // we use a separated widget for displaying the Instant suggest (so it doesn't
  // interfere with #2). So, we don't need to care about the value of
  // input.prevent_inline_autocomplete() here.
  return just_deleted_text || popup_->selected_line() != 0;
}

InstantController* OmniboxController::GetInstantController() const {
  return omnibox_edit_model_->GetInstantController();
}
