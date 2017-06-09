// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines the interface class OmniboxView.  Each toolkit will
// implement the edit view differently, so that code is inherently platform
// specific.  However, the OmniboxEditModel needs to do some communication with
// the view.  Since the model is shared between platforms, we need to define an
// interface that all view implementations will share.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_VIEW_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_VIEW_H_

#include <stddef.h>

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"

class GURL;
class OmniboxClient;
class OmniboxEditController;
class OmniboxViewMacTest;
class OmniboxEditModel;

class OmniboxView {
 public:
  // Represents the changes between two State objects.  This is used by the
  // model to determine how its internal state should be updated after the view
  // state changes.  See OmniboxEditModel::OnAfterPossibleChange().
  struct StateChanges {
    // |old_text| and |new_text| are not owned.
    const base::string16* old_text;
    const base::string16* new_text;
    size_t new_sel_start;
    size_t new_sel_end;
    bool selection_differs;
    bool text_differs;
    bool keyword_differs;
    bool just_deleted_text;
  };

  virtual ~OmniboxView();

  // Used by the automation system for getting at the model from the view.
  OmniboxEditModel* model() { return model_.get(); }
  const OmniboxEditModel* model() const { return model_.get(); }

  // Called when any relevant state changes other than changing tabs.
  virtual void Update() = 0;

  // Asks the browser to load the specified match, using the supplied
  // disposition. |alternate_nav_url|, if non-empty, contains the
  // alternate navigation URL for for this match. See comments on
  // AutocompleteResult::GetAlternateNavURL().
  //
  // |pasted_text| should only be set if this call is due to a
  // Paste-And-Go/Search action.
  //
  // |selected_line| is passed to SendOpenNotification(); see comments there.
  //
  // This may close the popup.
  virtual void OpenMatch(const AutocompleteMatch& match,
                         WindowOpenDisposition disposition,
                         const GURL& alternate_nav_url,
                         const base::string16& pasted_text,
                         size_t selected_line);

  // Returns the current text of the edit control, which could be the
  // "temporary" text set by the popup, the "permanent" text set by the
  // browser, or just whatever the user has currently typed.
  virtual base::string16 GetText() const = 0;

  // |true| if the user is in the process of editing the field, or if
  // the field is empty.
  bool IsEditingOrEmpty() const;

  // Returns the vector icon to display as the location icon.
  const gfx::VectorIcon& GetVectorIcon() const;

  // The user text is the text the user has manually keyed in.  When present,
  // this is shown in preference to the permanent text; hitting escape will
  // revert to the permanent text.
  void SetUserText(const base::string16& text);
  virtual void SetUserText(const base::string16& text,
                           bool update_popup);

  // Sets the window text and the caret position. |notify_text_changed| is true
  // if the model should be notified of the change.
  virtual void SetWindowTextAndCaretPos(const base::string16& text,
                                        size_t caret_pos,
                                        bool update_popup,
                                        bool notify_text_changed) = 0;

  // Sets the caret position. Removes any selection. Clamps the requested caret
  // position to the length of the current text.
  virtual void SetCaretPos(size_t caret_pos) = 0;

  // Transitions the user into keyword mode with their default search provider,
  // preserving and selecting the user's text if they already typed in a query.
  virtual void EnterKeywordModeForDefaultSearchProvider() = 0;

  // Returns true if all text is selected or there is no text at all.
  virtual bool IsSelectAll() const = 0;

  // Returns true if the user deleted the suggested text.
  virtual bool DeleteAtEndPressed() = 0;

  // Fills |start| and |end| with the indexes of the current selection's bounds.
  // It is not guaranteed that |*start < *end|, as the selection can be
  // directed.  If there is no selection, |start| and |end| will both be equal
  // to the current cursor position.
  virtual void GetSelectionBounds(size_t* start, size_t* end) const = 0;

  // Selects all the text in the edit.  Use this in place of SetSelAll() to
  // avoid selecting the "phantom newline" at the end of the edit.
  virtual void SelectAll(bool reversed) = 0;

  // Reverts the edit and popup back to their unedited state (permanent text
  // showing, popup closed, no user input in progress).
  virtual void RevertAll();

  // Updates the autocomplete popup and other state after the text has been
  // changed by the user.
  virtual void UpdatePopup() = 0;

  // Closes the autocomplete popup, if it's open. The name |ClosePopup|
  // conflicts with the OSX class override as that has a base class that also
  // defines a method with that name.
  virtual void CloseOmniboxPopup();

  // Sets the focus to the autocomplete view.
  virtual void SetFocus() = 0;

  // Shows or hides the caret based on whether the model's is_caret_visible() is
  // true.
  virtual void ApplyCaretVisibility() = 0;

  // Called when the temporary text in the model may have changed.
  // |display_text| is the new text to show; |save_original_selection| is true
  // when there wasn't previously a temporary text and thus we need to save off
  // the user's existing selection. |notify_text_changed| is true if the model
  // should be notified of the change.
  virtual void OnTemporaryTextMaybeChanged(const base::string16& display_text,
                                           bool save_original_selection,
                                           bool notify_text_changed) = 0;

