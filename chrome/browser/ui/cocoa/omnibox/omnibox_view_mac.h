// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"

class OmniboxPopupView;

namespace ui {
class Clipboard;
}

// Implements OmniboxView on an AutocompleteTextField.
class OmniboxViewMac : public OmniboxView,
                       public AutocompleteTextFieldObserver {
 public:
  OmniboxViewMac(OmniboxEditController* controller,
                 Profile* profile,
                 CommandUpdater* command_updater,
                 AutocompleteTextField* field);
  virtual ~OmniboxViewMac();

  // OmniboxView:
  virtual void SaveStateToTab(content::WebContents* tab) OVERRIDE;
  virtual void OnTabChanged(const content::WebContents* web_contents) OVERRIDE;
  virtual void Update() OVERRIDE;
  virtual void UpdatePlaceholderText() OVERRIDE;
  virtual void OpenMatch(const AutocompleteMatch& match,
                         WindowOpenDisposition disposition,
                         const GURL& alternate_nav_url,
                         const base::string16& pasted_text,
                         size_t selected_line) OVERRIDE;
  virtual base::string16 GetText() const OVERRIDE;
  virtual void SetWindowTextAndCaretPos(const base::string16& text,
                                        size_t caret_pos,
                                        bool update_popup,
                                        bool notify_text_changed) OVERRIDE;
  virtual void SetForcedQuery() OVERRIDE;
  virtual bool IsSelectAll() const OVERRIDE;
  virtual bool DeleteAtEndPressed() OVERRIDE;
  virtual void GetSelectionBounds(
      base::string16::size_type* start,
      base::string16::size_type* end) const OVERRIDE;
  virtual void SelectAll(bool reversed) OVERRIDE;
  virtual void RevertAll() OVERRIDE;
  virtual void UpdatePopup() OVERRIDE;
  virtual void CloseOmniboxPopup() OVERRIDE;
  virtual void SetFocus() OVERRIDE;
  virtual void ApplyCaretVisibility() OVERRIDE;
  virtual void OnTemporaryTextMaybeChanged(
      const base::string16& display_text,
      bool save_original_selection,
      bool notify_text_changed) OVERRIDE;
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const base::string16& display_text, size_t user_text_length) OVERRIDE;
  virtual void OnInlineAutocompleteTextCleared() OVERRIDE;
  virtual void OnRevertTemporaryText() OVERRIDE;
  virtual void OnBeforePossibleChange() OVERRIDE;
  virtual bool OnAfterPossibleChange() OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetRelativeWindowForPopup() const OVERRIDE;
  virtual void SetGrayTextAutocompletion(const base::string16& input) OVERRIDE;
  virtual base::string16 GetGrayTextAutocompletion() const OVERRIDE;
  virtual int GetTextWidth() const OVERRIDE;
  virtual int GetWidth() const OVERRIDE;
  virtual bool IsImeComposing() const OVERRIDE;

  // Implement the AutocompleteTextFieldObserver interface.
  virtual NSRange SelectionRangeForProposedRange(
      NSRange proposed_range) OVERRIDE;
  virtual void OnControlKeyChanged(bool pressed) OVERRIDE;
  virtual bool CanCopy() OVERRIDE;
  virtual void CopyToPasteboard(NSPasteboard* pboard) OVERRIDE;
  virtual bool ShouldEnableShowURL() OVERRIDE;
  virtual void ShowURL() OVERRIDE;
  virtual void OnPaste() OVERRIDE;
  virtual bool CanPasteAndGo() OVERRIDE;
  virtual int GetPasteActionStringId() OVERRIDE;
  virtual void OnPasteAndGo() OVERRIDE;
  virtual void OnFrameChanged() OVERRIDE;
  virtual void ClosePopup() OVERRIDE;
  virtual void OnDidBeginEditing() OVERRIDE;
  virtual void OnBeforeChange() OVERRIDE;
  virtual void OnDidChange() OVERRIDE;
  virtual void OnDidEndEditing() OVERRIDE;
  virtual bool OnDoCommandBySelector(SEL cmd) OVERRIDE;
  virtual void OnSetFocus(bool control_down) OVERRIDE;
  virtual void OnKillFocus() OVERRIDE;
  virtual void OnMouseDown(NSInteger button_number) OVERRIDE;
  virtual bool ShouldSelectAllOnMouseDown() OVERRIDE;

  // Helper for LocationBarViewMac.  Optionally selects all in |field_|.
  void FocusLocation(bool select_all);

  // Helper to get the font to use in the field, exposed for the
  // popup.
  // The style parameter specifies the new style for the font, and is a
  // bitmask of the values: BOLD, ITALIC and UNDERLINE (see ui/gfx/font.h).
  static NSFont* GetFieldFont(int style);

  // If |resource_id| has a PDF image which can be used, return it.
  // Otherwise return the PNG image from the resource bundle.
  static NSImage* ImageForResource(int resource_id);

  // Color used to draw suggest text.
  static NSColor* SuggestTextColor();

  AutocompleteTextField* field() const { return field_; }

 private:
  // Called when the user hits backspace in |field_|.  Checks whether
  // keyword search is being terminated.  Returns true if the
  // backspace should be intercepted (not forwarded on to the standard
  // machinery).
  bool OnBackspacePressed();

  // Returns the field's currently selected range.  Only valid if the
  // field has focus.
  NSRange GetSelectedRange() const;

  // Returns the field's currently marked range. Only valid if the field has
  // focus.
  NSRange GetMarkedRange() const;

  // Returns true if |field_| is first-responder in the window.  Used
  // in various DCHECKS to make sure code is running in appropriate
  // situations.
  bool IsFirstResponder() const;

  // If |model_| believes it has focus, grab focus if needed and set
  // the selection to |range|.  Otherwise does nothing.
  void SetSelectedRange(const NSRange range);

  // Update the field with |display_text| and highlight the host and scheme (if
  // it's an URL or URL-fragment).  Resets any suggest text that may be present.
  void SetText(const base::string16& display_text);

  // Internal implementation of SetText.  Does not reset the suggest text before
  // setting the display text.  Most callers should use |SetText()| instead.
  void SetTextInternal(const base::string16& display_text);

  // Update the field with |display_text| and set the selection.
  void SetTextAndSelectedRange(const base::string16& display_text,
                               const NSRange range);

  // Pass the current content of |field_| to SetText(), maintaining
  // any selection.  Named to be consistent with GTK and Windows,
  // though here we cannot really do the in-place operation they do.
  virtual void EmphasizeURLComponents() OVERRIDE;

  // Calculates text attributes according to |display_text| and applies them
  // to the given |as| object.
  void ApplyTextAttributes(const base::string16& display_text,
                           NSMutableAttributedString* as);

  // Return the number of UTF-16 units in the current buffer, excluding the
  // suggested text.
  virtual int GetOmniboxTextLength() const OVERRIDE;
  NSUInteger GetTextLength() const;

  // Returns true if the caret is at the end of the content.
  bool IsCaretAtEnd() const;

  scoped_ptr<OmniboxPopupView> popup_view_;

  AutocompleteTextField* field_;  // owned by tab controller

  // Selection at the point where the user started using the
  // arrows to move around in the popup.
  NSRange saved_temporary_selection_;

  // Tracking state before and after a possible change for reporting
  // to model_.
  NSRange selection_before_change_;
  base::string16 text_before_change_;
  NSRange marked_range_before_change_;

  // Was delete pressed?
  bool delete_was_pressed_;

  // Was the delete key pressed with an empty selection at the end of the edit?
  bool delete_at_end_pressed_;

  base::string16 suggest_text_;

  // State used to coalesce changes to text and selection to avoid drawing
  // transient state.
  bool in_coalesced_update_block_;
  bool do_coalesced_text_update_;
  base::string16 coalesced_text_update_;
  bool do_coalesced_range_update_;
  NSRange coalesced_range_update_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_VIEW_MAC_H_
