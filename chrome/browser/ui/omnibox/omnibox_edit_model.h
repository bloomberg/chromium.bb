// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_MODEL_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_MODEL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/common/metrics/proto/omnibox_event.pb.h"
#include "content/public/common/page_transition_types.h"
#include "googleurl/src/gurl.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"

class AutocompleteController;
class AutocompleteResult;
struct InstantSuggestion;
class OmniboxCurrentPageDelegate;
class OmniboxEditController;
class OmniboxPopupModel;
class OmniboxView;
class Profile;

namespace gfx {
class Image;
class Rect;
}

// Omnibox focus state.
enum OmniboxFocusState {
  // Not focused.
  OMNIBOX_FOCUS_NONE,

  // Visibly focused.
  OMNIBOX_FOCUS_VISIBLE,

  // Invisibly focused, i.e. focused with a hidden caret.
  OMNIBOX_FOCUS_INVISIBLE,
};

// Reasons why the Omnibox focus state could change.
enum OmniboxFocusChangeReason {
  // Includes any explicit changes to focus. (e.g. user clicking to change
  // focus, user tabbing to change focus, any explicit calls to SetFocus,
  // etc.)
  OMNIBOX_FOCUS_CHANGE_EXPLICIT,

  // Focus changed to restore state from a tab the user switched to.
  OMNIBOX_FOCUS_CHANGE_TAB_SWITCH,

  // Focus changed because user started typing. This only happens when focus
  // state is INVISIBLE (and this results in a change to VISIBLE).
  OMNIBOX_FOCUS_CHANGE_TYPING,
};

class OmniboxEditModel : public AutocompleteControllerDelegate {
 public:
  struct State {
    State(bool user_input_in_progress,
          const string16& user_text,
          const string16& keyword,
          bool is_keyword_hint,
          OmniboxFocusState focus_state);
    ~State();

    bool user_input_in_progress;
    const string16 user_text;
    const string16 keyword;
    const bool is_keyword_hint;
    OmniboxFocusState focus_state;
  };

  OmniboxEditModel(OmniboxView* view,
                   OmniboxEditController* controller,
                   Profile* profile);
  virtual ~OmniboxEditModel();

  AutocompleteController* autocomplete_controller() const {
    return autocomplete_controller_.get();
  }

  void set_popup_model(OmniboxPopupModel* popup_model) {
    popup_ = popup_model;
  }

  // TODO: The edit and popup should be siblings owned by the LocationBarView,
  // making this accessor unnecessary.
  OmniboxPopupModel* popup_model() const { return popup_; }

  OmniboxEditController* controller() const { return controller_; }

  Profile* profile() const { return profile_; }

  // Returns the current state.  This assumes we are switching tabs, and changes
  // the internal state appropriately.
  const State GetStateForTabSwitch();

  // Restores local state from the saved |state|.
  void RestoreState(const State& state);

  // Returns the match for the current text. If the user has not edited the text
  // this is the match corresponding to the permanent text.
  AutocompleteMatch CurrentMatch();

  // Called when the user wants to export the entire current text as a URL.
  // Sets the url, and if known, the title and favicon.
  void GetDataForURLExport(GURL* url, string16* title, gfx::Image* favicon);

  // Returns true if a verbatim query should be used for Instant. A verbatim
  // query is forced in certain situations, such as pressing delete at the end
  // of the edit.
  bool UseVerbatimInstant();

