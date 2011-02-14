// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_model.h"

#include <algorithm>

#include "unicode/ubidi.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "ui/gfx/rect.h"

///////////////////////////////////////////////////////////////////////////////
// AutocompletePopupModel

AutocompletePopupModel::AutocompletePopupModel(
    AutocompletePopupView* popup_view,
    AutocompleteEditModel* edit_model,
    Profile* profile)
    : view_(popup_view),
      edit_model_(edit_model),
      controller_(new AutocompleteController(profile)),
      profile_(profile),
      hovered_line_(kNoMatch),
      selected_line_(kNoMatch) {
}

AutocompletePopupModel::~AutocompletePopupModel() {
}

void AutocompletePopupModel::SetProfile(Profile* profile) {
  DCHECK(profile);
  profile_ = profile;
  controller_->SetProfile(profile);
}

void AutocompletePopupModel::StartAutocomplete(
    const string16& text,
    const string16& desired_tld,
    bool prevent_inline_autocomplete,
    bool prefer_keyword,
    bool allow_exact_keyword_match) {
  // The user is interacting with the edit, so stop tracking hover.
  SetHoveredLine(kNoMatch);

  manually_selected_match_.Clear();

  controller_->Start(text, desired_tld, prevent_inline_autocomplete,
                     prefer_keyword, allow_exact_keyword_match, false);
}

void AutocompletePopupModel::StopAutocomplete() {
  controller_->Stop(true);
}

bool AutocompletePopupModel::IsOpen() const {
  return view_->IsOpen();
}

void AutocompletePopupModel::SetHoveredLine(size_t line) {
  const bool is_disabling = (line == kNoMatch);
  DCHECK(is_disabling || (line < controller_->result().size()));

  if (line == hovered_line_)
    return;  // Nothing to do

  // Make sure the old hovered line is redrawn.  No need to redraw the selected
  // line since selection overrides hover so the appearance won't change.
  if ((hovered_line_ != kNoMatch) && (hovered_line_ != selected_line_))
    view_->InvalidateLine(hovered_line_);

  // Change the hover to the new line.
  hovered_line_ = line;
  if (!is_disabling && (hovered_line_ != selected_line_))
    view_->InvalidateLine(hovered_line_);
}

void AutocompletePopupModel::SetSelectedLine(size_t line,
                                             bool reset_to_default,
                                             bool force) {
  const AutocompleteResult& result = controller_->result();
  if (result.empty())
    return;

  // Cancel the query so the matches don't change on the user.
  controller_->Stop(false);

  line = std::min(line, result.size() - 1);
  const AutocompleteMatch& match = result.match_at(line);
  if (reset_to_default) {
    manually_selected_match_.Clear();
  } else {
    // Track the user's selection until they cancel it.
    manually_selected_match_.destination_url = match.destination_url;
    manually_selected_match_.provider_affinity = match.provider;
    manually_selected_match_.is_history_what_you_typed_match =
        match.is_history_what_you_typed_match;
  }

  if (line == selected_line_ && !force)
    return;  // Nothing else to do.

  // We need to update |selected_line_| before calling OnPopupDataChanged(), so
  // that when the edit notifies its controller that something has changed, the
  // controller can get the correct updated data.
  //
  // NOTE: We should never reach here with no selected line; the same code that
  // opened the popup and made it possible to get here should have also set a
  // selected line.
  CHECK(selected_line_ != kNoMatch);
  GURL current_destination(result.match_at(selected_line_).destination_url);
  view_->InvalidateLine(selected_line_);
  selected_line_ = line;
  view_->InvalidateLine(selected_line_);

  // Update the edit with the new data for this match.
  // TODO(pkasting): If |selected_line_| moves to the controller, this can be
  // eliminated and just become a call to the observer on the edit.
  string16 keyword;
  const bool is_keyword_hint = GetKeywordForMatch(match, &keyword);
  if (reset_to_default) {
    string16 inline_autocomplete_text;
    if ((match.inline_autocomplete_offset != string16::npos) &&
        (match.inline_autocomplete_offset < match.fill_into_edit.length())) {
      inline_autocomplete_text =
          match.fill_into_edit.substr(match.inline_autocomplete_offset);
    }
    edit_model_->OnPopupDataChanged(inline_autocomplete_text, NULL,
                                    keyword, is_keyword_hint);
  } else {
    edit_model_->OnPopupDataChanged(match.fill_into_edit, &current_destination,
                                    keyword, is_keyword_hint);
  }

  // Repaint old and new selected lines immediately, so that the edit doesn't
  // appear to update [much] faster than the popup.
  view_->PaintUpdatesNow();
}

void AutocompletePopupModel::ResetToDefaultMatch() {
  const AutocompleteResult& result = controller_->result();
  CHECK(!result.empty());
  SetSelectedLine(result.default_match() - result.begin(), true, false);
  view_->OnDragCanceled();
}

