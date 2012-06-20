// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_VIEW_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/ui/cocoa/location_bar/autocomplete_text_field.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"

class AutocompleteEditController;
class OmniboxPopupViewMac;
class Profile;
class ToolbarModel;

namespace ui {
class Clipboard;
}

// Implements OmniboxView on an AutocompleteTextField.

class OmniboxViewMac : public OmniboxView,
                       public AutocompleteTextFieldObserver {
 public:
  OmniboxViewMac(AutocompleteEditController* controller,
                 ToolbarModel* toolbar_model,
                 Profile* profile,
                 CommandUpdater* command_updater,
                 AutocompleteTextField* field);
  virtual ~OmniboxViewMac();

  // OmniboxView:
  virtual AutocompleteEditModel* model() OVERRIDE;
  virtual const AutocompleteEditModel* model() const OVERRIDE;
  virtual void SaveStateToTab(content::WebContents* tab) OVERRIDE;
  virtual void Update(
      const content::WebContents* tab_for_state_restoring) OVERRIDE;
  virtual void OpenMatch(const AutocompleteMatch& match,
                         WindowOpenDisposition disposition,
                         const GURL& alternate_nav_url,
                         size_t index) OVERRIDE;
  virtual string16 GetText() const OVERRIDE;
  virtual bool IsEditingOrEmpty() const OVERRIDE;
  virtual int GetIcon() const OVERRIDE;
  virtual void SetUserText(const string16& text) OVERRIDE;
  virtual void SetUserText(const string16& text,
                           const string16& display_text,
                           bool update_popup) OVERRIDE;
  virtual void SetWindowTextAndCaretPos(const string16& text,
                                        size_t caret_pos,
                                        bool update_popup,
                                        bool notify_text_changed) OVERRIDE;
  virtual void SetForcedQuery() OVERRIDE;
  virtual bool IsSelectAll() const OVERRIDE;
  virtual bool DeleteAtEndPressed() OVERRIDE;
  virtual void GetSelectionBounds(string16::size_type* start,
                                  string16::size_type* end) const OVERRIDE;
  virtual void SelectAll(bool reversed) OVERRIDE;
  virtual void RevertAll() OVERRIDE;
  virtual void UpdatePopup() OVERRIDE;
  virtual void ClosePopup() OVERRIDE;
  virtual void SetFocus() OVERRIDE;
  virtual void OnTemporaryTextMaybeChanged(
      const string16& display_text,
      bool save_original_selection) OVERRIDE;
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const string16& display_text, size_t user_text_length) OVERRIDE;
  virtual void OnStartingIME() OVERRIDE;
  virtual void OnRevertTemporaryText() OVERRIDE;
  virtual void OnBeforePossibleChange() OVERRIDE;
  virtual bool OnAfterPossibleChange() OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetRelativeWindowForPopup() const OVERRIDE;
  virtual CommandUpdater* GetCommandUpdater() OVERRIDE;
  virtual void SetInstantSuggestion(const string16& input,
                                    bool animate_to_complete) OVERRIDE;
  virtual string16 GetInstantSuggestion() const OVERRIDE;
  virtual int TextWidth() const OVERRIDE;
  virtual bool IsImeComposing() const OVERRIDE;

  // Implement the AutocompleteTextFieldObserver interface.
  virtual NSRange SelectionRangeForProposedRange(
      NSRange proposed_range) OVERRIDE;
  virtual void OnControlKeyChanged(bool pressed) OVERRIDE;
  virtual bool CanCopy() OVERRIDE;
  virtual void CopyToPasteboard(NSPasteboard* pboard) OVERRIDE;
  virtual void OnPaste() OVERRIDE;
  virtual bool CanPasteAndGo() OVERRIDE;
  virtual int GetPasteActionStringId() OVERRIDE;
  virtual void OnPasteAndGo() OVERRIDE;
  virtual void OnFrameChanged() OVERRIDE;
  virtual void OnDidBeginEditing() OVERRIDE;
  virtual void OnBeforeChange() OVERRIDE;
  virtual void OnDidChange() OVERRIDE;
  virtual void OnDidEndEditing() OVERRIDE;
  virtual bool OnDoCommandBySelector(SEL cmd) OVERRIDE;
  virtual void OnSetFocus(bool control_down) OVERRIDE;
  virtual void OnKillFocus() OVERRIDE;

  // Helper for LocationBarViewMac.  Optionally selects all in |field_|.
  void FocusLocation(bool select_all);

  // Helper to get appropriate contents from |clipboard|.  Returns
  // empty string if no appropriate data is found on |clipboard|.
  static string16 GetClipboardText(ui::Clipboard* clipboard);

  // Helper to get the font to use in the field, exposed for the
  // popup.
  static NSFont* GetFieldFont();

  // If |resource_id| has a PDF image which can be used, return it.
  // Otherwise return the PNG image from the resource bundle.
  static NSImage* ImageForResource(int resource_id);

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
  void SetText(const string16& display_text);

  // Internal implementation of SetText.  Does not reset the suggest text before
  // setting the display text.  Most callers should use |SetText()| instead.
  void SetTextInternal(const string16& display_text);

  // Update the field with |display_text| and set the selection.
  void SetTextAndSelectedRange(const string16& display_text,
                               const NSRange range);

  // Returns the non-suggest portion of |field_|'s string value.
  NSString* GetNonSuggestTextSubstring() const;

  // Returns the suggest portion of |field_|'s string value.
  NSString* GetSuggestTextSubstring() const;

  // Pass the current content of |field_| to SetText(), maintaining
  // any selection.  Named to be consistent with GTK and Windows,
  // though here we cannot really do the in-place operation they do.
  void EmphasizeURLComponents();

  // Internally invoked whenever the text changes in some way.
  void TextChanged();

  // Calculates text attributes according to |display_text| and applies them
  // to the given |as| object.
  void ApplyTextAttributes(const string16& display_text,
                           NSMutableAttributedString* as);

  // Return the number of UTF-16 units in the current buffer, excluding the
  // suggested text.
  NSUInteger GetTextLength() const;

  // Places the caret at the given position. This clears any selection.
  void PlaceCaretAt(NSUInteger pos);

  // Returns true if the caret is at the end of the content.
  bool IsCaretAtEnd() const;

  scoped_ptr<AutocompleteEditModel> model_;
  scoped_ptr<OmniboxPopupViewMac> popup_view_;

  AutocompleteEditController* controller_;
  ToolbarModel* toolbar_model_;

  // The object that handles additional command functionality exposed on the
  // edit, such as invoking the keyword editor.
  CommandUpdater* command_updater_;

  AutocompleteTextField* field_;  // owned by tab controller

  // Selection at the point where the user started using the
  // arrows to move around in the popup.
  NSRange saved_temporary_selection_;

  // Tracking state before and after a possible change for reporting
  // to model_.
  NSRange selection_before_change_;
  string16 text_before_change_;
  NSRange marked_range_before_change_;

  // Length of the suggest text.  The suggest text always appears at the end of
  // the field.
  size_t suggest_text_length_;

  // Was delete pressed?
  bool delete_was_pressed_;

  // Was the delete key pressed with an empty selection at the end of the edit?
  bool delete_at_end_pressed_;

  // The maximum/standard line height for the displayed text.
  CGFloat line_height_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_OMNIBOX_OMNIBOX_VIEW_MAC_H_
