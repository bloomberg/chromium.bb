// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_MODEL_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_MODEL_H_

#include <stddef.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/browser/autocomplete_controller_delegate.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

class AutocompleteController;
class AutocompleteResult;
class OmniboxClient;
class OmniboxEditController;
class OmniboxPopupModel;
class OmniboxView;

namespace gfx {
class Image;
class Rect;
}

// Reasons why the Omnibox could change into keyword mode.
// These numeric values are used in UMA logs; do not change them.
enum EnteredKeywordModeMethod {
  ENTERED_KEYWORD_MODE_VIA_TAB = 0,
  ENTERED_KEYWORD_MODE_VIA_SPACE_AT_END = 1,
  ENTERED_KEYWORD_MODE_VIA_SPACE_IN_MIDDLE = 2,
  ENTERED_KEYWORD_MODE_NUM_ITEMS
};

class OmniboxEditModel {
 public:
  // Did the Omnibox focus originate via the user clicking on the Omnibox or on
  // the Fakebox?
  enum FocusSource {
    INVALID = 0,
    OMNIBOX = 1,
    FAKEBOX = 2
  };

  struct State {
    State(bool user_input_in_progress,
          const base::string16& user_text,
          const base::string16& gray_text,
          const base::string16& keyword,
          bool is_keyword_hint,
          bool url_replacement_enabled,
          OmniboxFocusState focus_state,
          FocusSource focus_source,
          const AutocompleteInput& autocomplete_input);
    State(const State& other);
    ~State();

    bool user_input_in_progress;
    const base::string16 user_text;
    const base::string16 gray_text;
    const base::string16 keyword;
    const bool is_keyword_hint;
    bool url_replacement_enabled;
    OmniboxFocusState focus_state;
    FocusSource focus_source;
    const AutocompleteInput autocomplete_input;
  };

  OmniboxEditModel(OmniboxView* view,
                   OmniboxEditController* controller,
                   std::unique_ptr<OmniboxClient> client);
  virtual ~OmniboxEditModel();

  // TODO(beaudoin): Remove this accessor when the AutocompleteController has
  //     completely moved to OmniboxController.
  AutocompleteController* autocomplete_controller() const {
    return omnibox_controller_->autocomplete_controller();
  }

  void set_popup_model(OmniboxPopupModel* popup_model) {
    omnibox_controller_->set_popup_model(popup_model);
  }

  // TODO: The edit and popup should be siblings owned by the LocationBarView,
  // making this accessor unnecessary.
  // NOTE: popup_model() can be NULL for testing.
  OmniboxPopupModel* popup_model() const {
    return omnibox_controller_->popup_model();
  }

  OmniboxEditController* controller() const { return controller_; }

  OmniboxClient* client() { return client_.get(); }

  // Returns the current state.  This assumes we are switching tabs, and changes
  // the internal state appropriately.
  const State GetStateForTabSwitch();

  // Resets the tab state, then restores local state from the saved |state|.
  // |state| may be NULL if there is no saved state.
  void RestoreState(const State* state);

  // Returns the match for the current text. If the user has not edited the text
  // this is the match corresponding to the permanent text. Returns the
  // alternate nav URL, if |alternate_nav_url| is non-NULL and there is such a
  // URL.
  AutocompleteMatch CurrentMatch(GURL* alternate_nav_url) const;

  // Called when the user wants to export the entire current text as a URL.
  // Sets the url, and if known, the title and favicon.
  void GetDataForURLExport(GURL* url,
                           base::string16* title,
                           gfx::Image* favicon);

  // Returns true if the current edit contents will be treated as a
  // URL/navigation, as opposed to a search.
  bool CurrentTextIsURL() const;

  // Returns the match type for the current edit contents.
  AutocompleteMatch::Type CurrentTextType() const;

  // Invoked to adjust the text before writting to the clipboard for a copy
  // (e.g. by adding 'http' to the front). |sel_min| gives the minimum position
  // of the selection e.g. min(selection_start, selection_end). |text| is the
  // currently selected text. If |is_all_selected| is true all the text in the
  // edit is selected. If the url should be copied to the clipboard |write_url|
  // is set to true and |url| set to the url to write.
  void AdjustTextForCopy(int sel_min,
                         bool is_all_selected,
                         base::string16* text,
                         GURL* url,
                         bool* write_url);

  bool user_input_in_progress() const { return user_input_in_progress_; }

