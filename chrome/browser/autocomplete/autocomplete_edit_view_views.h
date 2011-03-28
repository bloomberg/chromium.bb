// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_VIEWS_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_VIEWS_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "content/common/notification_observer.h"
#include "content/common/page_transition_types.h"
#include "ui/base/range/range.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/view.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditController;
class AutocompleteEditModel;
class AutocompletePopupView;
class Profile;
class TabContents;

// Views-implementation of AutocompleteEditView. This is based on
// gtk implementation. The following features are not yet supported.
//
// IME support.
// LTR support.
// Selection behavior.
// Cut,copy and paste behavior.
// Drag and drop behavior.
// URL styles (strikestrough insecure scheme, emphasize host).
// Custom context menu for omnibox.
// Instant.
class AutocompleteEditViewViews : public views::View,
                                  public AutocompleteEditView,
                                  public NotificationObserver,
                                  public views::TextfieldController {
 public:
  AutocompleteEditViewViews(AutocompleteEditController* controller,
                            ToolbarModel* toolbar_model,
                            Profile* profile,
                            CommandUpdater* command_updater,
                            bool popup_window_mode,
                            const views::View* location_bar);
  virtual ~AutocompleteEditViewViews();

  // Initialize, create the underlying views, etc;
  void Init();

  // Sets the colors of the text view according to the theme.
  void SetBaseColor();

  // Called after key even is handled either by HandleKeyEvent or by Textfield.
  bool HandleAfterKeyEvent(const views::KeyEvent& event, bool handled);

  // Called when KeyRelease event is generated on textfield.
  bool HandleKeyReleaseEvent(const views::KeyEvent& event);

  // Called when Focus is set/unset on textfield.
  void HandleFocusIn();
  void HandleFocusOut();

  // Implements views::View
  virtual void Layout() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Implement the AutocompleteEditView interface.
  virtual AutocompleteEditModel* model() OVERRIDE;
  virtual const AutocompleteEditModel* model() const OVERRIDE;

  virtual void SaveStateToTab(TabContents* tab) OVERRIDE;

  virtual void Update(const TabContents* tab_for_state_restoring) OVERRIDE;

  virtual void OpenURL(const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition,
                       const GURL& alternate_nav_url,
                       size_t selected_line,
                       const string16& keyword) OVERRIDE;

  virtual string16 GetText() const OVERRIDE;

  virtual bool IsEditingOrEmpty() const OVERRIDE;
  virtual int GetIcon() const OVERRIDE;
  virtual void SetUserText(const string16& text) OVERRIDE;
  virtual void SetUserText(const string16& text,
                           const string16& display_text,
                           bool update_popup) OVERRIDE;
  virtual void SetWindowTextAndCaretPos(const string16& text,
                                        size_t caret_pos) OVERRIDE;
  virtual void SetForcedQuery() OVERRIDE;
  virtual bool IsSelectAll() OVERRIDE;
  virtual bool DeleteAtEndPressed() OVERRIDE;
  virtual void GetSelectionBounds(string16::size_type* start,
                                  string16::size_type* end) OVERRIDE;
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
  virtual void OnRevertTemporaryText() OVERRIDE;
  virtual void OnBeforePossibleChange() OVERRIDE;
  virtual bool OnAfterPossibleChange() OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual CommandUpdater* GetCommandUpdater() OVERRIDE;
  virtual void SetInstantSuggestion(const string16& input,
                                    bool animate_to_complete) OVERRIDE;
  virtual string16 GetInstantSuggestion() const OVERRIDE;
  virtual int TextWidth() const OVERRIDE;
  virtual bool IsImeComposing() const OVERRIDE;
  virtual views::View* AddToView(views::View* parent) OVERRIDE;
  virtual int OnPerformDrop(const views::DropTargetEvent& event) OVERRIDE;

  // NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // views::TextfieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& key_event) OVERRIDE;
  virtual void OnBeforeUserAction(views::Textfield* sender) OVERRIDE;
  virtual void OnAfterUserAction(views::Textfield* sender) OVERRIDE;

 private:
  // Return the number of characers in the current buffer.
  size_t GetTextLength() const;

  // Try to parse the current text as a URL and colorize the components.
  void EmphasizeURLComponents();

  // Internally invoked whenever the text changes in some way.
  void TextChanged();

  // Update the field with |text| and set the selection.
  void SetTextAndSelectedRange(const string16& text,
                               const ui::Range& range);

  // Returns the selected text.
  string16 GetSelectedText() const;

  // Selects the text given by |caret| and |end|.
  void SelectRange(size_t caret, size_t end);

  AutocompletePopupView* CreatePopupView(Profile* profile,
                                         const View* location_bar);

  views::Textfield* textfield_;

  scoped_ptr<AutocompleteEditModel> model_;
  scoped_ptr<AutocompletePopupView> popup_view_;
  AutocompleteEditController* controller_;
  ToolbarModel* toolbar_model_;

  // The object that handles additional command functionality exposed on the
  // edit, such as invoking the keyword editor.
  CommandUpdater* command_updater_;

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (smaller font size). This is used for popups.
  bool popup_window_mode_;

  ToolbarModel::SecurityLevel security_level_;

  // Selection at the point where the user started using the
  // arrows to move around in the popup.
  ui::Range saved_temporary_selection_;

  // Tracking state before and after a possible change.
  string16 text_before_change_;
  ui::Range sel_before_change_;
  bool ime_composing_before_change_;

  // Was the delete key pressed with an empty selection at the end of the edit?
  bool delete_at_end_pressed_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteEditViewViews);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_VIEWS_H_
