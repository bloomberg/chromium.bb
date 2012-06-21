// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MODEL_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MODEL_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"

class OmniboxPopupView;
class SkBitmap;

class OmniboxPopupModel {
 public:
  // See selected_line_state_ for details.
  enum LineState {
    NORMAL = 0,
    KEYWORD
  };

  OmniboxPopupModel(OmniboxPopupView* popup_view,
                    AutocompleteEditModel* edit_model);
  ~OmniboxPopupModel();

  // Returns true if the popup is currently open.
  bool IsOpen() const;

  OmniboxPopupView* view() const { return view_; }

  // Returns the AutocompleteController used by this popup.
  AutocompleteController* autocomplete_controller() const {
    return edit_model_->autocomplete_controller();
  }

  const AutocompleteResult& result() const {
    return autocomplete_controller()->result();
  }

  size_t hovered_line() const {
    return hovered_line_;
  }

  // Call to change the hovered line.  |line| should be within the range of
  // valid lines (to enable hover) or kNoMatch (to disable hover).
  void SetHoveredLine(size_t line);

  size_t selected_line() const {
    return selected_line_;
  }

  LineState selected_line_state() const {
    return selected_line_state_;
  }

  // Call to change the selected line.  This will update all state and repaint
  // the necessary parts of the window, as well as updating the edit with the
  // new temporary text.  |line| will be clamped to the range of valid lines.
  // |reset_to_default| is true when the selection is being reset back to the
  // default match, and thus there is no temporary text (and no
  // |manually_selected_match_|). If |force| is true then the selected line will
  // be updated forcibly even if the |line| is same as the current selected
  // line.
  // NOTE: This assumes the popup is open, and thus both old and new values for
  // the selected line should not be kNoMatch.
  void SetSelectedLine(size_t line, bool reset_to_default, bool force);

  // Called when the user hits escape after arrowing around the popup.  This
  // will change the selected line back to the default match and redraw.
  void ResetToDefaultMatch();

  // Immediately updates and opens the popup if necessary, then moves the
  // current selection down (|count| > 0) or up (|count| < 0), clamping to the
  // first or last result if necessary.  If |count| == 0, the selection will be
  // unchanged, but the popup will still redraw and modify the text in the
  // AutocompleteEditModel.
  void Move(int count);

  // If the selected line has both a normal match and a keyword match, this can
  // be used to choose which to select. It is an error to call this when the
  // selected line does not have both matches (or there is no selection).
  void SetSelectedLineState(LineState state);

  // Called when the user hits shift-delete.  This should determine if the item
  // can be removed from history, and if so, remove it and update the popup.
  void TryDeletingCurrentItem();

  // If |match| is from an extension, returns the extension icon; otherwise
  // returns NULL.
  const SkBitmap* GetIconIfExtensionMatch(const AutocompleteMatch& match) const;

  // The match the user has manually chosen, if any.
  const AutocompleteResult::Selection& manually_selected_match() const {
    return manually_selected_match_;
  }

  // Invoked from the edit model any time the result set of the controller
  // changes.
  void OnResultChanged();

  // The token value for selected_line_, hover_line_ and functions dealing with
  // a "line number" that indicates "no line".
  static const size_t kNoMatch;

 private:
  OmniboxPopupView* view_;

  AutocompleteEditModel* edit_model_;

  // The line that's currently hovered.  If we're not drawing a hover rect,
  // this will be kNoMatch, even if the cursor is over the popup contents.
  size_t hovered_line_;

  // The currently selected line.  This is kNoMatch when nothing is selected,
  // which should only be true when the popup is closed.
  size_t selected_line_;

  // If the selected line has both a normal match and a keyword match, this
  // determines whether the normal match (if NORMAL) or the keyword match
  // (if KEYWORD) is selected.
  LineState selected_line_state_;

  // The match the user has manually chosen, if any.
  AutocompleteResult::Selection manually_selected_match_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxPopupModel);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_MODEL_H_