  // Sets the state of user_input_in_progress_, and notifies the observer if
  // that state has changed.
  void SetInputInProgress(bool in_progress);

  // Updates permanent_text_ to the current permanent text from the toolbar
  // model.  Returns true if the permanent text changed and the change should be
  // immediately user-visible, because either the user is not editing or the
  // edit does not have focus.
  bool UpdatePermanentText();

  // Returns the URL corresponding to the permanent text.
  GURL PermanentURL();

  // Sets the user_text_ to |text|.  Only the View should call this.
  void SetUserText(const base::string16& text);

  // Commits the gray suggested text as if it's been input by the user.
  // Returns true if the text was committed.
  // TODO: can the return type be void?
  bool CommitSuggestedText();

  // Invoked any time the text may have changed in the edit. Notifies the
  // controller.
  void OnChanged();

  // Reverts the edit model back to its unedited state (permanent text showing,
  // no user input in progress).
  void Revert();

  // Directs the popup to start autocomplete.
  void StartAutocomplete(bool has_selected_text,
                         bool prevent_inline_autocomplete,
                         bool entering_keyword_mode);

  // Closes the popup and cancels any pending asynchronous queries.
  void StopAutocomplete();

  // Determines whether the user can "paste and go", given the specified text.
  bool CanPasteAndGo(const base::string16& text) const;

  // Navigates to the destination last supplied to CanPasteAndGo.
  void PasteAndGo(const base::string16& text);

  // Returns true if this is a paste-and-search rather than paste-and-go (or
  // nothing).
  bool IsPasteAndSearch(const base::string16& text) const;

  // Asks the browser to load the popup's currently selected item, using the
  // supplied disposition.  This may close the popup. If |for_drop| is true,
  // it indicates the input is being accepted as part of a drop operation and
  // the transition should be treated as LINK (so that it won't trigger the
  // URL to be autocompleted).
  void AcceptInput(WindowOpenDisposition disposition,
                   bool for_drop);

  // Asks the browser to load the item at |index|, with the given properties.
  // OpenMatch() needs to know the original text that drove this action.  If
  // |pasted_text| is non-empty, this is a Paste-And-Go/Search action, and
  // that's the relevant input text.  Otherwise, the relevant input text is
  // either the user text or the permanent text, depending on if user input is
  // in progress.
  //
  // |match| is passed by value for two reasons:
  // (1) This function needs to modify |match|, so a const ref isn't
  //     appropriate.  Callers don't actually care about the modifications, so a
  //     pointer isn't required.
  // (2) The passed-in match is, on the caller side, typically coming from data
  //     associated with the popup.  Since this call can close the popup, that
  //     could clear that data, leaving us with a pointer-to-garbage.  So at
  //     some point someone needs to make a copy of the match anyway, to
  //     preserve it past the popup closure.
  void OpenMatch(AutocompleteMatch match,
                 WindowOpenDisposition disposition,
                 const GURL& alternate_nav_url,
                 const base::string16& pasted_text,
                 size_t index);

  OmniboxFocusState focus_state() const { return focus_state_; }
  bool has_focus() const { return focus_state_ != OMNIBOX_FOCUS_NONE; }
  bool is_caret_visible() const {
    return focus_state_ == OMNIBOX_FOCUS_VISIBLE;
  }

  // Accessors for keyword-related state (see comments on keyword_ and
  // is_keyword_hint_).
  const base::string16& keyword() const { return keyword_; }
  bool is_keyword_hint() const { return is_keyword_hint_; }
  bool is_keyword_selected() const {
    return !is_keyword_hint_ && !keyword_.empty();
  }

  // Accepts the current keyword hint as a keyword. It always returns true for
  // caller convenience. |entered_method| indicates how the use entered
  // keyword mode. This parameter is only used for metrics/logging; it's not
  // used to change user-visible behavior.
  bool AcceptKeyword(EnteredKeywordModeMethod entered_method);

  // Accepts the current temporary text as the user text.
  void AcceptTemporaryTextAsUserText();

  // Clears the current keyword.
  void ClearKeyword();

  // Returns the current autocomplete result.  This logic should in the future
  // live in AutocompleteController but resides here for now.  This method is
  // used by AutomationProvider::AutocompleteEditGetMatches.
  const AutocompleteResult& result() const {
    return omnibox_controller_->result();
  }

