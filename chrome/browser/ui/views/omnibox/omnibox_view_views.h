// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

#if defined(OS_CHROMEOS)
#include "chromeos/ime/input_method_manager.h"
#endif

class LocationBarView;
class OmniboxPopupView;
class Profile;

namespace ui {
class OSExchangeData;
}  // namespace ui

// Views-implementation of OmniboxView, based on the gtk implementation.
class OmniboxViewViews
    : public views::Textfield,
      public OmniboxView,
#if defined(OS_CHROMEOS)
      public
          chromeos::input_method::InputMethodManager::CandidateWindowObserver,
#endif
      public views::TextfieldController {
 public:
  // The internal view class name.
  static const char kViewClassName[];

  OmniboxViewViews(OmniboxEditController* controller,
                   Profile* profile,
                   CommandUpdater* command_updater,
                   bool popup_window_mode,
                   LocationBarView* location_bar,
                   const gfx::FontList& font_list);
  virtual ~OmniboxViewViews();

  // Initialize, create the underlying views, etc;
  void Init();

  // Sets the colors of the text view according to the theme.
  void SetBaseColor();

  // views::Textfield:
  virtual const char* GetClassName() const OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool OnKeyReleased(const ui::KeyEvent& event) OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(
      const ui::KeyEvent& event) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  // OmniboxView:
  virtual void SaveStateToTab(content::WebContents* tab) OVERRIDE;
  virtual void OnTabChanged(const content::WebContents* web_contents) OVERRIDE;
  virtual void Update() OVERRIDE;
  virtual string16 GetText() const OVERRIDE;
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
  virtual void SetFocus() OVERRIDE;
  virtual void ApplyCaretVisibility() OVERRIDE;
  virtual void OnTemporaryTextMaybeChanged(
      const string16& display_text,
      bool save_original_selection,
      bool notify_text_changed) OVERRIDE;
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const string16& display_text, size_t user_text_length) OVERRIDE;
  virtual void OnRevertTemporaryText() OVERRIDE;
  virtual void OnBeforePossibleChange() OVERRIDE;
  virtual bool OnAfterPossibleChange() OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetRelativeWindowForPopup() const OVERRIDE;
  virtual void SetGrayTextAutocompletion(const string16& input) OVERRIDE;
  virtual string16 GetGrayTextAutocompletion() const OVERRIDE;
  virtual int TextWidth() const OVERRIDE;
  virtual bool IsImeComposing() const OVERRIDE;
  virtual bool IsImeShowingPopup() const OVERRIDE;
  virtual int GetMaxEditWidth(int entry_width) const OVERRIDE;
  virtual views::View* AddToView(views::View* parent) OVERRIDE;
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE;

  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE;
  virtual void OnBeforeUserAction(views::Textfield* sender) OVERRIDE;
  virtual void OnAfterUserAction(views::Textfield* sender) OVERRIDE;
  virtual void OnAfterCutOrCopy() OVERRIDE;
  virtual void OnWriteDragData(ui::OSExchangeData* data) OVERRIDE;
  virtual void OnGetDragOperationsForTextfield(int* drag_operations) OVERRIDE;
  virtual void AppendDropFormats(
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual int OnDrop(const ui::OSExchangeData& data) OVERRIDE;
  virtual void UpdateContextMenu(ui::SimpleMenuModel* menu_contents) OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE;
  virtual bool HandlesCommand(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

#if defined(OS_CHROMEOS)
  // chromeos::input_method::InputMethodManager::CandidateWindowObserver:
  virtual void CandidateWindowOpened(
      chromeos::input_method::InputMethodManager* manager) OVERRIDE;
  virtual void CandidateWindowClosed(
      chromeos::input_method::InputMethodManager* manager) OVERRIDE;
#endif

 private:
  // Return the number of characers in the current buffer.
  virtual int GetOmniboxTextLength() const OVERRIDE;

  // Try to parse the current text as a URL and colorize the components.
  virtual void EmphasizeURLComponents() OVERRIDE;

  // Update the field with |text| and set the selection.
  void SetTextAndSelectedRange(const string16& text,
                               const gfx::Range& range);

  // Returns the selected text.
  string16 GetSelectedText() const;

  // Paste text from the clipboard into the omnibox.
  // Textfields implementation of Paste() pastes the contents of the clipboard
  // as is. We want to strip whitespace and other things (see GetClipboardText()
  // for details).
  // It is assumed this is invoked after a call to OnBeforePossibleChange() and
  // that after invoking this OnAfterPossibleChange() is invoked.
  void OnPaste();

  // Handle keyword hint tab-to-search and tabbing through dropdown results.
  bool HandleEarlyTabActions(const ui::KeyEvent& event);

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (smaller font size). This is used for popups.
  bool popup_window_mode_;

  scoped_ptr<OmniboxPopupView> popup_view_;

  ToolbarModel::SecurityLevel security_level_;

  // Selection persisted across temporary text changes, like popup suggestions.
  gfx::Range saved_temporary_selection_;

  // Holds the user's selection across focus changes.  There is only a saved
  // selection if this range IsValid().
  gfx::Range saved_selection_for_focus_change_;

  // Tracking state before and after a possible change.
  string16 text_before_change_;
  gfx::Range sel_before_change_;
  bool ime_composing_before_change_;

  // Was the delete key pressed with an empty selection at the end of the edit?
  bool delete_at_end_pressed_;
  LocationBarView* location_bar_view_;

  // True if the IME candidate window is open. When this is true, we want to
  // avoid showing the popup. So far, the candidate window is detected only
  // on Chrome OS.
  bool ime_candidate_window_open_;

  // Should we select all the text when we see the mouse button get released?
  // We select in response to a click that focuses the omnibox, but we defer
  // until release, setting this variable back to false if we saw a drag, to
  // allow the user to select just a portion of the text.
  bool select_all_on_mouse_release_;

  // Indicates if we want to select all text in the omnibox when we get a
  // GESTURE_TAP. We want to select all only when the textfield is not in focus
  // and gets a tap. So we use this variable to remember focus state before tap.
  bool select_all_on_gesture_tap_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_VIEWS_H_
