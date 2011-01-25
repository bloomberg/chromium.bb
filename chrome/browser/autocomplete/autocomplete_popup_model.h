// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_MODEL_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_MODEL_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class AutocompleteEditModel;
class AutocompleteEditView;
class Profile;
class SkBitmap;

class AutocompletePopupView;

class AutocompletePopupModel : public NotificationObserver {
 public:
  AutocompletePopupModel(AutocompletePopupView* popup_view,
                         AutocompleteEditModel* edit_model,
                         Profile* profile);
  ~AutocompletePopupModel();

  // Invoked when the profile has changed.
  void SetProfile(Profile* profile);

  // Starts a new query running.  These parameters are passed through to the
  // autocomplete controller; see comments there.
  void StartAutocomplete(const string16& text,
                         const string16& desired_tld,
                         bool prevent_inline_autocomplete,
                         bool prefer_keyword,
                         bool allow_exact_keyword_match);

  // Closes the window and cancels any pending asynchronous queries.
  void StopAutocomplete();

  // Returns true if the popup is currently open.
  bool IsOpen() const;

  AutocompletePopupView* view() const { return view_; }

  // Returns the AutocompleteController used by this popup.
  AutocompleteController* autocomplete_controller() const {
    return controller_.get();
  }

  const AutocompleteResult& result() const {
    return controller_->result();
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

  // Call to change the selected line.  This will update all state and repaint
  // the necessary parts of the window, as well as updating the edit with the
  // new temporary text.  |line| will be clamped to the range of valid lines.
  // |reset_to_default| is true when the selection is being reset back to the
  // default match, and thus there is no temporary text (and no
  // |manually_selected_match_|).
  // NOTE: This assumes the popup is open, and thus both old and new values for
  // the selected line should not be kNoMatch.
  void SetSelectedLine(size_t line, bool reset_to_default);

  // Called when the user hits escape after arrowing around the popup.  This
  // will change the selected line back to the default match and redraw.
  void ResetToDefaultMatch();

  // Copies the selected match into |match|.  If an update is in progress,
  // "selected" means "default in the latest matches".  If there are no matches,
  // does not update |match|.
  //
  // If |alternate_nav_url| is non-NULL, it will be set to the alternate
  // navigation URL for |url| if one exists, or left unchanged otherwise.  See
  // comments on AutocompleteResult::GetAlternateNavURL().
  //
  // TODO(pkasting): When manually_selected_match_ moves to the controller, this
  // can move too.
  void InfoForCurrentSelection(AutocompleteMatch* match,
                               GURL* alternate_nav_url) const;

  // Gets the selected keyword or keyword hint for the given match.  Returns
  // true if |keyword| represents a keyword hint, or false if |keyword|
  // represents a selected keyword.  (|keyword| will always be set [though
  // possibly to the empty string], and you cannot have both a selected keyword
  // and a keyword hint simultaneously.)
  bool GetKeywordForMatch(const AutocompleteMatch& match,
                          string16* keyword) const;

  // Calls through to SearchProvider::FinalizeInstantQuery.
  void FinalizeInstantQuery(const string16& input_text,
                            const string16& suggest_text);

  // Returns a pointer to a heap-allocated AutocompleteLog containing the
  // current input text, selected match, and result set.  The caller is
  // responsible for deleting the object.
  AutocompleteLog* GetAutocompleteLog();

  // Immediately updates and opens the popup if necessary, then moves the
  // current selection down (|count| > 0) or up (|count| < 0), clamping to the
  // first or last result if necessary.  If |count| == 0, the selection will be
  // unchanged, but the popup will still redraw and modify the text in the
  // AutocompleteEditModel.
  void Move(int count);

  // Called when the user hits shift-delete.  This should determine if the item
  // can be removed from history, and if so, remove it and update the popup.
  void TryDeletingCurrentItem();

  // Returns the special icon to use for a given match, or NULL if we should
  // use a standard style icon.
  const SkBitmap* GetSpecialIconForMatch(const AutocompleteMatch& match) const;

  Profile* profile() const { return profile_; }

  // The token value for selected_line_, hover_line_ and functions dealing with
  // a "line number" that indicates "no line".
  static const size_t kNoMatch = -1;

 private:
  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  AutocompletePopupView* view_;

  AutocompleteEditModel* edit_model_;
  scoped_ptr<AutocompleteController> controller_;

  NotificationRegistrar registrar_;

  // Profile for current tab.
  Profile* profile_;

  // The line that's currently hovered.  If we're not drawing a hover rect,
  // this will be kNoMatch, even if the cursor is over the popup contents.
  size_t hovered_line_;

  // The currently selected line.  This is kNoMatch when nothing is selected,
  // which should only be true when the popup is closed.
  size_t selected_line_;

  // The match the user has manually chosen, if any.
  AutocompleteResult::Selection manually_selected_match_;

  DISALLOW_COPY_AND_ASSIGN(AutocompletePopupModel);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_POPUP_MODEL_H_