  // Called when the view is gaining focus.  |control_down| is whether the
  // control key is down (at the time we're gaining focus).
  void OnSetFocus(bool control_down);

  // Sets the visibility of the caret in the omnibox, if it has focus. The
  // visibility of the caret is reset to visible if either
  //   - The user starts typing, or
  //   - We explicitly focus the omnibox again.
  // The latter case must be handled in three separate places--OnSetFocus(),
  // OmniboxView::SetFocus(), and the mouse handlers in OmniboxView. See
  // accompanying comments for why each of these is necessary.
  //
  // Caret visibility is tracked per-tab and updates automatically upon
  // switching tabs.
  void SetCaretVisibility(bool visible);

  // Sent before |OnKillFocus| and before the popup is closed.
  void OnWillKillFocus();

  // Called when the view is losing focus.  Resets some state.
  void OnKillFocus();

  // Returns whether the omnibox will handle a press of the escape key.  The
  // caller can use this to decide whether the browser should process escape as
  // "stop current page load".
  bool WillHandleEscapeKey() const;

  // Called when the user presses the escape key.  Decides what, if anything, to
  // revert about any current edits.  Returns whether the key was handled.
  bool OnEscapeKeyPressed();

  // Called when the user presses or releases the control key.  Changes state as
  // necessary.
  void OnControlKeyChanged(bool pressed);

  // Called when the user pastes in text.
  void OnPaste();

  // Returns true if pasting is in progress.
  bool is_pasting() const { return paste_state_ == PASTING; }

  // TODO(beaudoin): Try not to expose this.
  bool in_revert() const { return in_revert_; }

  // Called when the user presses up or down.  |count| is a repeat count,
  // negative for moving up, positive for moving down.
  virtual void OnUpOrDownKeyPressed(int count);

  // Called when any relevant data changes.  This rolls together several
  // separate pieces of data into one call so we can update all the UI
  // efficiently:
  //   |text| is either the new temporary text from the user manually selecting
  //     a different match, or the inline autocomplete text.  We distinguish by
  //     checking if |destination_for_temporary_text_change| is NULL.
  //   |destination_for_temporary_text_change| is NULL (if temporary text should
  //     not change) or the pre-change destination URL (if temporary text should
  //     change) so we can save it off to restore later.
  //   |keyword| is the keyword to show a hint for if |is_keyword_hint| is true,
  //     or the currently selected keyword if |is_keyword_hint| is false (see
  //     comments on keyword_ and is_keyword_hint_).
  void OnPopupDataChanged(
      const base::string16& text,
      GURL* destination_for_temporary_text_change,
      const base::string16& keyword,
      bool is_keyword_hint);

  // Called by the OmniboxView after something changes, with details about what
  // state changes occured.  Updates internal state, updates the popup if
  // necessary, and returns true if any significant changes occurred.  Note that
  // |text_differs| may be set even if |old_text| == |new_text|, e.g. if we've
  // just committed an IME composition.
  //
  // If |allow_keyword_ui_change| is false then the change should not affect
  // keyword ui state, even if the text matches a keyword exactly. This value
  // may be false when the user is composing a text with an IME.
  bool OnAfterPossibleChange(const base::string16& old_text,
                             const base::string16& new_text,
                             size_t selection_start,
                             size_t selection_end,
                             bool selection_differs,
                             bool text_differs,
                             bool just_deleted_text,
                             bool allow_keyword_ui_change);

  // Called when the current match has changed in the OmniboxController.
  void OnCurrentMatchChanged();

  // Name of the histogram tracking cut or copy omnibox commands.
  static const char kCutOrCopyAllTextHistogram[];

 private:
  friend class OmniboxControllerTest;

  enum PasteState {
    NONE,           // Most recent edit was not a paste.
    PASTING,        // In the middle of doing a paste. We need this intermediate
                    // state because OnPaste() does the actual detection of
                    // paste, but OnAfterPossibleChange() has to update the
                    // paste state for every edit. If OnPaste() set the state
                    // directly to PASTED, OnAfterPossibleChange() wouldn't know
                    // whether that represented the current edit or a past one.
    PASTED,         // Most recent edit was a paste.
  };