void AutocompletePopupModel::InfoForCurrentSelection(
    AutocompleteMatch* match,
    GURL* alternate_nav_url) const {
  DCHECK(match != NULL);
  const AutocompleteResult& result = controller_->result();
  if (!controller_->done()) {
    // It's technically possible for |result| to be empty if no provider returns
    // a synchronous result but the query has not completed synchronously;
    // pratically, however, that should never actually happen.
    if (result.empty())
      return;
    // The user cannot have manually selected a match, or the query would have
    // stopped.  So the default match must be the desired selection.
    *match = *result.default_match();
  } else {
    CHECK(IsOpen());
    // If there are no results, the popup should be closed (so we should have
    // failed the CHECK above), and URLsForDefaultMatch() should have been
    // called instead.
    CHECK(!result.empty());
    CHECK(selected_line_ < result.size());
    *match = result.match_at(selected_line_);
  }
  if (alternate_nav_url && manually_selected_match_.empty())
    *alternate_nav_url = result.alternate_nav_url();
}

bool AutocompletePopupModel::GetKeywordForMatch(const AutocompleteMatch& match,
                                                string16* keyword) const {
  // Assume we have no keyword until we find otherwise.
  keyword->clear();

  // If the current match is a keyword, return that as the selected keyword.
  if (TemplateURL::SupportsReplacement(match.template_url)) {
    keyword->assign(match.template_url->keyword());
    return false;
  }

  // See if the current match's fill_into_edit corresponds to a keyword.
  if (!profile_->GetTemplateURLModel())
    return false;
  profile_->GetTemplateURLModel()->Load();
  const string16 keyword_hint(TemplateURLModel::CleanUserInputKeyword(
      match.fill_into_edit));
  if (keyword_hint.empty())
    return false;

  // Don't provide a hint if this keyword doesn't support replacement.
  const TemplateURL* const template_url =
      profile_->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword_hint);
  if (!TemplateURL::SupportsReplacement(template_url))
    return false;

  // Don't provide a hint for inactive/disabled extension keywords.
  if (template_url->IsExtensionKeyword()) {
    const Extension* extension = profile_->GetExtensionService()->
        GetExtensionById(template_url->GetExtensionId(), false);
    if (!extension ||
        (profile_->IsOffTheRecord() &&
         !profile_->GetExtensionService()->IsIncognitoEnabled(extension)))
      return false;
  }

  keyword->assign(keyword_hint);
  return true;
}

void AutocompletePopupModel::FinalizeInstantQuery(
    const string16& input_text,
    const string16& suggest_text) {
  if (IsOpen()) {
    SearchProvider* search_provider = controller_->search_provider();
    search_provider->FinalizeInstantQuery(input_text, suggest_text);
  }
}

AutocompleteLog* AutocompletePopupModel::GetAutocompleteLog() {
  return new AutocompleteLog(controller_->input().text(),
      controller_->input().type(), selected_line_, 0, controller_->result());
}

void AutocompletePopupModel::Move(int count) {
  const AutocompleteResult& result = controller_->result();
  if (result.empty())
    return;

  // The user is using the keyboard to change the selection, so stop tracking
  // hover.
  SetHoveredLine(kNoMatch);

  // Clamp the new line to [0, result_.count() - 1].
  const size_t new_line = selected_line_ + count;
  SetSelectedLine(((count < 0) && (new_line >= selected_line_)) ? 0 : new_line,
                  false, false);
}

void AutocompletePopupModel::TryDeletingCurrentItem() {
  // We could use InfoForCurrentSelection() here, but it seems better to try
  // and shift-delete the actual selection, rather than any "in progress, not
  // yet visible" one.
  if (selected_line_ == kNoMatch)
    return;

  // Cancel the query so the matches don't change on the user.
  controller_->Stop(false);

  const AutocompleteMatch& match =
      controller_->result().match_at(selected_line_);
  if (match.deletable) {
    const size_t selected_line = selected_line_;
    const bool was_temporary_text = !manually_selected_match_.empty();

    // This will synchronously notify both the edit and us that the results
    // have changed, causing both to revert to the default match.
    controller_->DeleteMatch(match);
    const AutocompleteResult& result = controller_->result();
    if (!result.empty() &&
        (was_temporary_text || selected_line != selected_line_)) {
      // Move the selection to the next choice after the deleted one.
      // SetSelectedLine() will clamp to take care of the case where we deleted
      // the last item.
      // TODO(pkasting): Eventually the controller should take care of this
      // before notifying us, reducing flicker.  At that point the check for
      // deletability can move there too.
      SetSelectedLine(selected_line, false, true);
    }
  }
}

const SkBitmap* AutocompletePopupModel::GetSpecialIconForMatch(
    const AutocompleteMatch& match) const {
  if (!match.template_url || !match.template_url->IsExtensionKeyword())
    return NULL;

  return &profile_->GetExtensionService()->GetOmniboxPopupIcon(
      match.template_url->GetExtensionId());
}

void AutocompletePopupModel::OnResultChanged() {
  const AutocompleteResult& result = controller_->result();
  selected_line_ = result.default_match() == result.end() ?
      kNoMatch : static_cast<size_t>(result.default_match() - result.begin());
  // There had better not be a nonempty result set with no default match.
  CHECK((selected_line_ != kNoMatch) || result.empty());
  manually_selected_match_.Clear();
  // If we're going to trim the window size to no longer include the hovered
  // line, turn hover off.  Practically, this shouldn't happen, but it
  // doesn't hurt to be defensive.
  if ((hovered_line_ != kNoMatch) && (result.size() <= hovered_line_))
    SetHoveredLine(kNoMatch);

  view_->UpdatePopupAppearance();
}
