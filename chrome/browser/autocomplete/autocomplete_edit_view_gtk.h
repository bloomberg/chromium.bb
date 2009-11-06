// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_GTK_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/autocomplete/autocomplete_edit_view.h"
#include "chrome/browser/toolbar_model.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditController;
class AutocompleteEditModel;
class AutocompletePopupView;
class BubblePositioner;
class CommandUpdater;
class Profile;
class TabContents;

#if !defined(TOOLKIT_VIEWS)
class GtkThemeProvider;
#endif

class AutocompleteEditViewGtk : public AutocompleteEditView,
                                public NotificationObserver {
 public:
  // Modeled like the Windows CHARRANGE.  Represent a pair of cursor position
  // offsets.  Since GtkTextIters are invalid after the buffer is changed, we
  // work in character offsets (not bytes).
  struct CharRange {
    CharRange() : cp_min(0), cp_max(0) { }
    CharRange(int n, int x) : cp_min(n), cp_max(x) { }

    // Work in integers to match the gint GTK APIs.
    int cp_min;  // For a selection: Represents the start.
    int cp_max;  // For a selection: Represents the end (insert position).
  };

  AutocompleteEditViewGtk(AutocompleteEditController* controller,
                          ToolbarModel* toolbar_model,
                          Profile* profile,
                          CommandUpdater* command_updater,
                          bool popup_window_mode,
                          const BubblePositioner* bubble_positioner);
  ~AutocompleteEditViewGtk();

  // Initialize, create the underlying widgets, etc.
  void Init();

  GtkWidget* widget() { return alignment_.get(); }

  // Returns the width, in pixels, needed to display the current text. The
  // returned value includes margins and borders.
  int TextWidth();

  // Implement the AutocompleteEditView interface.
  virtual AutocompleteEditModel* model() { return model_.get(); }
  virtual const AutocompleteEditModel* model() const { return model_.get(); }

  virtual void SaveStateToTab(TabContents* tab);

  virtual void Update(const TabContents* tab_for_state_restoring);

  virtual void OpenURL(const GURL& url,
                       WindowOpenDisposition disposition,
                       PageTransition::Type transition,
                       const GURL& alternate_nav_url,
                       size_t selected_line,
                       const std::wstring& keyword);

  virtual std::wstring GetText() const;

  virtual void SetUserText(const std::wstring& text) {
    SetUserText(text, text, true);
  }
  virtual void SetUserText(const std::wstring& text,
                           const std::wstring& display_text,
                           bool update_popup);

  virtual void SetWindowTextAndCaretPos(const std::wstring& text,
                                        size_t caret_pos);

  virtual void SetForcedQuery();

  virtual bool IsSelectAll();
  virtual void SelectAll(bool reversed);
  virtual void RevertAll();

  virtual void UpdatePopup();
  virtual void ClosePopup();

  virtual void SetFocus();

  virtual void OnTemporaryTextMaybeChanged(const std::wstring& display_text,
                                           bool save_original_selection);
  virtual bool OnInlineAutocompleteTextMaybeChanged(
      const std::wstring& display_text, size_t user_text_length);
  virtual void OnRevertTemporaryText();
  virtual void OnBeforePossibleChange();
  virtual bool OnAfterPossibleChange();
  virtual gfx::NativeView GetNativeView() const;

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void SetBaseColor();

 private:
  // TODO(deanm): Would be nice to insulate the thunkers better, etc.
  static void HandleBeginUserActionThunk(GtkTextBuffer* unused, gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->HandleBeginUserAction();
  }
  void HandleBeginUserAction();

  static void HandleEndUserActionThunk(GtkTextBuffer* unused, gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->HandleEndUserAction();
  }
  void HandleEndUserAction();

  static gboolean HandleKeyPressThunk(GtkWidget* widget,
                                      GdkEventKey* event,
                                      gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->HandleKeyPress(
        widget, event);
  }
  gboolean HandleKeyPress(GtkWidget* widget, GdkEventKey* event);

  static gboolean HandleKeyReleaseThunk(GtkWidget* widget,
                                        GdkEventKey* event,
                                        gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->HandleKeyRelease(
        widget, event);
  }
  gboolean HandleKeyRelease(GtkWidget* widget, GdkEventKey* event);

  static gboolean HandleViewButtonPressThunk(GtkWidget* view,
                                             GdkEventButton* event,
                                             gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewButtonPress(event);
  }
  gboolean HandleViewButtonPress(GdkEventButton* event);

  static gboolean HandleViewButtonReleaseThunk(GtkWidget* view,
                                               GdkEventButton* event,
                                               gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewButtonRelease(event);
  }
  gboolean HandleViewButtonRelease(GdkEventButton* event);

  static gboolean HandleViewFocusInThunk(GtkWidget* view,
                                          GdkEventFocus* event,
                                          gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewFocusIn();
  }
  gboolean HandleViewFocusIn();

  static gboolean HandleViewFocusOutThunk(GtkWidget* view,
                                          GdkEventFocus* event,
                                          gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewFocusOut();
  }
  gboolean HandleViewFocusOut();

  static void HandleViewMoveCursorThunk(GtkWidget* view,
                                        GtkMovementStep step,
                                        gint count,
                                        gboolean extend_selection,
                                        gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewMoveCursor(step, count, extend_selection);
  }
  void HandleViewMoveCursor(GtkMovementStep step,
                            gint count,
                            gboolean extendion_selection);

  static void HandleViewSizeRequestThunk(GtkWidget* view,
                                         GtkRequisition* req,
                                         gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewSizeRequest(req);
  }
  void HandleViewSizeRequest(GtkRequisition* req);

  static void HandlePopulatePopupThunk(GtkEntry* entry,
                                       GtkMenu* menu,
                                       gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandlePopulatePopup(menu);
  }
  void HandlePopulatePopup(GtkMenu* menu);

  static void HandleEditSearchEnginesThunk(GtkMenuItem* menuitem,
                                           gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleEditSearchEngines();
  }
  void HandleEditSearchEngines();

  static void HandlePasteAndGoThunk(GtkMenuItem* menuitem,
                                    AutocompleteEditViewGtk* self) {
    self->HandlePasteAndGo();
  }
  void HandlePasteAndGo();

  static void HandleMarkSetThunk(GtkTextBuffer* buffer,
                                 GtkTextIter* location,
                                 GtkTextMark* mark,
                                 gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleMarkSet(buffer, location, mark);
  }
  void HandleMarkSet(GtkTextBuffer* buffer,
                     GtkTextIter* location,
                     GtkTextMark* mark);

  static void HandleDragDataReceivedThunk(GtkWidget* widget,
                                          GdkDragContext* context,
                                          gint x, gint y,
                                          GtkSelectionData* selection_data,
                                          guint target_type,
                                          guint time,
                                          gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleDragDataReceived(context, x, y, selection_data, target_type,
                               time);
  }
  void HandleDragDataReceived(GdkDragContext* context, gint x, gint y,
                              GtkSelectionData* selection_data,
                              guint target_type, guint time);

  static void HandleInsertTextThunk(GtkTextBuffer* buffer,
                                    GtkTextIter* location,
                                    const gchar* text,
                                    gint len,
                                    gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleInsertText(buffer, location, text, len);
  }
  void HandleInsertText(GtkTextBuffer* buffer,
                        GtkTextIter* location,
                        const gchar* text,
                        gint len);

  static void HandleBackSpaceThunk(GtkTextView* text_view, gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->HandleBackSpace();
  }
  void HandleBackSpace();

  static void HandleViewMoveFocusThunk(GtkWidget* widget, GtkDirectionType dir,
                                       gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleViewMoveFocus(widget);
  }
  void HandleViewMoveFocus(GtkWidget* widget);

  static void HandleCopyClipboardThunk(GtkTextView* text_view, gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->HandleCopyClipboard();
  }
  void HandleCopyClipboard();

  static void HandlePasteClipboardThunk(GtkTextView* text_view, gpointer self) {
    reinterpret_cast<AutocompleteEditViewGtk*>(self)->HandlePasteClipboard();
  }
  void HandlePasteClipboard();

  static gboolean HandleExposeEventThunk(GtkTextView* text_view,
                                         GdkEventExpose* expose,
                                         gpointer self) {
    return reinterpret_cast<AutocompleteEditViewGtk*>(self)->
        HandleExposeEvent(expose);
  }
  gboolean HandleExposeEvent(GdkEventExpose* expose);

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
  CharRange GetSelection();

  // Translate from character positions to iterators for the current buffer.
  void ItersFromCharRange(const CharRange& range,
                          GtkTextIter* iter_min,
                          GtkTextIter* iter_max);

  // Return the number of characers in the current buffer.
  int GetTextLength();

  // Try to parse the current text as a URL and colorize the components.
  void EmphasizeURLComponents();

  // Internally invoked whenever the text changes in some way.
  void TextChanged();

  // Save |selected_text| as the PRIMARY X selection.
  void SavePrimarySelection(const std::string& selected_text);

  // Update the field with |text| and set the selection.
  void SetTextAndSelectedRange(const std::wstring& text,
                               const CharRange& range);

  // Set the selection to |range|.
  void SetSelectedRange(const CharRange& range);

  // The widget we expose, used for vertically centering the real text edit,
  // since the height will change based on the font / font size, etc.
  OwnedWidgetGtk alignment_;

  // The actual text entry which will be owned by the alignment_.
  GtkWidget* text_view_;

  GtkTextTagTable* tag_table_;
  GtkTextBuffer* text_buffer_;
  GtkTextTag* faded_text_tag_;
  GtkTextTag* secure_scheme_tag_;
  GtkTextTag* insecure_scheme_tag_;
  GtkTextTag* normal_text_tag_;

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

  ToolbarModel::SecurityLevel scheme_security_level_;

  // Selection at the point where the user started using the
  // arrows to move around in the popup.
  CharRange saved_temporary_selection_;

  // Tracking state before and after a possible change.
  std::wstring text_before_change_;
  CharRange sel_before_change_;

  // The most-recently-selected text from the entry.  This is updated on-the-fly
  // as the user selects text.  It is used in cases where we need to make the
  // PRIMARY selection persist even after the user has unhighlighted the text in
  // the view (e.g. when they highlight some text and then click to unhighlight
  // it, we pass this string to SavePrimarySelection()).
  std::string selected_text_;

  // ID of the signal handler for "mark-set" on |text_buffer_|.
  gulong mark_set_handler_id_;

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

  DISALLOW_COPY_AND_ASSIGN(AutocompleteEditViewGtk);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_EDIT_VIEW_GTK_H_
