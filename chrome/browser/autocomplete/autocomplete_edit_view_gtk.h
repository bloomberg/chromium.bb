// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_GTK_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/page_transition_types.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditController;
class AutocompleteEditModel;
class AutocompletePopupView;
class Profile;
class TabContents;

namespace gfx {
class Font;
}

namespace ui {
class MultiAnimation;
}

namespace views {
class View;
}

#if !defined(TOOLKIT_VIEWS)
class GtkThemeProvider;
#endif

class AutocompleteEditViewGtk : public AutocompleteEditView,
                                public NotificationObserver,
                                public ui::AnimationDelegate {
 public:
  // Modeled like the Windows CHARRANGE.  Represent a pair of cursor position
  // offsets.  Since GtkTextIters are invalid after the buffer is changed, we
  // work in character offsets (not bytes).
  struct CharRange {
    CharRange() : cp_min(0), cp_max(0) { }
    CharRange(int n, int x) : cp_min(n), cp_max(x) { }

    // Returns the start/end of the selection.
    int selection_min() const { return std::min(cp_min, cp_max); }
    int selection_max() const { return std::max(cp_min, cp_max); }

    // Work in integers to match the gint GTK APIs.
    int cp_min;  // For a selection: Represents the start.
    int cp_max;  // For a selection: Represents the end (insert position).
  };

  AutocompleteEditViewGtk(AutocompleteEditController* controller,
                          ToolbarModel* toolbar_model,
                          Profile* profile,
                          CommandUpdater* command_updater,
                          bool popup_window_mode,
#if defined(TOOLKIT_VIEWS)
                          const views::View* location_bar
#else
                          GtkWidget* location_bar
#endif
                          );
  virtual ~AutocompleteEditViewGtk();

  // Initialize, create the underlying widgets, etc.
  void Init();
  // Returns the width in pixels needed to display the text from one character
  // before the caret to the end of the string. See comments in
  // LocationBarView::Layout as to why this uses -1.
  int WidthOfTextAfterCursor();

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
  virtual void SetInstantSuggestion(const string16& suggestion);
  virtual string16 GetInstantSuggestion() const;
  virtual int TextWidth() const;
  virtual bool IsImeComposing() const;

#if defined(TOOLKIT_VIEWS)
  virtual views::View* AddToView(views::View* parent);
  virtual int OnPerformDrop(const views::DropTargetEvent& event);

  // A factory method to create an AutocompleteEditView instance initialized for
  // linux_views.  This currently returns an instance of
  // AutocompleteEditViewGtk only, but AutocompleteEditViewViews will
  // be added as an option when TextfieldViews is enabled.
  static AutocompleteEditView* Create(AutocompleteEditController* controller,
                                      ToolbarModel* toolbar_model,
                                      Profile* profile,
                                      CommandUpdater* command_updater,
                                      bool popup_window_mode,
                                      const views::View* location_bar);
#endif

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Overridden from ui::AnimationDelegate.
  virtual void AnimationEnded(const ui::Animation* animation);
  virtual void AnimationProgressed(const ui::Animation* animation);
  virtual void AnimationCanceled(const ui::Animation* animation);

  // Sets the colors of the text view according to the theme.
  void SetBaseColor();
  // Sets the colors of the instant suggestion view according to the theme and
  // the animation state.
  void UpdateInstantViewColors();

  // Returns the text view gtk widget. May return NULL if the widget
  // has already been destroyed.
  GtkWidget* text_view() {
    return text_view_;
  }

 private:
  CHROMEG_CALLBACK_0(AutocompleteEditViewGtk, void, HandleBeginUserAction,
                     GtkTextBuffer*);
  CHROMEG_CALLBACK_0(AutocompleteEditViewGtk, void, HandleEndUserAction,
                     GtkTextBuffer*);
  CHROMEG_CALLBACK_2(AutocompleteEditViewGtk, void, HandleMarkSet,
                     GtkTextBuffer*, GtkTextIter*, GtkTextMark*);
  // As above, but called after the default handler.
  CHROMEG_CALLBACK_2(AutocompleteEditViewGtk, void, HandleMarkSetAfter,
                     GtkTextBuffer*, GtkTextIter*, GtkTextMark*);
  CHROMEG_CALLBACK_3(AutocompleteEditViewGtk, void, HandleInsertText,
                     GtkTextBuffer*, GtkTextIter*, const gchar*, gint);
  CHROMEG_CALLBACK_0(AutocompleteEditViewGtk, void,
                     HandleKeymapDirectionChanged, GdkKeymap*);
  CHROMEG_CALLBACK_2(AutocompleteEditViewGtk, void, HandleDeleteRange,
                     GtkTextBuffer*, GtkTextIter*, GtkTextIter*);
  // Unlike above HandleMarkSet and HandleMarkSetAfter, this handler will always
  // be connected to the signal.
  CHROMEG_CALLBACK_2(AutocompleteEditViewGtk, void, HandleMarkSetAlways,
                     GtkTextBuffer*, GtkTextIter*, GtkTextMark*);

  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, gboolean, HandleKeyPress,
                       GdkEventKey*);
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, gboolean, HandleKeyRelease,
                       GdkEventKey*);
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, gboolean, HandleViewButtonPress,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, gboolean,
                       HandleViewButtonRelease, GdkEventButton*);
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, gboolean, HandleViewFocusIn,
                       GdkEventFocus*);
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, gboolean, HandleViewFocusOut,
                       GdkEventFocus*);
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, void, HandleViewMoveFocus,
                       GtkDirectionType);
  CHROMEGTK_CALLBACK_3(AutocompleteEditViewGtk, void, HandleViewMoveCursor,
                       GtkMovementStep, gint, gboolean);
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, void, HandleViewSizeRequest,
                       GtkRequisition*);
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, void, HandlePopulatePopup,
                       GtkMenu*);
  CHROMEGTK_CALLBACK_0(AutocompleteEditViewGtk, void, HandleEditSearchEngines);
  CHROMEGTK_CALLBACK_0(AutocompleteEditViewGtk, void, HandlePasteAndGo);
  CHROMEGTK_CALLBACK_6(AutocompleteEditViewGtk, void, HandleDragDataReceived,
                       GdkDragContext*, gint, gint, GtkSelectionData*,
                       guint, guint);
  CHROMEGTK_CALLBACK_4(AutocompleteEditViewGtk, void, HandleDragDataGet,
                       GdkDragContext*, GtkSelectionData*, guint, guint);
  CHROMEGTK_CALLBACK_0(AutocompleteEditViewGtk, void, HandleBackSpace);
  CHROMEGTK_CALLBACK_0(AutocompleteEditViewGtk, void, HandleCopyClipboard);
  CHROMEGTK_CALLBACK_0(AutocompleteEditViewGtk, void, HandleCutClipboard);
  CHROMEGTK_CALLBACK_0(AutocompleteEditViewGtk, void, HandlePasteClipboard);
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, gboolean, HandleExposeEvent,
                       GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, void,
                       HandleWidgetDirectionChanged, GtkTextDirection);
  CHROMEGTK_CALLBACK_2(AutocompleteEditViewGtk, void,
                       HandleDeleteFromCursor, GtkDeleteType, gint);
  // We connect to this so we can determine our toplevel window, so we can
  // listen to focus change events on it.
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, void, HandleHierarchyChanged,
                       GtkWidget*);
