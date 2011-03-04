// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_VIEWS_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_VIEWS_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/page_transition_types.h"
#include "views/controls/textfield/textfield.h"
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
                                  public views::Textfield::Controller {
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
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual void Layout();

  // Implement the AutocompleteEditView interface.
  virtual AutocompleteEditModel* model();
  virtual const AutocompleteEditModel* model() const;

  virtual void SaveStateToTab(TabContents* tab);

  virtual void Update(const TabContents* tab_for_state_restoring);

  virtual void OpenURL(const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition,
                       const GURL& alternate_nav_url,
                       size_t selected_line,
                       const string16& keyword);

  virtual string16 GetText() const;

  virtual bool IsEditingOrEmpty() const;
  virtual int GetIcon() const;
  virtual void SetUserText(const string16& text);
  virtual void SetUserText(const string16& text,
                           const string16& display_text,
                           bool update_popup);
  virtual void SetWindowTextAndCaretPos(const string16& text,
                                        size_t caret_pos);
  virtual void SetForcedQuery();
  virtual bool IsSelectAll();
  virtual bool DeleteAtEndPressed();
  virtual void GetSelectionBounds(string16::size_type* start,
                                  string16::size_type* end);
  virtual void SelectAll(bool reversed);
  virtual void RevertAll();
  virtual void UpdatePopup();
  virtual void ClosePopup();
  virtual void SetFocus();
  virtual void OnTemporaryTextMaybeChanged(const string16& display_text,
                                           bool save_original_selection);
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const string16& display_text, size_t user_text_length);
  virtual void OnRevertTemporaryText();
  virtual void OnBeforePossibleChange();
  virtual bool OnAfterPossibleChange();
  virtual gfx::NativeView GetNativeView() const;
  virtual CommandUpdater* GetCommandUpdater();
  virtual void SetInstantSuggestion(const string16& input);
  virtual string16 GetInstantSuggestion() const;
  virtual int TextWidth() const;
  virtual bool IsImeComposing() const;
  virtual views::View* AddToView(views::View* parent);
  virtual int OnPerformDrop(const views::DropTargetEvent& event);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from Textfield::Controller
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents);
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& key_event);

 private:
  // Return the number of characers in the current buffer.
  size_t GetTextLength() const;

  // Try to parse the current text as a URL and colorize the components.
  void EmphasizeURLComponents();

  // Internally invoked whenever the text changes in some way.
  void TextChanged();

  // Update the field with |text| and set the selection.
  void SetTextAndSelectedRange(const string16& text,
                               const views::TextRange& range);

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
  views::TextRange saved_temporary_selection_;

  // Tracking state before and after a possible change.
  string16 text_before_change_;
  views::TextRange sel_before_change_;

  // TODO(oshima): following flags are copied from gtk implementation.
  // It should be possible to refactor this class to simplify flags and
  // logic. I'll work on this refactoring once all features are completed.

  // Indicates whether the IME changed the text.  It's possible for the IME to
  // handle a key event but not change the text contents (e.g., when pressing
  // shift+del with no selection).
  bool text_changed_;

  // Was delete pressed?
  bool delete_was_pressed_;

  // Was the delete key pressed with an empty selection at the end of the edit?
  bool delete_at_end_pressed_;

  // Indicates if we are handling a key press event.
  bool handling_key_press_;

  // Indicates if omnibox's content maybe changed by a key press event, so that
  // we need to call OnAfterPossibleChange() after handling the event.
  // This flag should be set for changes directly caused by a key press event,
  // including changes to content text, selection range and preedit string.
  // Changes caused by function calls like SetUserText() should not affect this
  // flag.
  bool content_maybe_changed_by_key_press_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteEditViewViews);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_VIEWS_H_