  // Called when the inline autocomplete text in the model may have changed.
  // |display_text| is the new text to show; |user_text_length| is the length of
  // the user input portion of that (so, up to but not including the inline
  // autocompletion).  Returns whether the display text actually changed.
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const base::string16& display_text, size_t user_text_length) = 0;

  // Called when the inline autocomplete text in the model has been cleared.
  virtual void OnInlineAutocompleteTextCleared() = 0;

  // Called when the temporary text has been reverted by the user.  This will
  // reset the user's original selection.
  virtual void OnRevertTemporaryText() = 0;

  // Checkpoints the current edit state before an operation that might trigger
  // a new autocomplete run to open or modify the popup. Call this before
  // user-initiated edit actions that trigger autocomplete, but *not* for
  // automatic changes to the textfield that should not affect autocomplete.
  virtual void OnBeforePossibleChange() = 0;
  // OnAfterPossibleChange() returns true if there was a change that caused it
  // to call UpdatePopup().  If |allow_keyword_ui_change| is false, we
  // prevent alterations to the keyword UI state (enabled vs. disabled).
  virtual bool OnAfterPossibleChange(bool allow_keyword_ui_change) = 0;

  // Returns the gfx::NativeView of the edit view.
  virtual gfx::NativeView GetNativeView() const = 0;

  // Gets the relative window for the pop up window of OmniboxPopupView. The pop
  // up window will be shown under the relative window. When an IME is attached
  // to the rich edit control, the IME window is the relative window. Otherwise,
  // the top-most window is the relative window.
  virtual gfx::NativeView GetRelativeWindowForPopup() const = 0;

  // Returns the width in pixels needed to display the current text. The
  // returned value includes margins.
  virtual int GetTextWidth() const = 0;

  // Returns the omnibox's width in pixels.
  virtual int GetWidth() const = 0;

  // Returns true if the user is composing something in an IME.
  virtual bool IsImeComposing() const = 0;

  // Returns true if we know for sure that an IME is showing a popup window,
  // which may overlap the omnibox's popup window.
  virtual bool IsImeShowingPopup() const;

  // Display a virtual keybaord or alternate input view if enabled.
  virtual void ShowImeIfNeeded();

  // Returns true if the view is displaying UI that indicates that query
  // refinement will take place when the user selects the current match.  For
  // search matches, this will cause the omnibox to search over the existing
  // corpus (e.g. Images) rather than start a new Web search.  This method will
  // only ever return true on mobile ports.
  virtual bool IsIndicatingQueryRefinement() const;

  // Called after a match has been opened.
  virtual void OnMatchOpened(AutocompleteMatch::Type match_type);

  // Returns |text| with any leading javascript schemas stripped.
  static base::string16 StripJavascriptSchemas(const base::string16& text);

  // First, calls StripJavascriptSchemas().  Then automatically collapses
  // internal whitespace as follows:
  // * If the only whitespace in |text| is newlines, users are most likely
  // pasting in URLs split into multiple lines by terminals, email programs,
  // etc. So all newlines are removed.
  // * Otherwise, users may be pasting in search data, e.g. street addresses. In
  // this case, runs of whitespace are collapsed down to single spaces.
  static base::string16 SanitizeTextForPaste(const base::string16& text);

 protected:
  // Tracks important state that may change between OnBeforePossibleChange() and
  // OnAfterPossibleChange().
  struct State {
    base::string16 text;
    base::string16 keyword;
    bool is_keyword_selected;
    size_t sel_start;
    size_t sel_end;
  };

  OmniboxView(OmniboxEditController* controller,
              std::unique_ptr<OmniboxClient> client);

  // Fills |state| with the current text state.
  void GetState(State* state);

  // Returns the delta between |before| and |after|.
  StateChanges GetStateChanges(const State& before,
                                          const State& after);

  // Internally invoked whenever the text changes in some way.
  virtual void TextChanged();

  // Return the number of characters in the current buffer. The name
  // |GetTextLength| can't be used as the Windows override of this class
  // inherits from a class that defines a method with that name.
  virtual int GetOmniboxTextLength() const = 0;

  // Try to parse the current text as a URL and colorize the components.
  virtual void EmphasizeURLComponents() = 0;

  OmniboxEditController* controller() { return controller_; }
  const OmniboxEditController* controller() const { return controller_; }

  // Marks part (or, if |range| is invalid, all) of the current text as
  // emphasized or de-emphasized, by changing its color.
  virtual void SetEmphasis(bool emphasize, const gfx::Range& range) = 0;

  // Sets the color and strikethrough state for |range|, which represents the
  // current scheme, based on the current security state.  Schemes are displayed
  // in different ways for different security levels.
  virtual void UpdateSchemeStyle(const gfx::Range& range) = 0;

  // Parses |display_text|, then invokes SetEmphasis() and UpdateSchemeStyle()
  // appropriately. If the text is a query string, there is no scheme, and
  // everything is emphasized equally, whereas for URLs the scheme may be styled
  // based on the current security state, with parts of the URL de-emphasized to
  // draw attention to whatever best represents the "identity" of the current
  // URL.
  void UpdateTextStyle(const base::string16& display_text,
                       const AutocompleteSchemeClassifier& classifier);

 private:
  friend class OmniboxViewMacTest;

  // |model_| can be NULL in tests.
  std::unique_ptr<OmniboxEditModel> model_;
  OmniboxEditController* controller_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxView);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_VIEW_H_
