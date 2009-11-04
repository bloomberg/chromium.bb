// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_model.h"

#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_popup_view.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/notification_service.h"
#include "third_party/icu/public/common/unicode/ubidi.h"

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
  registrar_.Add(this, NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED,
                 Source<AutocompleteController>(controller_.get()));
}

AutocompletePopupModel::~AutocompletePopupModel() {
  StopAutocomplete();
}

void AutocompletePopupModel::SetProfile(Profile* profile) {
  DCHECK(profile);
  profile_ = profile;
  controller_->SetProfile(profile);
}

void AutocompletePopupModel::StartAutocomplete(
    const std::wstring& text,
    const std::wstring& desired_tld,
    bool prevent_inline_autocomplete,
    bool prefer_keyword) {
  // The user is interacting with the edit, so stop tracking hover.
  SetHoveredLine(kNoMatch);

  manually_selected_match_.Clear();

  controller_->Start(text, desired_tld, prevent_inline_autocomplete,
                     prefer_keyword, false);
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
                                             bool reset_to_default) {
  const AutocompleteResult& result = controller_->result();
  CHECK(line < result.size());
  if (result.empty())
    return;

  // Cancel the query so the matches don't change on the user.
  controller_->Stop(false);

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

  if (line == selected_line_)
    return;  // Nothing else to do.

  // Update the edit with the new data for this match.
  // TODO(pkasting): If |selected_line_| moves to the controller, this can be
  // eliminated and just become a call to the observer on the edit.
  std::wstring keyword;
  const bool is_keyword_hint = GetKeywordForMatch(match, &keyword);

  if (reset_to_default) {
    std::wstring inline_autocomplete_text;
    if ((match.inline_autocomplete_offset != std::wstring::npos) &&
        (match.inline_autocomplete_offset < match.fill_into_edit.length())) {
      inline_autocomplete_text =
          match.fill_into_edit.substr(match.inline_autocomplete_offset);
    }
    edit_model_->OnPopupDataChanged(inline_autocomplete_text, false,
                                    keyword, is_keyword_hint, match.type);
  } else {
    edit_model_->OnPopupDataChanged(match.fill_into_edit, true,
                                    keyword, is_keyword_hint, match.type);
  }

  // Repaint old and new selected lines immediately, so that the edit doesn't
  // appear to update [much] faster than the popup.  We must not update
  // |selected_line_| before calling OnPopupDataChanged() (since the edit may
  // call us back to get data about the old selection), and we must not call
  // UpdateWindow() before updating |selected_line_| (since the paint routine
  // relies on knowing the correct selected line).
  //
  // NOTE: We should never reach here with no selected line; the same code that
  // opened the popup and made it possible to get here should have also set a
  // selected line.
  CHECK(selected_line_ != kNoMatch);
  view_->InvalidateLine(selected_line_);
  selected_line_ = line;
  view_->InvalidateLine(selected_line_);
  view_->PaintUpdatesNow();
}

void AutocompletePopupModel::ResetToDefaultMatch() {
  const AutocompleteResult& result = controller_->result();
  CHECK(!result.empty());
  SetSelectedLine(result.default_match() - result.begin(), true);
}

GURL AutocompletePopupModel::URLsForCurrentSelection(
    PageTransition::Type* transition,
    bool* is_history_what_you_typed_match,
    GURL* alternate_nav_url) const {
  const AutocompleteResult* result;
  AutocompleteResult::const_iterator match;
  if (!controller_->done()) {
    result = &controller_->latest_result();
    // It's technically possible for |result| to be empty if no provider returns
    // a synchronous result but the query has not completed synchronously;
    // pratically, however, that should never actually happen.
    if (result->empty())
      return GURL();
    // The user cannot have manually selected a match, or the query would have
    // stopped.  So the default match must be the desired selection.
    match = result->default_match();
  } else {
    CHECK(IsOpen());
    // The query isn't running, so the standard result set can't possibly be out
    // of date.
    //
    // NOTE: In practice, it should actually be safe to use
    // controller_->latest_result() here too, since the controller keeps that
    // up-to-date.  However we generally try to avoid referring to that.
    result = &controller_->result();
    // If there are no results, the popup should be closed (so we should have
    // failed the CHECK above), and URLsForDefaultMatch() should have been
    // called instead.
    if (result->empty()) {
      // We're going to checkfail, but first see whether
      // controller_->latest_result() is actually in sync with |result|.
      CHECK(controller_->latest_result().empty());
      CHECK(false);
    }
    CHECK(selected_line_ < result->size());
    match = result->begin() + selected_line_;
  }
  if (transition)
    *transition = match->transition;
  if (is_history_what_you_typed_match)
    *is_history_what_you_typed_match = match->is_history_what_you_typed_match;
  if (alternate_nav_url && manually_selected_match_.empty())
    *alternate_nav_url = result->alternate_nav_url();
  return match->destination_url;
}

bool AutocompletePopupModel::GetKeywordForMatch(const AutocompleteMatch& match,
                                                std::wstring* keyword) const {
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
  const std::wstring keyword_hint(
      TemplateURLModel::CleanUserInputKeyword(match.fill_into_edit));
  if (keyword_hint.empty())
    return false;

  // Don't provide a hint if this keyword doesn't support replacement.
  const TemplateURL* const template_url =
      profile_->GetTemplateURLModel()->GetTemplateURLForKeyword(keyword_hint);
  if (!TemplateURL::SupportsReplacement(template_url))
    return false;

  keyword->assign(keyword_hint);
  return true;
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
  SetSelectedLine((((count < 0) && (new_line >= selected_line_)) ?
      0 : std::min(new_line, result.size() - 1)), false);
}

void AutocompletePopupModel::TryDeletingCurrentItem() {
  // We could use URLsForCurrentSelection() here, but it seems better to try
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
    controller_->DeleteMatch(match);  // This may synchronously notify us that
                                      // the results have changed.
    const AutocompleteResult& result = controller_->result();
    if (!result.empty()) {
      // Move the selection to the next choice after the deleted one.
      // TODO(pkasting): Eventually the controller should take care of this
      // before notifying us, reducing flicker.  At that point the check for
      // deletability can move there too.
      SetSelectedLine(std::min(result.size() - 1, selected_line), false);
    }
  }
}

void AutocompletePopupModel::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK_EQ(NotificationType::AUTOCOMPLETE_CONTROLLER_RESULT_UPDATED,
            type.value);

  const AutocompleteResult* result =
      Details<const AutocompleteResult>(details).ptr();
  selected_line_ = (result->default_match() == result->end()) ?
      kNoMatch : (result->default_match() - result->begin());
  // There had better not be a nonempty result set with no default match.
  CHECK((selected_line_ != kNoMatch) || result->empty());
  // If we're going to trim the window size to no longer include the hovered
  // line, turn hover off.  Practically, this shouldn't happen, but it
  // doesn't hurt to be defensive.
  if ((hovered_line_ != kNoMatch) && (result->size() <= hovered_line_))
    SetHoveredLine(kNoMatch);

  view_->UpdatePopupAppearance();
}