  enum ControlKeyState {
    UP,                   // The control key is not depressed.
    DOWN_WITHOUT_CHANGE,  // The control key is depressed, and the edit's
                          // contents/selection have not changed since it was
                          // depressed.  This is the only state in which we
                          // do the "ctrl-enter" behavior when the user hits
                          // enter.
    DOWN_WITH_CHANGE,     // The control key is depressed, and the edit's
                          // contents/selection have changed since it was
                          // depressed.  If the user now hits enter, we assume
                          // he simply hasn't released the key, rather than that
                          // he intended to hit "ctrl-enter".
  };

  // Returns true if a query to an autocomplete provider is currently
  // in progress.  This logic should in the future live in
  // AutocompleteController but resides here for now.  This method is used by
  // AutomationProvider::AutocompleteEditIsQueryInProgress.
  bool query_in_progress() const;

  // Called whenever user_text_ should change.
  void InternalSetUserText(const base::string16& text);

  // Turns off keyword mode for the current match.
  void ClearPopupKeywordMode() const;

  // Conversion between user text and display text. User text is the text the
  // user has input. Display text is the text being shown in the edit. The
  // two are different if a keyword is selected.
  base::string16 DisplayTextFromUserText(const base::string16& text) const;
  base::string16 UserTextFromDisplayText(const base::string16& text) const;

  // If there's a selected match, copies it into |match|. Else, returns the
  // default match for the current text, as well as the alternate nav URL, if
  // |alternate_nav_url| is non-NULL and there is such a URL.
  void GetInfoForCurrentText(AutocompleteMatch* match,
                             GURL* alternate_nav_url) const;

  // Reverts the edit box from a temporary text back to the original user text.
  // If |revert_popup| is true then the popup will be reverted as well.
  void RevertTemporaryText(bool revert_popup);

  // Accepts current keyword if the user just typed a space at the end of
  // |new_text|.  This handles both of the following cases:
  //   (assume "foo" is a keyword, | is the input caret, [] is selected text)
  //   foo| -> foo |      (a space was appended to a keyword)
  //   foo[bar] -> foo |  (a space replaced other text after a keyword)
  // Returns true if the current keyword is accepted.
  bool MaybeAcceptKeywordBySpace(const base::string16& new_text);

  // Checks whether the user inserted a space into |old_text| and by doing so
  // created a |new_text| that looks like "<keyword> <search phrase>".
  bool CreatedKeywordSearchByInsertingSpaceInMiddle(
      const base::string16& old_text,
      const base::string16& new_text,
      size_t caret_position) const;

  // Checks if a given character is a valid space character for accepting
  // keyword.
  static bool IsSpaceCharForAcceptingKeyword(wchar_t c);

  // Classify the current page being viewed as, for example, the new tab
  // page or a normal web page.  Used for logging omnibox events for
  // UMA opted-in users.  Examines the user's profile to determine if the
  // current page is the user's home page.
  metrics::OmniboxEventProto::PageClassification ClassifyPage() const;

  // Sets |match| and |alternate_nav_url| based on classifying |text|.
  // |alternate_nav_url| may be NULL.
  void ClassifyStringForPasteAndGo(const base::string16& text,
                                   AutocompleteMatch* match,
                                   GURL* alternate_nav_url) const;

  // If focus_state_ does not match |state|, we update it and notify the
  // InstantController about the change (passing along the |reason| for the
  // change). If the caret visibility changes, we call ApplyCaretVisibility() on
  // the view.
  void SetFocusState(OmniboxFocusState state, OmniboxFocusChangeReason reason);

  // NOTE: |client_| must outlive |omnibox_controller_|, as the latter has a
  // reference to the former.
  std::unique_ptr<OmniboxClient> client_;

  std::unique_ptr<OmniboxController> omnibox_controller_;

  OmniboxView* view_;

  OmniboxEditController* controller_;

  OmniboxFocusState focus_state_;

  // Used to keep track whether the input currently in progress originated by
  // focusing in the Omnibox or in the Fakebox. This will be INVALID if no input
  // is in progress or the Omnibox is not focused.
  FocusSource focus_source_;

  // The URL of the currently displayed page.
  base::string16 permanent_text_;

  // This flag is true when the user has modified the contents of the edit, but
  // not yet accepted them.  We use this to determine when we need to save
  // state (on switching tabs) and whether changes to the page URL should be
  // immediately displayed.
  // This flag will be true in a superset of the cases where the popup is open.
  bool user_input_in_progress_;

  // The text that the user has entered.  This does not include inline
  // autocomplete text that has not yet been accepted.
  base::string16 user_text_;

