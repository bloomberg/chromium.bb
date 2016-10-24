// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_popup_model.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_popup_model_observer.h"
#include "components/omnibox/browser/omnibox_popup_view.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "third_party/icu/source/common/unicode/ubidi.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"

using bookmarks::BookmarkModel;

///////////////////////////////////////////////////////////////////////////////
// OmniboxPopupModel

const size_t OmniboxPopupModel::kNoMatch = static_cast<size_t>(-1);

OmniboxPopupModel::OmniboxPopupModel(
    OmniboxPopupView* popup_view,
    OmniboxEditModel* edit_model)
    : view_(popup_view),
      edit_model_(edit_model),
      hovered_line_(kNoMatch),
      selected_line_(kNoMatch),
      selected_line_state_(NORMAL) {
  edit_model->set_popup_model(this);
}

OmniboxPopupModel::~OmniboxPopupModel() {
}

// static
void OmniboxPopupModel::ComputeMatchMaxWidths(int contents_width,
                                              int separator_width,
                                              int description_width,
                                              int available_width,
                                              bool description_on_separate_line,
                                              bool allow_shrinking_contents,
                                              int* contents_max_width,
                                              int* description_max_width) {
  available_width = std::max(available_width, 0);
  *contents_max_width = std::min(contents_width, available_width);
  *description_max_width = std::min(description_width, available_width);

  // If the description is empty, or the contents and description are on
  // separate lines, each can get the full available width.
  if (!description_width || description_on_separate_line)
    return;

  // If we want to display the description, we need to reserve enough space for
  // the separator.
  available_width -= separator_width;
  if (available_width < 0) {
    *description_max_width = 0;
    return;
  }

  if (contents_width + description_width > available_width) {
    if (allow_shrinking_contents) {
      // Try to split the available space fairly between contents and
      // description (if one wants less than half, give it all it wants and
      // give the other the remaining space; otherwise, give each half).
      // However, if this makes the contents too narrow to show a significant
      // amount of information, give the contents more space.
      *contents_max_width = std::max(
          (available_width + 1) / 2, available_width - description_width);

      const int kMinimumContentsWidth = 300;
      *contents_max_width = std::min(
          std::min(std::max(*contents_max_width, kMinimumContentsWidth),
                   contents_width),
          available_width);
    }

    // Give the description the remaining space, unless this makes it too small
    // to display anything meaningful, in which case just hide the description
    // and let the contents take up the whole width.
    *description_max_width =
        std::min(description_width, available_width - *contents_max_width);
    const int kMinimumDescriptionWidth = 75;
    if (*description_max_width <
        std::min(description_width, kMinimumDescriptionWidth)) {
      *description_max_width = 0;
      // Since we're not going to display the description, the contents can have
      // the space we reserved for the separator.
      available_width += separator_width;
      *contents_max_width = std::min(contents_width, available_width);
    }
  }
}

bool OmniboxPopupModel::IsOpen() const {
  return view_->IsOpen();
}

void OmniboxPopupModel::SetHoveredLine(size_t line) {
  const bool is_disabling = (line == kNoMatch);
  DCHECK(is_disabling || (line < result().size()));

  if (line == hovered_line_)
    return;  // Nothing to do

  // We need to update |hovered_line_| before calling InvalidateLine(), since it
  // will check it to determine how to draw.
  const size_t prev_hovered_line = hovered_line_;
  hovered_line_ = line;

  // Make sure the old hovered line is redrawn.  No need to redraw the selected
  // line since selection overrides hover so the appearance won't change.
  if ((prev_hovered_line != kNoMatch) && (prev_hovered_line != selected_line_))
    view_->InvalidateLine(prev_hovered_line);

  if (!is_disabling && (hovered_line_ != selected_line_))
    view_->InvalidateLine(hovered_line_);
}

void OmniboxPopupModel::SetSelectedLine(size_t line,
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
        match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED;
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

  view_->OnLineSelected(selected_line_);

  // Update the edit with the new data for this match.
  // TODO(pkasting): If |selected_line_| moves to the controller, this can be
  // eliminated and just become a call to the observer on the edit.
  base::string16 keyword;
  bool is_keyword_hint;
  TemplateURLService* service = edit_model_->client()->GetTemplateURLService();
  match.GetKeywordUIState(service, &keyword, &is_keyword_hint);

  if (reset_to_default) {
    edit_model_->OnPopupDataChanged(match.inline_autocompletion, NULL,
                                    keyword, is_keyword_hint);
  } else {
    edit_model_->OnPopupDataChanged(match.fill_into_edit, &current_destination,
                                    keyword, is_keyword_hint);
  }

  // Repaint old and new selected lines immediately, so that the edit doesn't
  // appear to update [much] faster than the popup.
  view_->PaintUpdatesNow();
}

void OmniboxPopupModel::ResetToDefaultMatch() {
  const AutocompleteResult& result = this->result();
  CHECK(!result.empty());
  SetSelectedLine(result.default_match() - result.begin(), true, false);
  view_->OnDragCanceled();
}

void OmniboxPopupModel::Move(int count) {
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

void OmniboxPopupModel::SetSelectedLineState(LineState state) {
  DCHECK(!result().empty());
  DCHECK_NE(kNoMatch, selected_line_);

  const AutocompleteMatch& match = result().match_at(selected_line_);
  DCHECK(match.associated_keyword.get());

  selected_line_state_ = state;
  view_->InvalidateLine(selected_line_);
}

void OmniboxPopupModel::TryDeletingCurrentItem() {
  // We could use GetInfoForCurrentText() here, but it seems better to try
  // and shift-delete the actual selection, rather than any "in progress, not
  // yet visible" one.
  if (selected_line_ == kNoMatch)
    return;

  // Cancel the query so the matches don't change on the user.
  autocomplete_controller()->Stop(false);

  const AutocompleteMatch& match = result().match_at(selected_line_);
  if (match.SupportsDeletion()) {
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

gfx::Image OmniboxPopupModel::GetIconIfExtensionMatch(
    const AutocompleteMatch& match) const {
  return edit_model_->client()->GetIconIfExtensionMatch(match);
}

bool OmniboxPopupModel::IsStarredMatch(const AutocompleteMatch& match) const {
  BookmarkModel* bookmark_model = edit_model_->client()->GetBookmarkModel();
  return bookmark_model && bookmark_model->IsBookmarked(match.destination_url);
}

void OmniboxPopupModel::OnResultChanged() {
  answer_bitmap_ = SkBitmap();
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

  bool popup_was_open = view_->IsOpen();
  view_->UpdatePopupAppearance();
  // If popup has just been shown or hidden, notify observers.
  if (view_->IsOpen() != popup_was_open) {
    for (OmniboxPopupModelObserver& observer : observers_)
      observer.OnOmniboxPopupShownOrHidden();
  }
}

void OmniboxPopupModel::AddObserver(OmniboxPopupModelObserver* observer) {
  observers_.AddObserver(observer);
}

void OmniboxPopupModel::RemoveObserver(OmniboxPopupModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void OmniboxPopupModel::SetAnswerBitmap(const SkBitmap& bitmap) {
  answer_bitmap_ = bitmap;
  view_->UpdatePopupAppearance();
}
