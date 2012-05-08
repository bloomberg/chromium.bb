// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "ui/gfx/rect.h"

///////////////////////////////////////////////////////////////////////////////
// AutocompletePopupModel

const size_t AutocompletePopupModel::kNoMatch = -1;

AutocompletePopupModel::AutocompletePopupModel(
    AutocompletePopupView* popup_view,
    AutocompleteEditModel* edit_model)
    : view_(popup_view),
      edit_model_(edit_model),
      hovered_line_(kNoMatch),
      selected_line_(kNoMatch),
      selected_line_state_(NORMAL) {
  edit_model->set_popup_model(this);
}

AutocompletePopupModel::~AutocompletePopupModel() {
}

bool AutocompletePopupModel::IsOpen() const {
  return view_->IsOpen();
}

void AutocompletePopupModel::SetHoveredLine(size_t line) {
  const bool is_disabling = (line == kNoMatch);
  DCHECK(is_disabling || (line < result().size()));

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
  const AutocompleteResult& result = this->result();
  if (result.empty())
    return;

  // Cancel the query so the matches don't change on the user.
  autocomplete_controller()->Stop(false);

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

  // We need to update |selected_line_state_| and |selected_line_| before
  // calling InvalidateLine(), since it will check them to determine how to
  // draw.  We also need to update |selected_line_| before calling
  // OnPopupDataChanged(), so that when the edit notifies its controller that
  // something has changed, the controller can get the correct updated data.
  //
  // NOTE: We should never reach here with no selected line; the same code that
  // opened the popup and made it possible to get here should have also set a
  // selected line.
  CHECK(selected_line_ != kNoMatch);
  GURL current_destination(result.match_at(selected_line_).destination_url);
  const size_t prev_selected_line = selected_line_;
  selected_line_state_ = NORMAL;
  selected_line_ = line;
  view_->InvalidateLine(prev_selected_line);
  view_->InvalidateLine(selected_line_);

  // Update the edit with the new data for this match.
  // TODO(pkasting): If |selected_line_| moves to the controller, this can be
  // eliminated and just become a call to the observer on the edit.
  string16 keyword;
  bool is_keyword_hint;
  match.GetKeywordUIState(edit_model_->profile(), &keyword, &is_keyword_hint);

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
  const AutocompleteResult& result = this->result();
  CHECK(!result.empty());
  SetSelectedLine(result.default_match() - result.begin(), true, false);
  view_->OnDragCanceled();
}

void AutocompletePopupModel::Move(int count) {
  const AutocompleteResult& result = this->result();
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

void AutocompletePopupModel::SetSelectedLineState(LineState state) {
  DCHECK(!result().empty());
  DCHECK_NE(kNoMatch, selected_line_);

  const AutocompleteMatch& match = result().match_at(selected_line_);
  DCHECK(match.associated_keyword.get());

  selected_line_state_ = state;
  view_->InvalidateLine(selected_line_);
}

void AutocompletePopupModel::TryDeletingCurrentItem() {
  // We could use InfoForCurrentSelection() here, but it seems better to try
  // and shift-delete the actual selection, rather than any "in progress, not
  // yet visible" one.
  if (selected_line_ == kNoMatch)
    return;

  // Cancel the query so the matches don't change on the user.
  autocomplete_controller()->Stop(false);

  const AutocompleteMatch& match = result().match_at(selected_line_);
  if (match.deletable) {
    const size_t selected_line = selected_line_;
    const bool was_temporary_text = !manually_selected_match_.empty();

    // This will synchronously notify both the edit and us that the results
    // have changed, causing both to revert to the default match.
    autocomplete_controller()->DeleteMatch(match);
    const AutocompleteResult& result = this->result();
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

const SkBitmap* AutocompletePopupModel::GetIconIfExtensionMatch(
    const AutocompleteMatch& match) const {
  Profile* profile = edit_model_->profile();
  const TemplateURL* template_url = match.GetTemplateURL(profile);
  return (template_url && template_url->IsExtensionKeyword()) ?
      &profile->GetExtensionService()->GetOmniboxPopupIcon(
          template_url->GetExtensionId()) : NULL;
}

void AutocompletePopupModel::OnResultChanged() {
  const AutocompleteResult& result = this->result();
  selected_line_ = result.default_match() == result.end() ?
      kNoMatch : static_cast<size_t>(result.default_match() - result.begin());
  // There had better not be a nonempty result set with no default match.
  CHECK((selected_line_ != kNoMatch) || result.empty());
  manually_selected_match_.Clear();
  selected_line_state_ = NORMAL;
  // If we're going to trim the window size to no longer include the hovered
  // line, turn hover off.  Practically, this shouldn't happen, but it
  // doesn't hurt to be defensive.
  if ((hovered_line_ != kNoMatch) && (result.size() <= hovered_line_))
    SetHoveredLine(kNoMatch);

  view_->UpdatePopupAppearance();
}