#if GTK_CHECK_VERSION(2, 20, 0)
  CHROMEGTK_CALLBACK_1(AutocompleteEditViewGtk, void, HandlePreeditChanged,
                       const gchar*);
#endif
  // Undo/redo operations won't trigger "begin-user-action" and
  // "end-user-action" signals, so we need to hook into "undo" and "redo"
  // signals and call OnBeforePossibleChange()/OnAfterPossibleChange() by
  // ourselves.
  CHROMEGTK_CALLBACK_0(AutocompleteEditViewGtk, void, HandleUndoRedo);
  CHROMEGTK_CALLBACK_0(AutocompleteEditViewGtk, void, HandleUndoRedoAfter);

  CHROMEG_CALLBACK_1(AutocompleteEditViewGtk, void, HandleWindowSetFocus,
                     GtkWindow*, GtkWidget*);

  // Callback function called after context menu is closed.
  CHROMEGTK_CALLBACK_0(AutocompleteEditViewGtk, void,
                       HandlePopupMenuDeactivate);

  // Callback for the PRIMARY selection clipboard.
  static void ClipboardGetSelectionThunk(GtkClipboard* clipboard,
                                         GtkSelectionData* selection_data,
                                         guint info,
                                         gpointer object);
  void ClipboardGetSelection(GtkClipboard* clipboard,
                             GtkSelectionData* selection_data,
                             guint info);

  void HandleCopyOrCutClipboard(bool copy);

  // Common implementation for performing a drop on the edit view.
  bool OnPerformDropImpl(const string16& text);

  // Returns the font used in |text_view_|.
  gfx::Font GetFont();

  // Take control of the PRIMARY selection clipboard with |text|. Use
  // |text_buffer_| as the owner, so that this doesn't remove the selection on
  // it. This makes use of the above callbacks.
  void OwnPrimarySelection(const std::string& text);

  // Gets the GTK_TEXT_WINDOW_WIDGET coordinates for |text_view_| that bound the
  // given iters.
  gfx::Rect WindowBoundsFromIters(GtkTextIter* iter1, GtkTextIter* iter2);

  // Actual implementation of SelectAll(), but also provides control over
  // whether the PRIMARY selection is set to the selected text (in SelectAll(),
  // it isn't, but we want set the selection when the user clicks in the entry).
  void SelectAllInternal(bool reversed, bool update_primary_selection);

  // Get ready to update |text_buffer_|'s highlighting without making changes to
  // the PRIMARY selection.  Removes the clipboard from |text_buffer_| and
  // blocks the "mark-set" signal handler.
  void StartUpdatingHighlightedText();

  // Finish updating |text_buffer_|'s highlighting such that future changes will
  // automatically update the PRIMARY selection.  Undoes
  // StartUpdatingHighlightedText()'s changes.
  void FinishUpdatingHighlightedText();

  // Get the character indices of the current selection.  This honors
  // direction, cp_max is the insertion point, and cp_min is the bound.
  CharRange GetSelection() const;

  // Translate from character positions to iterators for the current buffer.
  void ItersFromCharRange(const CharRange& range,
                          GtkTextIter* iter_min,
                          GtkTextIter* iter_max);

  // Return the number of characers in the current buffer.
  int GetTextLength() const;

  // Places the caret at the given position. This clears any selection.
  void PlaceCaretAt(int pos);

  // Returns true if the caret is at the end of the content.
  bool IsCaretAtEnd() const;

  // Try to parse the current text as a URL and colorize the components.
  void EmphasizeURLComponents();

  // Internally invoked whenever the text changes in some way.
  void TextChanged();

  // Save |selected_text| as the PRIMARY X selection. Unlike
  // OwnPrimarySelection(), this won't set an owner or use callbacks.
  void SavePrimarySelection(const std::string& selected_text);

  // Update the field with |text| and set the selection.
  void SetTextAndSelectedRange(const string16& text,
                               const CharRange& range);

  // Set the selection to |range|.
  void SetSelectedRange(const CharRange& range);

  // Adjust the text justification according to the text direction of the widget
  // and |text_buffer_|'s content, to make sure the real text justification is
  // always in sync with the UI language direction.
  void AdjustTextJustification();

  // Get the text direction of |text_buffer_|'s content, by searching the first
  // character that has a strong direction.
  PangoDirection GetContentDirection();

  // Returns the selected text.
  std::string GetSelectedText() const;

  // If the selected text parses as a URL OwnPrimarySelection is invoked.
  void UpdatePrimarySelectionIfValidURL();

  // Retrieves the first and last iterators in the |text_buffer_|, but excludes
  // the anchor holding the |instant_view_| widget.
  void GetTextBufferBounds(GtkTextIter* start, GtkTextIter* end) const;

  // Validates an iterator in the |text_buffer_|, to make sure it doesn't go
  // beyond the anchor for holding the |instant_view_| widget.
  void ValidateTextBufferIter(GtkTextIter* iter) const;

  // Adjusts vertical alignment of the |instant_view_| in the |text_view_|, to
  // make sure they have the same baseline.
  void AdjustVerticalAlignmentOfInstantView();

  // Stop showing the instant suggest auto-commit animation.
  void StopAnimation();

  // The widget we expose, used for vertically centering the real text edit,
  // since the height will change based on the font / font size, etc.
  OwnedWidgetGtk alignment_;

  // The actual text entry which will be owned by the alignment_.  The
  // reference will be set to NULL upon destruction to tell if the gtk
  // widget tree has been destroyed. This is because gtk destroies child
  // widgets if the parent (alignemtn_)'s refcount does not go down to 0.
  GtkWidget* text_view_;

  GtkTextTagTable* tag_table_;
  GtkTextBuffer* text_buffer_;
  GtkTextTag* faded_text_tag_;
  GtkTextTag* secure_scheme_tag_;
  GtkTextTag* security_error_scheme_tag_;
  GtkTextTag* normal_text_tag_;

  // Objects for the instant suggestion text view.
  GtkTextTag* instant_anchor_tag_;

  // A widget for displaying instant suggestion text. It'll be attached to a
  // child anchor in the |text_buffer_| object.
  GtkWidget* instant_view_;
  // Animation from instant suggest (faded text) to autocomplete (selected
  // text).
  scoped_ptr<ui::MultiAnimation> instant_animation_;

  // A mark to split the content and the instant anchor. Wherever the end
  // iterator of the text buffer is required, the iterator to this mark should
  // be used.
  GtkTextMark* instant_mark_;

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
  CharRange saved_temporary_selection_;

  // Tracking state before and after a possible change.
  string16 text_before_change_;
  CharRange sel_before_change_;

  // The most-recently-selected text from the entry that was copied to the
  // clipboard.  This is updated on-the-fly as the user selects text. This may
  // differ from the actual selected text, such as when 'http://' is prefixed to
  // the text.  It is used in cases where we need to make the PRIMARY selection
  // persist even after the user has unhighlighted the text in the view
  // (e.g. when they highlight some text and then click to unhighlight it, we
  // pass this string to SavePrimarySelection()).
  std::string selected_text_;

  // When we own the X clipboard, this is the text for it.
  std::string primary_selection_text_;

  // IDs of the signal handlers for "mark-set" on |text_buffer_|.
  gulong mark_set_handler_id_;
  gulong mark_set_handler_id2_;