  // We keep track of when the user last focused on the omnibox.
  base::TimeTicks last_omnibox_focus_;

  // Whether any user input has occurred since focusing on the omnibox. This is
  // used along with |last_omnibox_focus_| to calculate the time between a user
  // focusing on the omnibox and editing. It is initialized to true since
  // there was no focus event.
  bool user_input_since_focus_;

  // We keep track of when the user began modifying the omnibox text.
  // This should be valid whenever user_input_in_progress_ is true.
  base::TimeTicks time_user_first_modified_omnibox_;

  // When the user closes the popup, we need to remember the URL for their
  // desired choice, so that if they hit enter without reopening the popup we
  // know where to go.  We could simply rerun autocomplete in this case, but
  // we'd need to either wait for all results to come in (unacceptably slow) or
  // do the wrong thing when the user had chosen some provider whose results
  // were not returned instantaneously.
  //
  // This variable is only valid when user_input_in_progress_ is true, since
  // when it is false the user has either never input anything (so there won't
  // be a value here anyway) or has canceled their input, which should be
  // treated the same way.  Also, since this is for preserving a desired URL
  // after the popup has been closed, we ignore this if the popup is open, and
  // simply ask the popup for the desired URL directly.  As a result, the
  // contents of this variable only need to be updated when the popup is closed
  // but user_input_in_progress_ is not being cleared.
  base::string16 url_for_remembered_user_selection_;

  // Inline autocomplete is allowed if the user has not just deleted text, and
  // no temporary text is showing.  In this case, inline_autocomplete_text_ is
  // appended to the user_text_ and displayed selected (at least initially).
  //
  // NOTE: When the popup is closed there should never be inline autocomplete
  // text (actions that close the popup should either accept the text, convert
  // it to a normal selection, or change the edit entirely).
  bool just_deleted_text_;
  base::string16 inline_autocomplete_text_;

  // Used by OnPopupDataChanged to keep track of whether there is currently a
  // temporary text.
  //
  // Example of use: If the user types "goog", then arrows down in the
  // autocomplete popup until, say, "google.com" appears in the edit box, then
  // the user_text_ is still "goog", and "google.com" is "temporary text".
  // When the user hits <esc>, the edit box reverts to "goog".  Hit <esc> again
  // and the popup is closed and "goog" is replaced by the permanent_text_,
  // which is the URL of the current page.
  //
  // original_url_ is only valid when there is temporary text, and is used as
  // the unique identifier of the originally selected item.  Thus, if the user
  // arrows to a different item with the same text, we can still distinguish
  // them and not revert all the way to the permanent_text_.
  bool has_temporary_text_;
  GURL original_url_;

  // When the user's last action was to paste, we disallow inline autocomplete
  // (on the theory that the user is trying to paste in a new URL or part of
  // one, and in either case inline autocomplete would get in the way).
  PasteState paste_state_;

  // Whether the control key is depressed.  We track this to avoid calling
  // UpdatePopup() repeatedly if the user holds down the key, and to know
  // whether to trigger "ctrl-enter" behavior.
  ControlKeyState control_key_state_;

  // The keyword associated with the current match.  The user may have an actual
  // selected keyword, or just some input text that looks like a keyword (so we
  // can show a hint to press <tab>).  This is the keyword in either case;
  // is_keyword_hint_ (below) distinguishes the two cases.
  base::string16 keyword_;

  // True if the keyword associated with this match is merely a hint, i.e. the
  // user hasn't actually selected a keyword yet.  When this is true, we can use
  // keyword_ to show a "Press <tab> to search" sort of hint.
  bool is_keyword_hint_;

  // This is needed to properly update the SearchModel state when the user
  // presses escape.
  bool in_revert_;

  // Indicates if the upcoming autocomplete search is allowed to be treated as
  // an exact keyword match.  If this is true then keyword mode will be
  // triggered automatically if the input is "<keyword> <search string>".  We
  // allow this when CreatedKeywordSearchByInsertingSpaceInMiddle() is true.
  // This has no effect if we're already in keyword mode.
  bool allow_exact_keyword_match_;

  // The input that was sent to the AutocompleteController. Since no
  // autocomplete query is started after a tab switch, it is possible for this
  // |input_| to differ from the one currently stored in AutocompleteController.
  AutocompleteInput input_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxEditModel);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_EDIT_MODEL_H_