  // If the user presses ctrl-enter, it means "add .com to the the end".  The
  // desired TLD is the TLD the user desires to add to the end of the current
  // input, if any, based on their control key state and any other actions
  // they've taken.
  string16 GetDesiredTLD() const;

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
                         string16* text,
                         GURL* url,
                         bool* write_url);

  bool user_input_in_progress() const { return user_input_in_progress_; }

  // Sets the state of user_input_in_progress_, and notifies the observer if
  // that state has changed.
  void SetInputInProgress(bool in_progress);

  // Updates permanent_text_ to |new_permanent_text|.  Returns true if this
  // change should be immediately user-visible, because either the user is not
  // editing or the edit does not have focus.
  bool UpdatePermanentText(const string16& new_permanent_text);

  // Returns the URL corresponding to the permanent text.
  GURL PermanentURL();

  // Sets the user_text_ to |text|.  Only the View should call this.
  void SetUserText(const string16& text);

  // Calls through to SearchProvider::FinalizeInstantQuery.
  // If |skip_inline_autocomplete| is true then the |suggestion| text will be
  // turned into final text instead of inline autocomplete suggest.
  void FinalizeInstantQuery(const string16& input_text,
                            const InstantSuggestion& suggestion,
                            bool skip_inline_autocomplete);

  // Sets the suggestion text.
  void SetInstantSuggestion(const InstantSuggestion& suggestion);

  // Commits the suggested text. If |skip_inline_autocomplete| is true then the
  // suggested text will be committed as final text as if it's inputted by the
  // user, rather than as inline autocomplete suggest.
  // Returns true if the text was committed.
  // TODO: can the return type be void?
  bool CommitSuggestedText(bool skip_inline_autocomplete);

  // Accepts the currently showing Instant preview, if any, and returns true.
  // Returns false if there is no Instant preview showing.
  bool AcceptCurrentInstantPreview();

  // Invoked any time the text may have changed in the edit. Updates Instant and
  // notifies the controller.
  void OnChanged();

  // Reverts the edit model back to its unedited state (permanent text showing,
  // no user input in progress).
  void Revert();

  // Directs the popup to start autocomplete.
  void StartAutocomplete(bool has_selected_text,
                         bool prevent_inline_autocomplete) const;

  // Closes the popup and cancels any pending asynchronous queries.
  void StopAutocomplete();

  // Determines whether the user can "paste and go", given the specified text.
  bool CanPasteAndGo(const string16& text) const;

  // Navigates to the destination last supplied to CanPasteAndGo.
  void PasteAndGo(const string16& text);

  // Returns true if this is a paste-and-search rather than paste-and-go (or
  // nothing).
  bool IsPasteAndSearch(const string16& text) const;

  // Asks the browser to load the popup's currently selected item, using the
  // supplied disposition.  This may close the popup. If |for_drop| is true,
  // it indicates the input is being accepted as part of a drop operation and
  // the transition should be treated as LINK (so that it won't trigger the
  // URL to be autocompleted).
  void AcceptInput(WindowOpenDisposition disposition,
                   bool for_drop);

  // Asks the browser to load the item at |index|, with the given properties.
  void OpenMatch(const AutocompleteMatch& match,
                 WindowOpenDisposition disposition,
                 const GURL& alternate_nav_url,
                 size_t index);

  OmniboxFocusState focus_state() const { return focus_state_; }
  bool has_focus() const { return focus_state_ != OMNIBOX_FOCUS_NONE; }
  bool is_caret_visible() const {
    return focus_state_ == OMNIBOX_FOCUS_VISIBLE;
  }

  // Accessors for keyword-related state (see comments on keyword_ and
  // is_keyword_hint_).
  const string16& keyword() const { return keyword_; }
  bool is_keyword_hint() const { return is_keyword_hint_; }

  // Accepts the current keyword hint as a keyword. It always returns true for
  // caller convenience.
  bool AcceptKeyword();

  // Clears the current keyword.  |visible_text| is the (non-keyword) text
  // currently visible in the edit.
  void ClearKeyword(const string16& visible_text);

  // Returns the current autocomplete result.  This logic should in the future
  // live in AutocompleteController but resides here for now.  This method is
  // used by AutomationProvider::AutocompleteEditGetMatches.
  const AutocompleteResult& result() const;

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
  void OnWillKillFocus(gfx::NativeView view_gaining_focus);

  // Called when the view is losing focus.  Resets some state.
  void OnKillFocus();

  // Called when the user presses the escape key.  Decides what, if anything, to
  // revert about any current edits.  Returns whether the key was handled.
  bool OnEscapeKeyPressed();

  // Called when the user presses or releases the control key.  Changes state as
  // necessary.
  void OnControlKeyChanged(bool pressed);

  // Called when the user pastes in text.
  void on_paste() { paste_state_ = PASTING; }

  // Returns true if pasting is in progress.
  bool is_pasting() const { return paste_state_ == PASTING; }

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
      const string16& text,
      GURL* destination_for_temporary_text_change,
      const string16& keyword,
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
  bool OnAfterPossibleChange(const string16& old_text,
                             const string16& new_text,
                             size_t selection_start,
                             size_t selection_end,
                             bool selection_differs,
                             bool text_differs,
                             bool just_deleted_text,
                             bool allow_keyword_ui_change);

  // Invoked when the popup has changed its bounds to |bounds|. |bounds| here
  // is in screen coordinates.
  void OnPopupBoundsChanged(const gfx::Rect& bounds);

 private:
  friend class InstantTestBase;

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

  // AutocompleteControllerDelegate:
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

  // Returns true if a query to an autocomplete provider is currently
  // in progress.  This logic should in the future live in
  // AutocompleteController but resides here for now.  This method is used by
  // AutomationProvider::AutocompleteEditIsQueryInProgress.
  bool query_in_progress() const;

  // Called whenever user_text_ should change.
  void InternalSetUserText(const string16& text);

  // Returns true if a keyword is selected.
  bool KeywordIsSelected() const;

  // Turns off keyword mode for the current match.
  void ClearPopupKeywordMode() const;

  // Conversion between user text and display text. User text is the text the
  // user has input. Display text is the text being shown in the edit. The
  // two are different if a keyword is selected.
  string16 DisplayTextFromUserText(const string16& text) const;
  string16 UserTextFromDisplayText(const string16& text) const;

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

  // Returns the default match for the current text, as well as the alternate
  // nav URL, if |alternate_nav_url| is non-NULL and there is such a URL.
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
  bool MaybeAcceptKeywordBySpace(const string16& new_text);

  // Checks whether the user inserted a space into |old_text| and by doing so
  // created a |new_text| that looks like "<keyword> <search phrase>".
  bool CreatedKeywordSearchByInsertingSpaceInMiddle(
      const string16& old_text,
      const string16& new_text,
      size_t caret_position) const;

  // Tries to start an Instant preview for |match|. Returns true if Instant
  // processed the match.
  bool DoInstant(const AutocompleteMatch& match);

  // Starts a DNS prefetch for the given |match|.
  void DoPreconnect(const AutocompleteMatch& match);

  // Checks if a given character is a valid space character for accepting
  // keyword.
  static bool IsSpaceCharForAcceptingKeyword(wchar_t c);

  // Classify the current page being viewed as, for example, the new tab
  // page or a normal web page.  Used for logging omnibox events for
  // UMA opted-in users.  Examines the user's profile to determine if the
  // current page is the user's home page.
  metrics::OmniboxEventProto::PageClassification ClassifyPage(
      const GURL& gurl) const;

  // Sets |match| and |alternate_nav_url| based on classifying |text|.
  // |alternate_nav_url| may be NULL.
  void ClassifyStringForPasteAndGo(const string16& text,
                                   AutocompleteMatch* match,
                                   GURL* alternate_nav_url) const;

  // If focus_state_ does not match |state|, we update it and notify the
  // InstantController about the change (passing along the |reason| for the
  // change). If the caret visibility changes, we call ApplyCaretVisibility() on
  // the view.
  void SetFocusState(OmniboxFocusState state, OmniboxFocusChangeReason reason);

  scoped_ptr<AutocompleteController> autocomplete_controller_;

  OmniboxView* view_;

  OmniboxPopupModel* popup_;

  OmniboxEditController* controller_;

  scoped_ptr<OmniboxCurrentPageDelegate> delegate_;

  OmniboxFocusState focus_state_;

  // The URL of the currently displayed page.
  string16 permanent_text_;

  // This flag is true when the user has modified the contents of the edit, but
  // not yet accepted them.  We use this to determine when we need to save
  // state (on switching tabs) and whether changes to the page URL should be
  // immediately displayed.
  // This flag will be true in a superset of the cases where the popup is open.
  bool user_input_in_progress_;

  // The text that the user has entered.  This does not include inline
  // autocomplete text that has not yet been accepted.
  string16 user_text_;

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
  string16 url_for_remembered_user_selection_;

  // Inline autocomplete is allowed if the user has not just deleted text, and
  // no temporary text is showing.  In this case, inline_autocomplete_text_ is
  // appended to the user_text_ and displayed selected (at least initially).
  //
  // NOTE: When the popup is closed there should never be inline autocomplete
  // text (actions that close the popup should either accept the text, convert
  // it to a normal selection, or change the edit entirely).
  bool just_deleted_text_;
  string16 inline_autocomplete_text_;

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

  // True if Instant set the current temporary text, as opposed to it being set
  // due to the user arrowing up/down through the popup.
  // TODO(sreeram): This is a temporary hack. Remove it once the omnibox edit
  // model/view code is decoupled from Instant (among other things).
  bool is_temporary_text_set_by_instant_;

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
  string16 keyword_;

  // True if the keyword associated with this match is merely a hint, i.e. the
  // user hasn't actually selected a keyword yet.  When this is true, we can use
  // keyword_ to show a "Press <tab> to search" sort of hint.
  bool is_keyword_hint_;

  Profile* profile_;

  // This is needed as prior to accepting the current text the model is
  // reverted, which triggers resetting Instant. We don't want to update Instant
  // in this case, so we use the flag to determine if this is happening.
  bool in_revert_;

  // InstantController needs this in extended mode to distinguish the case in
  // which it should instruct a committed search results page to revert to
  // showing results for the original query.
  bool in_escape_handler_;

  // Indicates if the upcoming autocomplete search is allowed to be treated as
  // an exact keyword match.  If this is true then keyword mode will be
  // triggered automatically if the input is "<keyword> <search string>".  We
  // allow this when CreatedKeywordSearchByInsertingSpaceInMiddle() is true.
  // This has no effect if we're already in keyword mode.
  bool allow_exact_keyword_match_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxEditModel);
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EDIT_MODEL_H_
