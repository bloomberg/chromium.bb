// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_WIN_H_
#pragma once

#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlctrls.h>
#include <atlmisc.h>
#include <peninputpanel.h>
#include <tom.h>  // For ITextDocument, a COM interface to CRichEditCtrl.

#include "base/memory/scoped_ptr.h"
#include "base/win/scoped_comptr.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_contents_view.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/font.h"
#include "webkit/glue/window_open_disposition.h"

class AutocompleteEditController;
class AutocompleteEditModel;
class AutocompletePopupView;
class LocationBarView;

namespace views {
class MenuRunner;
class NativeViewHost;
class View;
}

// TODO(abodenha): This should be removed once we have the new windows SDK
// which defines these messages.
#if !defined(WM_POINTERDOWN)
#define WM_POINTERDOWN  0x0246
#endif  // WM_POINTERDOWN

// Provides the implementation of an edit control with a drop-down
// autocomplete box. The box itself is implemented in autocomplete_popup.cc
// This file implements the edit box and management for the popup.
class OmniboxViewWin
    : public CWindowImpl<OmniboxViewWin,
                         CRichEditCtrl,
                         CWinTraits<WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL |
                                    ES_NOHIDESEL> >,
      public CRichEditCommands<OmniboxViewWin>,
      public ui::SimpleMenuModel::Delegate,
      public OmniboxView {
 public:
  struct State {
    State(const CHARRANGE& selection,
          const CHARRANGE& saved_selection_for_focus_change)
        : selection(selection),
          saved_selection_for_focus_change(saved_selection_for_focus_change) {
    }

    const CHARRANGE selection;
    const CHARRANGE saved_selection_for_focus_change;
  };

  DECLARE_WND_CLASS(L"Chrome_OmniboxView");

  OmniboxViewWin(AutocompleteEditController* controller,
                 ToolbarModel* toolbar_model,
                 LocationBarView* parent_view,
                 CommandUpdater* command_updater,
                 bool popup_window_mode,
                 views::View* location_bar);
  ~OmniboxViewWin();

  // Gets the relative window for the specified native view.
  static gfx::NativeView GetRelativeWindowForNativeView(
      gfx::NativeView edit_native_view);

  views::View* parent_view() const;

  // Returns the width in pixels needed to display the text from one character
  // before the caret to the end of the string. See comments in
  // LocationBarView::Layout as to why this uses -1.
  int WidthOfTextAfterCursor();

  // Returns the font.
  gfx::Font GetFont();

  // OmniboxView:
  virtual AutocompleteEditModel* model() OVERRIDE { return model_.get(); }
  virtual const AutocompleteEditModel* model() const OVERRIDE {
    return model_.get();
  }
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
  virtual void OnRevertTemporaryText() OVERRIDE;
  virtual void OnBeforePossibleChange() OVERRIDE;
  virtual bool OnAfterPossibleChange() OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetRelativeWindowForPopup() const OVERRIDE;
  virtual CommandUpdater* GetCommandUpdater() OVERRIDE;
  virtual void SetInstantSuggestion(const string16& suggestion,
                                    bool animate_to_complete) OVERRIDE;
  virtual int TextWidth() const OVERRIDE;
  virtual string16 GetInstantSuggestion() const OVERRIDE;
  virtual bool IsImeComposing() const OVERRIDE;
  virtual int GetMaxEditWidth(int entry_width) const OVERRIDE;
  virtual views::View* AddToView(views::View* parent) OVERRIDE;
  virtual int OnPerformDrop(const views::DropTargetEvent& event) OVERRIDE;

  int GetPopupMaxYCoordinate();

  void SetDropHighlightPosition(int position);
  int drop_highlight_position() const { return drop_highlight_position_; }

  // Returns true if a drag a drop session was initiated by this edit.
  bool in_drag() const { return in_drag_; }

  // Moves the selected text to the specified position.
  void MoveSelectedText(int new_position);

  // Inserts the text at the specified position.
  void InsertText(int position, const string16& text);

  // Invokes CanPasteAndGo with the specified text, and if successful navigates
  // to the appropriate URL. The behavior of this is the same as if the user
  // typed in the specified text and pressed enter.
  void PasteAndGo(const string16& text);

  void set_force_hidden(bool force_hidden) { force_hidden_ = force_hidden; }

  // Called before an accelerator is processed to give us a chance to override
  // it.
  bool SkipDefaultKeyEventProcessing(const views::KeyEvent& event);

  // Handler for external events passed in to us.  The View that owns us may
  // send us events that we should treat as if they were events on us.
  void HandleExternalMsg(UINT msg, UINT flags, const CPoint& screen_point);

  // CWindowImpl
  BEGIN_MSG_MAP(AutocompleteEdit)
    MSG_WM_CHAR(OnChar)
    MSG_WM_CONTEXTMENU(OnContextMenu)
    MSG_WM_COPY(OnCopy)
    MSG_WM_CUT(OnCut)
    MESSAGE_HANDLER_EX(WM_GETOBJECT, OnGetObject)
    MESSAGE_HANDLER_EX(WM_IME_COMPOSITION, OnImeComposition)
    MESSAGE_HANDLER_EX(WM_IME_NOTIFY, OnImeNotify)
    MESSAGE_HANDLER_EX(WM_POINTERDOWN, OnPointerDown)
    MSG_WM_KEYDOWN(OnKeyDown)
    MSG_WM_KEYUP(OnKeyUp)
    MSG_WM_KILLFOCUS(OnKillFocus)
    MSG_WM_LBUTTONDBLCLK(OnLButtonDblClk)
    MSG_WM_LBUTTONDOWN(OnLButtonDown)
    MSG_WM_LBUTTONUP(OnLButtonUp)
    MSG_WM_MBUTTONDBLCLK(OnMButtonDblClk)
    MSG_WM_MBUTTONDOWN(OnMButtonDown)
    MSG_WM_MBUTTONUP(OnMButtonUp)
    MSG_WM_MOUSEACTIVATE(OnMouseActivate)
    MSG_WM_MOUSEMOVE(OnMouseMove)
    MSG_WM_MOUSEWHEEL(OnMouseWheel)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_PASTE(OnPaste)
    MSG_WM_RBUTTONDBLCLK(OnRButtonDblClk)
    MSG_WM_RBUTTONDOWN(OnRButtonDown)
    MSG_WM_RBUTTONUP(OnRButtonUp)
    MSG_WM_SETFOCUS(OnSetFocus)
    MSG_WM_SETTEXT(OnSetText)
    MSG_WM_SYSCHAR(OnSysChar)  // WM_SYSxxx == WM_xxx with ALT down
    MSG_WM_SYSKEYDOWN(OnKeyDown)
    MSG_WM_SYSKEYUP(OnKeyUp)
    MSG_WM_WINDOWPOSCHANGING(OnWindowPosChanging)
    DEFAULT_REFLECTION_HANDLER()  // avoids black margin area
  END_MSG_MAP()

  // ui::SimpleMenuModel::Delegate
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // Returns true if the caret is at the end of the content.
  bool IsCaretAtEnd() const;

 private:
  enum MouseButton {
    kLeft  = 0,
    kRight = 1,
  };

  // This object freezes repainting of the edit until the object is destroyed.
  // Some methods of the CRichEditCtrl draw synchronously to the screen.  If we
  // don't freeze, the user will see a rapid series of calls to these as
  // flickers.
  //
  // Freezing the control while it is already frozen is permitted; the control
  // will unfreeze once both freezes are released (the freezes stack).
  class ScopedFreeze {
   public:
    ScopedFreeze(OmniboxViewWin* edit, ITextDocument* text_object_model);
    ~ScopedFreeze();

   private:
    OmniboxViewWin* const edit_;
    ITextDocument* const text_object_model_;

    DISALLOW_COPY_AND_ASSIGN(ScopedFreeze);
  };

  class EditDropTarget;
  friend class EditDropTarget;

  // This object suspends placing any operations on the edit's undo stack until
  // the object is destroyed.  If we don't do this, some of the operations we
  // perform behind the user's back will be undoable by the user, which feels
  // bizarre and confusing.
  class ScopedSuspendUndo {
   public:
    explicit ScopedSuspendUndo(ITextDocument* text_object_model);
    ~ScopedSuspendUndo();

   private:
    ITextDocument* const text_object_model_;

    DISALLOW_COPY_AND_ASSIGN(ScopedSuspendUndo);
  };

  // Replacement word-breaking proc for the rich edit control.
  static int CALLBACK WordBreakProc(LPTSTR edit_text,
                                    int current_pos,
                                    int num_bytes,
                                    int action);

  // Returns true if |edit_text| starting at |current_pos| is "://".
  static bool SchemeEnd(LPTSTR edit_text, int current_pos, int length);

  // Message handlers
  void OnChar(TCHAR ch, UINT repeat_count, UINT flags);
  void OnContextMenu(HWND window, const CPoint& point);
  void OnCopy();
  void OnCut();
  LRESULT OnGetObject(UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT OnImeComposition(UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT OnImeNotify(UINT message, WPARAM wparam, LPARAM lparam);
  LRESULT OnPointerDown(UINT message, WPARAM wparam, LPARAM lparam);
  void OnKeyDown(TCHAR key, UINT repeat_count, UINT flags);
  void OnKeyUp(TCHAR key, UINT repeat_count, UINT flags);
  void OnKillFocus(HWND focus_wnd);
  void OnLButtonDblClk(UINT keys, const CPoint& point);
  void OnLButtonDown(UINT keys, const CPoint& point);
  void OnLButtonUp(UINT keys, const CPoint& point);
  void OnMButtonDblClk(UINT keys, const CPoint& point);
  void OnMButtonDown(UINT keys, const CPoint& point);
  void OnMButtonUp(UINT keys, const CPoint& point);
  LRESULT OnMouseActivate(HWND window, UINT hit_test, UINT mouse_message);
  void OnMouseMove(UINT keys, const CPoint& point);
  BOOL OnMouseWheel(UINT flags, short delta, CPoint point);
  void OnPaint(HDC bogus_hdc);
  void OnPaste();
  void OnRButtonDblClk(UINT keys, const CPoint& point);
  void OnRButtonDown(UINT keys, const CPoint& point);
  void OnRButtonUp(UINT keys, const CPoint& point);
  void OnSetFocus(HWND focus_wnd);
  LRESULT OnSetText(const wchar_t* text);
  void OnSysChar(TCHAR ch, UINT repeat_count, UINT flags);
  void OnWindowPosChanging(WINDOWPOS* window_pos);

  // Helper function for OnChar() and OnKeyDown() that handles keystrokes that
  // could change the text in the edit.
  void HandleKeystroke(UINT message, TCHAR key, UINT repeat_count, UINT flags);

  // Helper functions for OnKeyDown() that handle accelerators applicable when
  // we're not read-only and all the time, respectively.  These return true if
  // they handled the key.
  bool OnKeyDownOnlyWritable(TCHAR key, UINT repeat_count, UINT flags);
  bool OnKeyDownAllModes(TCHAR key, UINT repeat_count, UINT flags);

  // Like GetSel(), but returns a range where |cpMin| will be larger than
  // |cpMax| if the cursor is at the start rather than the end of the selection
  // (in other words, tracks selection direction as well as offsets).
  // Note the non-Google-style "non-const-ref" argument, which matches GetSel().
  void GetSelection(CHARRANGE& sel) const;

  // Returns the currently selected text of the edit control.
  string16 GetSelectedText() const;

  // Like SetSel(), but respects the selection direction implied by |start| and
  // |end|: if |end| < |start|, the effective cursor will be placed at the
  // beginning of the selection.
  void SetSelection(LONG start, LONG end);

  // Like SetSelection(), but takes a CHARRANGE.
  void SetSelectionRange(const CHARRANGE& sel) {
    SetSelection(sel.cpMin, sel.cpMax);
  }

  // Places the caret at the given position.  This clears any selection.
  void PlaceCaretAt(size_t pos);

  // Returns true if |sel| represents a forward or backward selection of all the
  // text.
  bool IsSelectAllForRange(const CHARRANGE& sel) const;

  // Given an X coordinate in client coordinates, returns that coordinate
  // clipped to be within the horizontal bounds of the visible text.
  //
  // This is used in our mouse handlers to work around quirky behaviors of the
  // underlying CRichEditCtrl like not supporting triple-click when the user
  // doesn't click on the text itself.
  //
  // |is_triple_click| should be true iff this is the third click of a triple
  // click.  Sadly, we need to clip slightly differently in this case.
  LONG ClipXCoordToVisibleText(LONG x, bool is_triple_click) const;

  // Parses the contents of the control for the scheme and the host name.
  // Highlights the scheme in green or red depending on it security level.
  // If a host name is found, it makes it visually stronger.
  void EmphasizeURLComponents();

  // Erases the portion of the selection in the font's y-adjustment area.  For
  // some reason the edit draws the selection rect here even though it's not
  // part of the font.
  void EraseTopOfSelection(CDC* dc,
                           const CRect& client_rect,
                           const CRect& paint_clip_rect);

  // Draws a slash across the scheme if desired.
  void DrawSlashForInsecureScheme(HDC hdc,
                                  const CRect& client_rect,
                                  const CRect& paint_clip_rect);

  // Renders the drop highlight.
  void DrawDropHighlight(HDC hdc,
                         const CRect& client_rect,
                         const CRect& paint_clip_rect);

  // Internally invoked whenever the text changes in some way.
  void TextChanged();

  // Determines whether the user can "paste and go", given the specified text.
  bool CanPasteAndGo(const string16& text) const;

  // Getter for the text_object_model_.  Note that the pointer returned here is
  // only valid as long as the AutocompleteEdit is still alive.  Also, if the
  // underlying call fails, this may return NULL.
  ITextDocument* GetTextObjectModel() const;

  // Invoked during a mouse move. As necessary starts a drag and drop session.
  void StartDragIfNecessary(const CPoint& point);

  // Invoked during a mouse down. If the mouse location is over the selection
  // this sets possible_drag_ to true to indicate a drag should start if the
  // user moves the mouse far enough to start a drag.
  void OnPossibleDrag(const CPoint& point);

  // Redraws the necessary region for a drop highlight at the specified
  // position. This does nothing if position is beyond the bounds of the
  // text.
  void RepaintDropHighlight(int position);

  // Generates the context menu for the edit field.
  void BuildContextMenu();

  void SelectAllIfNecessary(MouseButton button, const CPoint& point);
  void TrackMousePosition(MouseButton button, const CPoint& point);

  // Returns the sum of the left and right margins.
  int GetHorizontalMargin() const;

  // Returns the width in pixels needed to display |text|.
  int WidthNeededToDisplay(const string16& text) const;

  // Real implementation of OnAfterPossibleChange() method.
  // If |force_text_changed| is true, then the text_changed code will always be
  // triggerred no matter if the text is actually changed or not.
  bool OnAfterPossibleChangeInternal(bool force_text_changed);

  // Common implementation for performing a drop on the edit view.
  int OnPerformDropImpl(const views::DropTargetEvent& event, bool in_drag);

  scoped_ptr<AutocompleteEditModel> model_;

  scoped_ptr<AutocompletePopupView> popup_view_;

  AutocompleteEditController* controller_;

  // The parent view for the edit, used to align the popup and for
  // accessibility.
  LocationBarView* parent_view_;

  ToolbarModel* toolbar_model_;

  // The object that handles additional command functionality exposed on the
  // edit, such as invoking the keyword editor.
  CommandUpdater* command_updater_;

  // When true, the location bar view is read only and also is has a slightly
  // different presentation (font size / color). This is used for popups.
  bool popup_window_mode_;

  // True if we should prevent attempts to make the window visible when we
  // handle WM_WINDOWPOSCHANGING.  While toggling fullscreen mode, the main
  // window is hidden, and if the edit is shown it will draw over the main
  // window when that window reappears.
  bool force_hidden_;

  // Non-null when the edit is gaining focus from a left click.  This is only
  // needed between when WM_MOUSEACTIVATE and WM_LBUTTONDOWN get processed.  It
  // serves two purposes: first, by communicating to OnLButtonDown() that we're
  // gaining focus from a left click, it allows us to work even with the
  // inconsistent order in which various Windows messages get sent (see comments
  // in OnMouseActivate()).  Second, by holding the edit frozen, it ensures that
  // when we process WM_SETFOCUS the edit won't first redraw itself with the
  // caret at the beginning, and then have it blink to where the mouse cursor
  // really is shortly afterward.
  scoped_ptr<ScopedFreeze> gaining_focus_;

  // When the user clicks to give us focus, we watch to see if they're clicking
  // or dragging.  When they're clicking, we select nothing until mouseup, then
  // select all the text in the edit.  During this process, tracking_click_[X]
  // is true and click_point_[X] holds the original click location.
  // At other times, tracking_click_[X] is false, and the contents of
  // click_point_[X] should be ignored. The arrays hold the state for the
  // left and right mouse buttons, and are indexed using the MouseButton enum.
  bool tracking_click_[2];
  CPoint click_point_[2];

  // We need to know if the user triple-clicks, so track double click points
  // and times so we can see if subsequent clicks are actually triple clicks.
  bool tracking_double_click_;
  CPoint double_click_point_;
  DWORD double_click_time_;

  // Used to discard unnecessary WM_MOUSEMOVE events after the first such
  // unnecessary event.  See detailed comments in OnMouseMove().
  bool can_discard_mousemove_;

  // Used to prevent IME message handling in the midst of updating the edit
  // text.  See comments where this is used.
  bool ignore_ime_messages_;

  // Variables for tracking state before and after a possible change.
  string16 text_before_change_;
  CHARRANGE sel_before_change_;

  // Set at the same time the model's original_* members are set, and valid in
  // the same cases.
  CHARRANGE original_selection_;

  // Holds the user's selection across focus changes.  cpMin holds -1 when
  // there is no saved selection.
  CHARRANGE saved_selection_for_focus_change_;

  // Was the delete key pressed with an empty selection at the end of the edit?
  bool delete_at_end_pressed_;

  // The context menu for the edit.
  scoped_ptr<ui::SimpleMenuModel> context_menu_contents_;
  scoped_ptr<views::MenuRunner> context_menu_runner_;

  // Font we're using.  We keep a reference to make sure the font supplied to
  // the constructor doesn't go away before we do.
  gfx::Font font_;

  // Metrics about the font, which we keep so we don't need to recalculate them
  // every time we paint.  |font_y_adjustment_| is the number of pixels we need
  // to shift the font vertically in order to make its baseline be at our
  // desired baseline in the edit.
  int font_x_height_;
  int font_y_adjustment_;

  // If true, indicates the mouse is down and if the mouse is moved enough we
  // should start a drag.
  bool possible_drag_;

  // If true, we're in a call to DoDragDrop.
  bool in_drag_;

  // If true indicates we've run a drag and drop session. This is used to
  // avoid starting two drag and drop sessions if the drag is canceled while
  // the mouse is still down.
  bool initiated_drag_;

  // Position of the drop highlight.  If this is -1, there is no drop highlight.
  int drop_highlight_position_;

  // True if the IME candidate window is open.  When this is true, we want to
  // avoid showing the popup.
  bool ime_candidate_window_open_;

  // Security UI-related data.
  COLORREF background_color_;
  ToolbarModel::SecurityLevel security_level_;

  // This interface is useful for accessing the CRichEditCtrl at a low level.
  mutable ITextDocument* text_object_model_;

  // This contains the scheme char start and stop indexes that should be
  // stricken-out when displaying an insecure scheme.
  url_parse::Component insecure_scheme_component_;

  // Instance of accessibility information and handling.
  mutable base::win::ScopedComPtr<IAccessible> autocomplete_accessibility_;

  // The native view host.
  views::NativeViewHost* native_view_host_;

  // ITextInputPanel to allow us to show the Windows virtual keyboard when a
  // user touches the Omnibox.
  base::win::ScopedComPtr<ITextInputPanel> keyboard_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_VIEW_WIN_H_
