// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_

#import <UIKit/UIKit.h>

#include <memory>
#include "base/mac/scoped_nsobject.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/toolbar/toolbar_model.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"

struct AutocompleteMatch;
class AutocompleteResult;
@class AutocompleteTextFieldDelegate;
class GURL;
@class OmniboxTextFieldIOS;
@protocol OmniboxPopupPositioner;
class WebOmniboxEditController;
class OmniboxPopupViewIOS;
@protocol PreloadProvider;

namespace ios {
class ChromeBrowserState;
}

// iOS implementation of OmniBoxView.  Wraps a UITextField and
// interfaces with the rest of the autocomplete system.
class OmniboxViewIOS : public OmniboxView {
 public:
  // Retains |field|.
  OmniboxViewIOS(OmniboxTextFieldIOS* field,
                 WebOmniboxEditController* controller,
                 ios::ChromeBrowserState* browser_state,
                 id<PreloadProvider> prerender,
                 id<OmniboxPopupPositioner> positioner);
  ~OmniboxViewIOS() override;

  // Returns a color representing |security_level|, adjusted based on whether
  // the browser is in Incognito mode.
  static UIColor* GetSecureTextColor(
      security_state::SecurityLevel security_level,
      bool in_dark_mode);

  // OmniboxView implementation.
  void OpenMatch(const AutocompleteMatch& match,
                 WindowOpenDisposition disposition,
                 const GURL& alternate_nav_url,
                 const base::string16& pasted_text,
                 size_t selected_line) override;
  base::string16 GetText() const override;
  void SetWindowTextAndCaretPos(const base::string16& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override;
  void SetCaretPos(size_t caret_pos) override;
  void RevertAll() override;
  void UpdatePopup() override;
  void OnTemporaryTextMaybeChanged(const base::string16& display_text,
                                   bool save_original_selection,
                                   bool notify_text_changed) override;
  bool OnInlineAutocompleteTextMaybeChanged(const base::string16& display_text,
                                            size_t user_text_length) override;
  void OnBeforePossibleChange() override;
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override;
  bool IsImeComposing() const override;
  bool IsIndicatingQueryRefinement() const override;

  // OmniboxView stubs.
  void Update() override {}
  void EnterKeywordModeForDefaultSearchProvider() override {}
  bool IsSelectAll() const override;
  bool DeleteAtEndPressed() override;
  void GetSelectionBounds(base::string16::size_type* start,
                          base::string16::size_type* end) const override;
  void SelectAll(bool reversed) override {}
  void SetFocus() override {}
  void ApplyCaretVisibility() override {}
  void OnInlineAutocompleteTextCleared() override {}
  void OnRevertTemporaryText() override {}
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetRelativeWindowForPopup() const override;
  int GetTextWidth() const override;
  int GetWidth() const override;

  // AutocompleteTextFieldDelegate methods
  void OnDidBeginEditing();
  bool OnWillChange(NSRange range, NSString* new_text);
  void OnDidChange(bool processing_user_input);
  void OnDidEndEditing();
  void OnAccept();
  void OnClear();
  bool OnCopy();
  void WillPaste();
  void OnDeleteBackward();

  ios::ChromeBrowserState* browser_state() { return browser_state_; }

  // Updates this edit view to show the proper text, highlight and images.
  void UpdateAppearance();

  // Clears the text from the omnibox.
  void ClearText();

  // Set first result image.
  void SetLeftImage(const int imageId);

  // Hide keyboard and call OnDidEndEditing.  This dismisses the keyboard and
  // also finalizes the editing state of the omnibox.
  void HideKeyboardAndEndEditing();

  // Hide keyboard only.  Used when omnibox popups grab focus but editing isn't
  // complete.
  void HideKeyboard();

  // Focus the omnibox field.  This is used when the omnibox popup copies a
  // search query to the omnibox so the user can modify it further.
  // This does not affect the popup state and is a NOOP if the omnibox is
  // already focused.
  void FocusOmnibox();

  // Called when the popup results change.  Used to update prerendering.
  void OnPopupResultsChanged(const AutocompleteResult& result);

  // Returns |true| if AutocompletePopupView is currently open.
  BOOL IsPopupOpen();

  // Returns the resource ID of the icon to show for the current text. Takes
  // into account the security level of the page, and |offline_page|.
  int GetIcon(bool offline_page) const;

 protected:
  int GetOmniboxTextLength() const override;
  void EmphasizeURLComponents() override;

 private:
  // Calculates text attributes according to |display_text| and
  // returns them in an autoreleased object.
  NSAttributedString* ApplyTextAttributes(const base::string16& text);

  void SetEmphasis(bool emphasize, const gfx::Range& range) override;
  void UpdateSchemeStyle(const gfx::Range& scheme_range) override;

  // Removes the query refinement chip from the omnibox.
  void RemoveQueryRefinementChip();

  // Returns true if user input should currently be ignored.  On iOS7,
  // modifying the contents of a text field while Siri is pending leads to a
  // UIKit crash.  In order to sidestep that crash, OmniboxViewIOS checks that
  // voice search is not pending before attempting to process user actions that
  // may modify text field contents.
  // TODO(crbug.com/303212): Remove this workaround once the crash is fixed.
  bool ShouldIgnoreUserInputDueToPendingVoiceSearch();

  ios::ChromeBrowserState* browser_state_;

  base::scoped_nsobject<OmniboxTextFieldIOS> field_;
  WebOmniboxEditController* controller_;  // weak, owns us
  std::unique_ptr<OmniboxPopupViewIOS> popup_view_;
  // |preloader_| should be __weak but is included from non-ARC code.
  __unsafe_unretained id<PreloadProvider> preloader_;

  State state_before_change_;
  base::scoped_nsobject<NSString> marked_text_before_change_;
  NSRange current_selection_;
  NSRange old_selection_;

  // TODO(rohitrao): This is a monster hack, needed because closing the popup
  // ends up inadvertently triggering a new round of autocomplete.  Fix the
  // underlying problem, which is that textDidChange: is called when closing the
  // popup, and then remove this hack.  b/5877366.
  BOOL ignore_popup_updates_;

  // Bridges delegate method calls from |field_| to C++ land.
  base::scoped_nsobject<AutocompleteTextFieldDelegate> field_delegate_;

  // Temporary pointer to the attributed display string, stored as color and
  // other emphasis attributes are applied by the superclass.
  NSMutableAttributedString* attributing_display_string_;  // weak
};

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_VIEW_IOS_H_
