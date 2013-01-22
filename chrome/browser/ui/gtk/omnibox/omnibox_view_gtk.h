// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OMNIBOX_OMNIBOX_VIEW_GTK_H_
#define CHROME_BROWSER_UI_GTK_OMNIBOX_OMNIBOX_VIEW_GTK_H_

#include <gtk/gtk.h>

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"

class Browser;
class OmniboxPopupView;
class Profile;

namespace gfx {
class Font;
}

class GtkThemeService;

class OmniboxViewGtk : public OmniboxView,
                       public content::NotificationObserver {
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

  OmniboxViewGtk(OmniboxEditController* controller,
                 ToolbarModel* toolbar_model,
                 Browser* browser,
                 CommandUpdater* command_updater,
                 bool popup_window_mode,
                 GtkWidget* location_bar);
  virtual ~OmniboxViewGtk();

  // Initialize, create the underlying widgets, etc.
  void Init();
  // Returns the width in pixels needed to display the text from one character
  // before the caret to the end of the string. See comments in
  // LocationBarView::Layout as to why this uses -1.
  int WidthOfTextAfterCursor();

  // OmniboxView:
  virtual void SaveStateToTab(content::WebContents* tab) OVERRIDE;
  virtual void Update(
      const content::WebContents* tab_for_state_restoring) OVERRIDE;
  virtual string16 GetText() const OVERRIDE;
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
  virtual void UpdatePopup() OVERRIDE;
  virtual void SetFocus() OVERRIDE;
  virtual void ApplyCaretVisibility() OVERRIDE;
  virtual void OnTemporaryTextMaybeChanged(
      const string16& display_text,
      bool save_original_selection) OVERRIDE;
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const string16& display_text, size_t user_text_length) OVERRIDE;
  virtual void OnRevertTemporaryText() OVERRIDE;
  virtual void OnBeforePossibleChange() OVERRIDE;
  virtual bool OnAfterPossibleChange() OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetRelativeWindowForPopup() const OVERRIDE;
  virtual void SetInstantSuggestion(const string16& suggestion) OVERRIDE;
  virtual string16 GetInstantSuggestion() const OVERRIDE;
  virtual int TextWidth() const OVERRIDE;
  virtual bool IsImeComposing() const OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Sets the colors of the text view according to the theme.
  void SetBaseColor();
  // Sets the colors of the Instant suggestion view according to the theme.
  void UpdateInstantViewColors();

  // Returns the text view gtk widget. May return NULL if the widget
  // has already been destroyed.
  GtkWidget* text_view() {
    return text_view_;
  }

 private:
  CHROMEG_CALLBACK_0(OmniboxViewGtk, void, HandleBeginUserAction,
                     GtkTextBuffer*);
  CHROMEG_CALLBACK_0(OmniboxViewGtk, void, HandleEndUserAction, GtkTextBuffer*);
  CHROMEG_CALLBACK_2(OmniboxViewGtk, void, HandleMarkSet, GtkTextBuffer*,
                     GtkTextIter*, GtkTextMark*);
  // As above, but called after the default handler.
  CHROMEG_CALLBACK_2(OmniboxViewGtk, void, HandleMarkSetAfter, GtkTextBuffer*,
                     GtkTextIter*, GtkTextMark*);
  CHROMEG_CALLBACK_3(OmniboxViewGtk, void, HandleInsertText, GtkTextBuffer*,
                     GtkTextIter*, const gchar*, gint);
  CHROMEG_CALLBACK_0(OmniboxViewGtk, void, HandleKeymapDirectionChanged,
                     GdkKeymap*);
  CHROMEG_CALLBACK_2(OmniboxViewGtk, void, HandleDeleteRange, GtkTextBuffer*,
                     GtkTextIter*, GtkTextIter*);
  // Unlike above HandleMarkSet and HandleMarkSetAfter, this handler will always
  // be connected to the signal.
  CHROMEG_CALLBACK_2(OmniboxViewGtk, void, HandleMarkSetAlways, GtkTextBuffer*,
                     GtkTextIter*, GtkTextMark*);

  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, gboolean, HandleKeyPress, GdkEventKey*);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, gboolean, HandleKeyRelease,
                       GdkEventKey*);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, gboolean, HandleViewButtonPress,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, gboolean, HandleViewButtonRelease,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, gboolean, HandleViewFocusIn,
                       GdkEventFocus*);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, gboolean, HandleViewFocusOut,
                       GdkEventFocus*);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, void, HandleViewMoveFocus,
                       GtkDirectionType);
  CHROMEGTK_CALLBACK_3(OmniboxViewGtk, void, HandleViewMoveCursor,
                       GtkMovementStep, gint, gboolean);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, void, HandleViewSizeRequest,
                       GtkRequisition*);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, void, HandlePopulatePopup, GtkMenu*);
  CHROMEGTK_CALLBACK_0(OmniboxViewGtk, void, HandleEditSearchEngines);
  CHROMEGTK_CALLBACK_0(OmniboxViewGtk, void, HandlePasteAndGo);
  CHROMEGTK_CALLBACK_6(OmniboxViewGtk, void, HandleDragDataReceived,
                       GdkDragContext*, gint, gint, GtkSelectionData*,
                       guint, guint);
  CHROMEGTK_CALLBACK_4(OmniboxViewGtk, void, HandleDragDataGet,
                       GdkDragContext*, GtkSelectionData*, guint, guint);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, void, HandleDragBegin,
                       GdkDragContext*);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, void, HandleDragEnd,
                       GdkDragContext*);
  CHROMEGTK_CALLBACK_0(OmniboxViewGtk, void, HandleBackSpace);
  CHROMEGTK_CALLBACK_0(OmniboxViewGtk, void, HandleCopyClipboard);
  CHROMEGTK_CALLBACK_0(OmniboxViewGtk, void, HandleCopyURLClipboard);
  CHROMEGTK_CALLBACK_0(OmniboxViewGtk, void, HandleCutClipboard);
  CHROMEGTK_CALLBACK_0(OmniboxViewGtk, void, HandlePasteClipboard);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, gboolean, HandleExposeEvent,
                       GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, void, HandleWidgetDirectionChanged,
                       GtkTextDirection);
  CHROMEGTK_CALLBACK_2(OmniboxViewGtk, void, HandleDeleteFromCursor,
                       GtkDeleteType, gint);
  // We connect to this so we can determine our toplevel window, so we can
  // listen to focus change events on it.
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, void, HandleHierarchyChanged,
                       GtkWidget*);
  CHROMEGTK_CALLBACK_1(OmniboxViewGtk, void, HandlePreEditChanged,
                       const gchar*);
  // Undo/redo operations won't trigger "begin-user-action" and
  // "end-user-action" signals, so we need to hook into "undo" and "redo"
  // signals and call OnBeforePossibleChange()/OnAfterPossibleChange() by
  // ourselves.
  CHROMEGTK_CALLBACK_0(OmniboxViewGtk, void, HandleUndoRedo);
  CHROMEGTK_CALLBACK_0(OmniboxViewGtk, void, HandleUndoRedoAfter);

  CHROMEG_CALLBACK_1(OmniboxViewGtk, void, HandleWindowSetFocus,
                     GtkWindow*, GtkWidget*);

  // Callback function called after context menu is closed.
  CHROMEGTK_CALLBACK_0(OmniboxViewGtk, void, HandlePopupMenuDeactivate);

  // Callback for the PRIMARY selection clipboard.
  static void ClipboardGetSelectionThunk(GtkClipboard* clipboard,
                                         GtkSelectionData* selection_data,
                                         guint info,
                                         gpointer object);
  void ClipboardGetSelection(GtkClipboard* clipboard,
                             GtkSelectionData* selection_data,
                             guint info);

  void HandleCopyOrCutClipboard(bool copy);

  // OmniboxView overrides.
  virtual int GetOmniboxTextLength() const OVERRIDE;
  virtual void EmphasizeURLComponents() OVERRIDE;

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

  // Returns true if the caret is at the end of the content.
  bool IsCaretAtEnd() const;

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

  // The Browser that contains this omnibox.
  Browser* browser_;

  // The widget we expose, used for vertically centering the real text edit,
  // since the height will change based on the font / font size, etc.
  ui::OwnedWidgetGtk alignment_;

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

  // Objects for the Instant suggestion text view.
  GtkTextTag* instant_anchor_tag_;

  // A widget for displaying Instant suggestion text. It'll be attached to a
  // child anchor in the |text_buffer_| object.
  GtkWidget* instant_view_;

  // A mark to split the content and the Instant anchor. Wherever the end
  // iterator of the text buffer is required, the iterator to this mark should
  // be used.
  GtkTextMark* instant_mark_;

  scoped_ptr<OmniboxPopupView> popup_view_;

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

  std::string dragged_text_;
  // When we own the X clipboard, this is the text for it.
  std::string primary_selection_text_;

  // IDs of the signal handlers for "mark-set" on |text_buffer_|.
  gulong mark_set_handler_id_;
  gulong mark_set_handler_id2_;

  // Is the first mouse button currently down?  When selection marks get moved,
  // we use this to determine if the user was highlighting text with the mouse
  // -- if so, we avoid selecting all the text on mouse-up.
  bool button_1_pressed_;

  // Supplies colors, et cetera.
  GtkThemeService* theme_service_;

  content::NotificationRegistrar registrar_;

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

  // Indicates if Shift key was pressed.
  // Used in conjunction with the Tab key to determine if either traversal
  // needs to move up the results or if the keyword needs to be cleared.
  bool shift_was_pressed_;

  // Indicates that user requested to paste clipboard.
  // The actual paste clipboard action might be performed later if the
  // clipboard is not empty.
  bool paste_clipboard_requested_;

  // Text to "Paste and go"; set by HandlePopulatePopup() and consumed by
  // HandlePasteAndGo().
  string16 sanitized_text_for_paste_and_go_;

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
  // including changes to content text, selection range and pre-edit string.
  // Changes caused by function calls like SetUserText() should not affect this
  // flag.
  bool content_maybe_changed_by_key_press_;

  // Set this flag to call UpdatePopup() in lost focus and need to update.
  // Because context menu might take the focus, before setting the flag, check
  // the focus with model_->has_focus().
  bool update_popup_without_focus_;

  // On GTK 2.20+ |pre_edit_| and |pre_edit_size_before_change_| will be used.
  const bool supports_pre_edit_;

  // Stores the text being composed by the input method.
  string16 pre_edit_;

  // Tracking pre-edit state before and after a possible change. We don't need
  // to track pre-edit_'s content, as it'll be treated as part of text content.
  size_t pre_edit_size_before_change_;

  // The view that is going to be focused next. Only valid while handling
  // "focus-out" events.
  GtkWidget* going_to_focus_;

  ui::GtkSignalRegistrar signals_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_OMNIBOX_OMNIBOX_VIEW_GTK_H_