#if defined(OS_CHROMEOS)
  // The following variables are used to implement select-all-on-mouse-up, which
  // is disabled in the standard Linux build due to poor interaction with the
  // PRIMARY X selection.

  // Is the first mouse button currently down?  When selection marks get moved,
  // we use this to determine if the user was highlighting text with the mouse
  // -- if so, we avoid selecting all the text on mouse-up.
  bool button_1_pressed_;

  // Did the user change the selected text in the middle of the current click?
  // If so, we don't select all of the text when the button is released -- we
  // don't want to blow away their selection.
  bool text_selected_during_click_;

  // Was the text view already focused before the user clicked in it?  We use
  // this to figure out whether we should select all of the text when the button
  // is released (we only do so if the view was initially unfocused).
  bool text_view_focused_before_button_press_;
#endif

#if !defined(TOOLKIT_VIEWS)
  // Supplies colors, et cetera.
  GtkThemeProvider* theme_provider_;

  NotificationRegistrar registrar_;
#endif

  // Indicates if Enter key was pressed.
  //
  // It's used in the key press handler to detect an Enter key press event
  // during sync dispatch of "end-user-action" signal so that an unexpected
  // change caused by the event can be ignored in OnAfterPossibleChange().
  bool enter_was_pressed_;

  // Indicates if Tab key was pressed.
  //
  // It's only used in the key press handler to detect a Tab key press event
  // during sync dispatch of "move-focus" signal.
  bool tab_was_pressed_;

  // Indicates that user requested to paste clipboard.
  // The actual paste clipboard action might be performed later if the
  // clipboard is not empty.
  bool paste_clipboard_requested_;

  // Indicates if an Enter key press is inserted as text.
  // It's used in the key press handler to determine if an Enter key event is
  // handled by IME or not.
  bool enter_was_inserted_;

  // Indicates whether the IME changed the text.  It's possible for the IME to
  // handle a key event but not change the text contents (e.g., when pressing
  // shift+del with no selection).
  bool text_changed_;

  // Contains the character range that should have a strikethrough (used for
  // insecure schemes). If the range is size one or less, no strikethrough
  // is needed.
  CharRange strikethrough_;

  // Indicates if the selected text is suggested text or not. If the selection
  // is not suggested text, that means the user manually made the selection.
  bool selection_suggested_;

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

  // Set this flag to call UpdatePopup() in lost focus and need to update.
  // Because context menu might take the focus, before setting the flag, check
  // the focus with model_->has_focus().
  bool update_popup_without_focus_;

#if GTK_CHECK_VERSION(2, 20, 0)
  // Stores the text being composed by the input method.
  string16 preedit_;

  // Tracking preedit state before and after a possible change. We don't need to
  // track preedit_'s content, as it'll be treated as part of text content.
  size_t preedit_size_before_change_;
#endif

  // The view that is going to be focused next. Only valid while handling
  // "focus-out" events.
  GtkWidget* going_to_focus_;

  ui::GtkSignalRegistrar signals_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteEditViewGtk);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_GTK_H_
