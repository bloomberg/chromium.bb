// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_win.h"

#include <algorithm>
#include <locale>
#include <string>

#include <richedit.h>
#include <textserv.h>

#include "base/auto_reset.h"
#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/property_bag.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/iat_patch_function.h"
#include "base/win/windows_version.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/url_util.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_source.h"
#include "ui/base/dragdrop/drop_target.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "ui/base/events.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/win/mouse_wheel_util.h"
#include "ui/gfx/canvas.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/menu/menu_2.h"
#include "ui/views/controls/textfield/native_textfield_win.h"
#include "ui/views/widget/widget.h"

#pragma comment(lib, "oleacc.lib")  // Needed for accessibility support.
#pragma comment(lib, "riched20.lib")  // Needed for the richedit control.

using content::UserMetricsAction;
using content::WebContents;

///////////////////////////////////////////////////////////////////////////////
// AutocompleteEditModel

namespace {

// A helper method for determining a valid DROPEFFECT given the allowed
// DROPEFFECTS.  We prefer copy over link.
DWORD CopyOrLinkDropEffect(DWORD effect) {
  if (effect & DROPEFFECT_COPY)
    return DROPEFFECT_COPY;
  if (effect & DROPEFFECT_LINK)
    return DROPEFFECT_LINK;
  return DROPEFFECT_NONE;
}

// A helper method for determining a valid drag operation given the allowed
// operation.  We prefer copy over link.
int CopyOrLinkDragOperation(int drag_operation) {
  if (drag_operation & ui::DragDropTypes::DRAG_COPY)
    return ui::DragDropTypes::DRAG_COPY;
  if (drag_operation & ui::DragDropTypes::DRAG_LINK)
    return ui::DragDropTypes::DRAG_LINK;
  return ui::DragDropTypes::DRAG_NONE;
}

// The AutocompleteEditState struct contains enough information about the
// AutocompleteEditModel and OmniboxViewWin to save/restore a user's
// typing, caret position, etc. across tab changes.  We explicitly don't
// preserve things like whether the popup was open as this might be weird.
struct AutocompleteEditState {
  AutocompleteEditState(const AutocompleteEditModel::State& model_state,
                        const OmniboxViewWin::State& view_state)
      : model_state(model_state),
        view_state(view_state) {
  }

  const AutocompleteEditModel::State model_state;
  const OmniboxViewWin::State view_state;
};

// Returns true if the current point is far enough from the origin that it
// would be considered a drag.
bool IsDrag(const POINT& origin, const POINT& current) {
  return views::View::ExceededDragThreshold(current.x - origin.x,
                                            current.y - origin.y);
}

}  // namespace

// EditDropTarget is the IDropTarget implementation installed on
// OmniboxViewWin. EditDropTarget prefers URL over plain text. A drop
// of a URL replaces all the text of the edit and navigates immediately to the
// URL. A drop of plain text from the same edit either copies or moves the
// selected text, and a drop of plain text from a source other than the edit
// does a paste and go.
class OmniboxViewWin::EditDropTarget : public ui::DropTarget {
 public:
  explicit EditDropTarget(OmniboxViewWin* edit);

 protected:
  virtual DWORD OnDragEnter(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect);
  virtual DWORD OnDragOver(IDataObject* data_object,
                           DWORD key_state,
                           POINT cursor_position,
                           DWORD effect);
  virtual void OnDragLeave(IDataObject* data_object);
  virtual DWORD OnDrop(IDataObject* data_object,
                       DWORD key_state,
                       POINT cursor_position,
                       DWORD effect);

 private:
  // If dragging a string, the drop highlight position of the edit is reset
  // based on the mouse position.
  void UpdateDropHighlightPosition(const POINT& cursor_screen_position);

  // Resets the visual drop indicates we install on the edit.
  void ResetDropHighlights();

  // The edit we're the drop target for.
  OmniboxViewWin* edit_;

  // If true, the drag session contains a URL.
  bool drag_has_url_;

  // If true, the drag session contains a string. If drag_has_url_ is true,
  // this is false regardless of whether the clipboard has a string.
  bool drag_has_string_;

  DISALLOW_COPY_AND_ASSIGN(EditDropTarget);
};

OmniboxViewWin::EditDropTarget::EditDropTarget(OmniboxViewWin* edit)
    : ui::DropTarget(edit->m_hWnd),
      edit_(edit),
      drag_has_url_(false),
      drag_has_string_(false) {
}

DWORD OmniboxViewWin::EditDropTarget::OnDragEnter(IDataObject* data_object,
                                                  DWORD key_state,
                                                  POINT cursor_position,
                                                  DWORD effect) {
  ui::OSExchangeData os_data(new ui::OSExchangeDataProviderWin(data_object));
  drag_has_url_ = os_data.HasURL();
  drag_has_string_ = !drag_has_url_ && os_data.HasString();
  if (drag_has_url_) {
    if (edit_->in_drag()) {
      // The edit we're associated with originated the drag. No point in
      // allowing the user to drop back on us.
      drag_has_url_ = false;
    }
    // NOTE: it would be nice to visually show all the text is going to
    // be replaced by selecting all, but this caused painting problems. In
    // particular the flashing caret would appear outside the edit! For now
    // we stick with no visual indicator other than that shown own the mouse
    // cursor.
  }
  return OnDragOver(data_object, key_state, cursor_position, effect);
}

DWORD OmniboxViewWin::EditDropTarget::OnDragOver(IDataObject* data_object,
                                                 DWORD key_state,
                                                 POINT cursor_position,
                                                 DWORD effect) {
  if (drag_has_url_)
    return CopyOrLinkDropEffect(effect);

  if (drag_has_string_) {
    UpdateDropHighlightPosition(cursor_position);
    if (edit_->drop_highlight_position() == -1 && edit_->in_drag())
      return DROPEFFECT_NONE;
    if (edit_->in_drag()) {
      // The edit we're associated with originated the drag.  Do the normal drag
      // behavior.
      DCHECK((effect & DROPEFFECT_COPY) && (effect & DROPEFFECT_MOVE));
      return (key_state & MK_CONTROL) ? DROPEFFECT_COPY : DROPEFFECT_MOVE;
    }
    // Our edit didn't originate the drag, only allow link or copy.
    return CopyOrLinkDropEffect(effect);
  }

  return DROPEFFECT_NONE;
}

void OmniboxViewWin::EditDropTarget::OnDragLeave(IDataObject* data_object) {
  ResetDropHighlights();
}

DWORD OmniboxViewWin::EditDropTarget::OnDrop(IDataObject* data_object,
                                             DWORD key_state,
                                             POINT cursor_position,
                                             DWORD effect) {
  effect = OnDragOver(data_object, key_state, cursor_position, effect);

  ui::OSExchangeData os_data(new ui::OSExchangeDataProviderWin(data_object));
  views::DropTargetEvent event(os_data, cursor_position.x, cursor_position.y,
      ui::DragDropTypes::DropEffectToDragOperation(effect));

  int drag_operation = edit_->OnPerformDropImpl(event, edit_->in_drag());

  if (!drag_has_url_)
    ResetDropHighlights();

  return ui::DragDropTypes::DragOperationToDropEffect(drag_operation);
}

void OmniboxViewWin::EditDropTarget::UpdateDropHighlightPosition(
    const POINT& cursor_screen_position) {
  if (drag_has_string_) {
    POINT client_position = cursor_screen_position;
    ::ScreenToClient(edit_->m_hWnd, &client_position);
    int drop_position = edit_->CharFromPos(client_position);
    if (edit_->in_drag()) {
      // Our edit originated the drag, don't allow a drop if over the selected
      // region.
      LONG sel_start, sel_end;
      edit_->GetSel(sel_start, sel_end);
      if ((sel_start != sel_end) && (drop_position >= sel_start) &&
          (drop_position <= sel_end))
        drop_position = -1;
    } else {
      // A drop from a source other than the edit replaces all the text, so
      // we don't show the drop location. See comment in OnDragEnter as to why
      // we don't try and select all here.
      drop_position = -1;
    }
    edit_->SetDropHighlightPosition(drop_position);
  }
}

void OmniboxViewWin::EditDropTarget::ResetDropHighlights() {
  if (drag_has_string_)
    edit_->SetDropHighlightPosition(-1);
}


///////////////////////////////////////////////////////////////////////////////
// Helper classes

OmniboxViewWin::ScopedFreeze::ScopedFreeze(OmniboxViewWin* edit,
                                           ITextDocument* text_object_model)
    : edit_(edit),
      text_object_model_(text_object_model) {
  // Freeze the screen.
  if (text_object_model_) {
    long count;
    text_object_model_->Freeze(&count);
  }
}

OmniboxViewWin::ScopedFreeze::~ScopedFreeze() {
  // Unfreeze the screen.
  // NOTE: If this destructor is reached while the edit is being destroyed (for
  // example, because we double-clicked the edit of a popup and caused it to
  // transform to an unconstrained window), it will no longer have an HWND, and
  // text_object_model_ may point to a destroyed object, so do nothing here.
  if (edit_->IsWindow() && text_object_model_) {
    long count;
    text_object_model_->Unfreeze(&count);
    if (count == 0) {
      // We need to UpdateWindow() here in addition to InvalidateRect() because,
      // as far as I can tell, the edit likes to synchronously erase its
      // background when unfreezing, thus requiring us to synchronously redraw
      // if we don't want flicker.
      edit_->InvalidateRect(NULL, false);
      edit_->UpdateWindow();
    }
  }
}

OmniboxViewWin::ScopedSuspendUndo::ScopedSuspendUndo(
    ITextDocument* text_object_model)
    : text_object_model_(text_object_model) {
  // Suspend Undo processing.
  if (text_object_model_)
    text_object_model_->Undo(tomSuspend, NULL);
}

OmniboxViewWin::ScopedSuspendUndo::~ScopedSuspendUndo() {
  // Resume Undo processing.
  if (text_object_model_)
    text_object_model_->Undo(tomResume, NULL);
}

// A subclass of NativeViewHost that provides accessibility info for the
// underlying Omnibox view.
class OmniboxViewWrapper : public views::NativeViewHost {
 public:
  explicit OmniboxViewWrapper(OmniboxViewWin* omnibox_view_win)
      : omnibox_view_win_(omnibox_view_win) {}

  gfx::NativeViewAccessible GetNativeViewAccessible() {
    // This forces it to use NativeViewAccessibilityWin rather than
    // any accessibility provided natively by the HWND.
    return View::GetNativeViewAccessible();
  }

  // views::View
  virtual void GetAccessibleState(ui::AccessibleViewState* state) {
    views::NativeViewHost::GetAccessibleState(state);
    state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_LOCATION);
    state->role = ui::AccessibilityTypes::ROLE_TEXT;
    state->value = omnibox_view_win_->GetText();
    state->state = ui::AccessibilityTypes::STATE_EDITABLE;
    size_t sel_start;
    size_t sel_end;
    omnibox_view_win_->GetSelectionBounds(&sel_start, &sel_end);
    state->selection_start = sel_start;
    state->selection_end = sel_end;
  }

 private:
  OmniboxViewWin* omnibox_view_win_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxViewWrapper);
};

///////////////////////////////////////////////////////////////////////////////
// OmniboxViewWin

namespace {

// These are used to hook the CRichEditCtrl's calls to BeginPaint() and
// EndPaint() and provide a memory DC instead.  See OnPaint().
HWND edit_hwnd = NULL;
PAINTSTRUCT paint_struct;

// Intercepted method for BeginPaint(). Must use __stdcall convention.
HDC WINAPI BeginPaintIntercept(HWND hWnd, LPPAINTSTRUCT lpPaint) {
  if (!edit_hwnd || (hWnd != edit_hwnd))
    return ::BeginPaint(hWnd, lpPaint);

  *lpPaint = paint_struct;
  return paint_struct.hdc;
}

// Intercepted method for EndPaint(). Must use __stdcall convention.
BOOL WINAPI EndPaintIntercept(HWND hWnd, const PAINTSTRUCT* lpPaint) {
  return (edit_hwnd && (hWnd == edit_hwnd)) || ::EndPaint(hWnd, lpPaint);
}

// Returns a lazily initialized property bag accessor for saving our state in a
// WebContents.
base::PropertyAccessor<AutocompleteEditState>* GetStateAccessor() {
  static base::PropertyAccessor<AutocompleteEditState> state;
  return &state;
}

class PaintPatcher {
 public:
  PaintPatcher();
  ~PaintPatcher();

  void RefPatch();
  void DerefPatch();

 private:
  size_t refcount_;
  base::win::IATPatchFunction begin_paint_;
  base::win::IATPatchFunction end_paint_;

  DISALLOW_COPY_AND_ASSIGN(PaintPatcher);
};

PaintPatcher::PaintPatcher() : refcount_(0) {
}

PaintPatcher::~PaintPatcher() {
  DCHECK_EQ(0U, refcount_);
}

void PaintPatcher::RefPatch() {
  if (refcount_ == 0) {
    DCHECK(!begin_paint_.is_patched());
    DCHECK(!end_paint_.is_patched());
    begin_paint_.Patch(L"riched20.dll", "user32.dll", "BeginPaint",
                       &BeginPaintIntercept);
    end_paint_.Patch(L"riched20.dll", "user32.dll", "EndPaint",
                     &EndPaintIntercept);
  }
  ++refcount_;
}

void PaintPatcher::DerefPatch() {
  DCHECK(begin_paint_.is_patched());
  DCHECK(end_paint_.is_patched());
  --refcount_;
  if (refcount_ == 0) {
    begin_paint_.Unpatch();
    end_paint_.Unpatch();
  }
}

base::LazyInstance<PaintPatcher> g_paint_patcher = LAZY_INSTANCE_INITIALIZER;

// twips are a unit of type measurement, and RichEdit controls use them
// to set offsets.
const int kTwipsPerInch = 1440;

}  // namespace

OmniboxViewWin::OmniboxViewWin(AutocompleteEditController* controller,
                               ToolbarModel* toolbar_model,
                               LocationBarView* parent_view,
                               CommandUpdater* command_updater,
                               bool popup_window_mode,
                               views::View* location_bar)
    : model_(new AutocompleteEditModel(this, controller,
                                       parent_view->profile())),
      popup_view_(AutocompletePopupContentsView::CreateForEnvironment(
          parent_view->font(), this, model_.get(), location_bar)),
      controller_(controller),
      parent_view_(parent_view),
      toolbar_model_(toolbar_model),
      command_updater_(command_updater),
      popup_window_mode_(popup_window_mode),
      force_hidden_(false),
      tracking_click_(),
      tracking_double_click_(false),
      double_click_time_(0),
      can_discard_mousemove_(false),
      ignore_ime_messages_(false),
      delete_at_end_pressed_(false),
      font_(parent_view->font()),
      possible_drag_(false),
      in_drag_(false),
      initiated_drag_(false),
      drop_highlight_position_(-1),
      ime_candidate_window_open_(false),
      background_color_(skia::SkColorToCOLORREF(LocationBarView::GetColor(
          ToolbarModel::NONE, LocationBarView::BACKGROUND))),
      security_level_(ToolbarModel::NONE),
      text_object_model_(NULL) {
  // Dummy call to a function exported by riched20.dll to ensure it sets up an
  // import dependency on the dll.
  CreateTextServices(NULL, NULL, NULL);

  saved_selection_for_focus_change_.cpMin = -1;

  g_paint_patcher.Pointer()->RefPatch();

  Create(location_bar->GetWidget()->GetNativeView(), 0, 0, 0,
         l10n_util::GetExtendedStyles());
  SetReadOnly(popup_window_mode_);
  SetFont(font_.GetNativeFont());

  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    // Locally define CLSID_TextInputPanel to avoid issues with multiply defined
    // or undefined symbols if we include peninputpanel_i.c.
    const GUID CLSID_TextInputPanel = {0xf9b189d7, 0x228b, 0x4f2b, 0x86, 0x50,\
                0xb9, 0x7f, 0x59, 0xe0, 0x2c, 0x8c};
    keyboard_.CreateInstance(CLSID_TextInputPanel, NULL, CLSCTX_INPROC);
    if (keyboard_ != NULL)
      keyboard_->put_AttachedEditWindow(m_hWnd);
  }

  // NOTE: Do not use SetWordBreakProcEx() here, that is no longer supported as
  // of Rich Edit 2.0 onward.
  SendMessage(m_hWnd, EM_SETWORDBREAKPROC, 0,
              reinterpret_cast<LPARAM>(&WordBreakProc));

  // Get the metrics for the font.
  HDC hdc = ::GetDC(NULL);
  HGDIOBJ old_font = SelectObject(hdc, font_.GetNativeFont());
  TEXTMETRIC tm = {0};
  GetTextMetrics(hdc, &tm);
  const float kXHeightRatio = 0.7f;  // The ratio of a font's x-height to its
                                     // cap height.  Sadly, Windows doesn't
                                     // provide a true value for a font's
                                     // x-height in its text metrics, so we
                                     // approximate.
  font_x_height_ = static_cast<int>((static_cast<float>(font_.GetBaseline() -
      tm.tmInternalLeading) * kXHeightRatio) + 0.5);
  // The distance from the top of the field to the desired baseline of the
  // rendered text.
  const int kTextBaseline = popup_window_mode_ ? 15 : 18;
  font_y_adjustment_ = kTextBaseline - font_.GetBaseline();

  // Get the number of twips per pixel, which we need below to offset our text
  // by the desired number of pixels.
  const long kTwipsPerPixel = kTwipsPerInch / GetDeviceCaps(hdc, LOGPIXELSY);
  // It's unsafe to delete a DC with a non-stock object selected, so restore the
  // original font.
  SelectObject(hdc, old_font);
  ::ReleaseDC(NULL, hdc);

  // Set the default character style -- adjust to our desired baseline.
  CHARFORMAT cf = {0};
  cf.dwMask = CFM_OFFSET;
  cf.yOffset = -font_y_adjustment_ * kTwipsPerPixel;
  SetDefaultCharFormat(cf);

  SetBackgroundColor(background_color_);

  // By default RichEdit has a drop target. Revoke it so that we can install our
  // own. Revoke takes care of deleting the existing one.
  RevokeDragDrop(m_hWnd);

  // Register our drop target. RichEdit appears to invoke RevokeDropTarget when
  // done so that we don't have to explicitly.
  if (!popup_window_mode_) {
    scoped_refptr<EditDropTarget> drop_target = new EditDropTarget(this);
    RegisterDragDrop(m_hWnd, drop_target.get());
  }
}

OmniboxViewWin::~OmniboxViewWin() {
  // Explicitly release the text object model now that we're done with it, and
  // before we free the library. If the library gets unloaded before this
  // released, it becomes garbage.
  text_object_model_->Release();

  // We balance our reference count and unpatch when the last instance has
  // been destroyed.  This prevents us from relying on the AtExit or static
  // destructor sequence to do our unpatching, which is generally fragile.
  g_paint_patcher.Pointer()->DerefPatch();
}

views::View* OmniboxViewWin::parent_view() const {
  return parent_view_;
}

int OmniboxViewWin::WidthOfTextAfterCursor() {
  CHARRANGE selection;
  GetSelection(selection);
  const int start = std::max(0, static_cast<int>(selection.cpMax - 1));
  return WidthNeededToDisplay(GetText().substr(start));
}

gfx::Font OmniboxViewWin::GetFont() {
  return font_;
}

void OmniboxViewWin::SaveStateToTab(WebContents* tab) {
  DCHECK(tab);

  const AutocompleteEditModel::State model_state(
      model_->GetStateForTabSwitch());

  CHARRANGE selection;
  GetSelection(selection);
  GetStateAccessor()->SetProperty(tab->GetPropertyBag(),
      AutocompleteEditState(
          model_state,
          State(selection, saved_selection_for_focus_change_)));
}

void OmniboxViewWin::Update(const WebContents* tab_for_state_restoring) {
  const bool visibly_changed_permanent_text =
      model_->UpdatePermanentText(toolbar_model_->GetText());

  const ToolbarModel::SecurityLevel security_level =
      toolbar_model_->GetSecurityLevel();
  const bool changed_security_level = (security_level != security_level_);

  // Bail early when no visible state will actually change (prevents an
  // unnecessary ScopedFreeze, and thus UpdateWindow()).
  if (!changed_security_level && !visibly_changed_permanent_text &&
      !tab_for_state_restoring)
    return;

  // Update our local state as desired.  We set security_level_ here so it will
  // already be correct before we get to any RevertAll()s below and use it.
  security_level_ = security_level;

  // When we're switching to a new tab, restore its state, if any.
  ScopedFreeze freeze(this, GetTextObjectModel());
  if (tab_for_state_restoring) {
    // Make sure we reset our own state first.  The new tab may not have any
    // saved state, or it may not have had input in progress, in which case we
    // won't overwrite all our local state.
    RevertAll();

    const AutocompleteEditState* state = GetStateAccessor()->GetProperty(
        tab_for_state_restoring->GetPropertyBag());
    if (state) {
      model_->RestoreState(state->model_state);

      // Restore user's selection.  We do this after restoring the user_text
      // above so we're selecting in the correct string.
      SetSelectionRange(state->view_state.selection);
      saved_selection_for_focus_change_ =
          state->view_state.saved_selection_for_focus_change;
    }
  } else if (visibly_changed_permanent_text) {
    // Not switching tabs, just updating the permanent text.  (In the case where
    // we _were_ switching tabs, the RevertAll() above already drew the new
    // permanent text.)

    // Tweak: if the edit was previously nonempty and had all the text selected,
    // select all the new text.  This makes one particular case better: the
    // user clicks in the box to change it right before the permanent URL is
    // changed.  Since the new URL is still fully selected, the user's typing
    // will replace the edit contents as they'd intended.
    //
    // NOTE: The selection can be longer than the text length if the edit is in
    // in rich text mode and the user has selected the "phantom newline" at the
    // end, so use ">=" instead of "==" to see if all the text is selected.  In
    // theory we prevent this case from ever occurring, but this is still safe.
    CHARRANGE sel;
    GetSelection(sel);
    const bool was_reversed = (sel.cpMin > sel.cpMax);
    const bool was_sel_all = (sel.cpMin != sel.cpMax) &&
      IsSelectAllForRange(sel);

    RevertAll();

    if (was_sel_all)
      SelectAll(was_reversed);
  } else if (changed_security_level) {
    // Only the security style changed, nothing else.  Redraw our text using it.
    EmphasizeURLComponents();
  }
}

void OmniboxViewWin::OpenMatch(const AutocompleteMatch& match,
                               WindowOpenDisposition disposition,
                               const GURL& alternate_nav_url,
                               size_t selected_line) {
  if (!match.destination_url.is_valid())
    return;

  // When we navigate, we first revert to the unedited state, then if necessary
  // synchronously change the permanent text to the new URL.  If we don't freeze
  // here, the user could potentially see a flicker of the current URL before
  // the new one reappears, which would look glitchy.
  ScopedFreeze freeze(this, GetTextObjectModel());
  model_->OpenMatch(match, disposition, alternate_nav_url, selected_line);
}

string16 OmniboxViewWin::GetText() const {
  const int len = GetTextLength() + 1;
  string16 str;
  if (len > 1)
    GetWindowText(WriteInto(&str, len), len);
  return str;
}

bool OmniboxViewWin::IsEditingOrEmpty() const {
  return model_->user_input_in_progress() || (GetTextLength() == 0);
}

int OmniboxViewWin::GetIcon() const {
  return IsEditingOrEmpty() ?
      AutocompleteMatch::TypeToIcon(model_->CurrentTextType()) :
      toolbar_model_->GetIcon();
}

void OmniboxViewWin::SetUserText(const string16& text) {
  SetUserText(text, text, true);
}

void OmniboxViewWin::SetUserText(const string16& text,
                                 const string16& display_text,
                                 bool update_popup) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  model_->SetUserText(text);
  saved_selection_for_focus_change_.cpMin = -1;
  SetWindowTextAndCaretPos(display_text, display_text.length(), update_popup,
      true);
}

void OmniboxViewWin::SetWindowTextAndCaretPos(const string16& text,
                                              size_t caret_pos,
                                              bool update_popup,
                                              bool notify_text_changed) {
  SetWindowText(text.c_str());
  PlaceCaretAt(caret_pos);

  if (update_popup)
    UpdatePopup();

  if (notify_text_changed)
    TextChanged();
}

void OmniboxViewWin::SetForcedQuery() {
  const string16 current_text(GetText());
  const size_t start = current_text.find_first_not_of(kWhitespaceWide);
  if (start == string16::npos || (current_text[start] != '?'))
    SetUserText(L"?");
  else
    SetSelection(current_text.length(), start + 1);
}

bool OmniboxViewWin::IsSelectAll() const {
  CHARRANGE selection;
  GetSel(selection);
  return IsSelectAllForRange(selection);
}

bool OmniboxViewWin::DeleteAtEndPressed() {
  return delete_at_end_pressed_;
}

void OmniboxViewWin::GetSelectionBounds(string16::size_type* start,
                                        string16::size_type* end) const {
  CHARRANGE selection;
  GetSel(selection);
  *start = static_cast<size_t>(selection.cpMin);
  *end = static_cast<size_t>(selection.cpMax);
}

void OmniboxViewWin::SelectAll(bool reversed) {
  if (reversed)
    SetSelection(GetTextLength(), 0);
  else
    SetSelection(0, GetTextLength());
}

void OmniboxViewWin::RevertAll() {
  ScopedFreeze freeze(this, GetTextObjectModel());
  ClosePopup();
  saved_selection_for_focus_change_.cpMin = -1;
  model_->Revert();
}

void OmniboxViewWin::UpdatePopup() {
  ScopedFreeze freeze(this, GetTextObjectModel());
  model_->SetInputInProgress(true);

  // Don't allow the popup to open while the candidate window is open, so
  // they don't overlap.
  if (ime_candidate_window_open_)
    return;

  if (!model_->has_focus()) {
    // When we're in the midst of losing focus, don't rerun autocomplete.  This
    // can happen when losing focus causes the IME to cancel/finalize a
    // composition.  We still want to note that user input is in progress, we
    // just don't want to do anything else.
    //
    // Note that in this case the ScopedFreeze above was unnecessary; however,
    // we're inside the callstack of OnKillFocus(), which has already frozen the
    // edit, so this will never result in an unnecessary UpdateWindow() call.
    return;
  }

  // Don't inline autocomplete when:
  //   * The user is deleting text
  //   * The caret/selection isn't at the end of the text
  //   * The user has just pasted in something that replaced all the text
  //   * The user is trying to compose something in an IME
  CHARRANGE sel;
  GetSel(sel);
  model_->StartAutocomplete(sel.cpMax != sel.cpMin,
                            (sel.cpMax < GetTextLength()) || IsImeComposing());
}

void OmniboxViewWin::ClosePopup() {
  model_->StopAutocomplete();
}

void OmniboxViewWin::SetFocus() {
  ::SetFocus(m_hWnd);
}

void OmniboxViewWin::SetDropHighlightPosition(int position) {
  if (drop_highlight_position_ != position) {
    RepaintDropHighlight(drop_highlight_position_);
    drop_highlight_position_ = position;
    RepaintDropHighlight(drop_highlight_position_);
  }
}

void OmniboxViewWin::MoveSelectedText(int new_position) {
  const string16 selected_text(GetSelectedText());
  CHARRANGE sel;
  GetSel(sel);
  DCHECK((sel.cpMax != sel.cpMin) && (new_position >= 0) &&
         (new_position <= GetTextLength()));

  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();

  // Nuke the selected text.
  ReplaceSel(L"", TRUE);

  // And insert it into the new location.
  if (new_position >= sel.cpMin)
    new_position -= (sel.cpMax - sel.cpMin);
  PlaceCaretAt(new_position);
  ReplaceSel(selected_text.c_str(), TRUE);

  OnAfterPossibleChange();
}

void OmniboxViewWin::InsertText(int position, const string16& text) {
  DCHECK((position >= 0) && (position <= GetTextLength()));
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  SetSelection(position, position);
  ReplaceSel(text.c_str());
  OnAfterPossibleChange();
}

void OmniboxViewWin::OnTemporaryTextMaybeChanged(const string16& display_text,
                                                 bool save_original_selection) {
  if (save_original_selection)
    GetSelection(original_selection_);

  // Set new text and cursor position.  Sometimes this does extra work (e.g.
  // when the new text and the old text are identical), but it's only called
  // when the user manually changes the selected line in the popup, so that's
  // not really a problem.  Also, even when the text hasn't changed we'd want to
  // update the caret, because if the user had the cursor in the middle of the
  // text and then arrowed to another entry with the same text, we'd still want
  // to move the caret.
  ScopedFreeze freeze(this, GetTextObjectModel());
  SetWindowTextAndCaretPos(display_text, display_text.length(), false, true);
}

bool OmniboxViewWin::OnInlineAutocompleteTextMaybeChanged(
    const string16& display_text,
    size_t user_text_length) {
  // Update the text and selection.  Because this can be called repeatedly while
  // typing, we've careful not to freeze the edit unless we really need to.
  // Also, unlike in the temporary text case above, here we don't want to update
  // the caret/selection unless we have to, since this might make the user's
  // caret position change without warning during typing.
  if (display_text == GetText())
    return false;

  ScopedFreeze freeze(this, GetTextObjectModel());
  SetWindowText(display_text.c_str());
  // Set a reversed selection to keep the caret in the same position, which
  // avoids scrolling the user's text.
  SetSelection(static_cast<LONG>(display_text.length()),
               static_cast<LONG>(user_text_length));
  TextChanged();
  return true;
}

void OmniboxViewWin::OnRevertTemporaryText() {
  SetSelectionRange(original_selection_);
  TextChanged();
}

void OmniboxViewWin::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = GetText();
  GetSelection(sel_before_change_);
}

bool OmniboxViewWin::OnAfterPossibleChange() {
  return OnAfterPossibleChangeInternal(false);
}

bool OmniboxViewWin::OnAfterPossibleChangeInternal(bool force_text_changed) {
  // Prevent the user from selecting the "phantom newline" at the end of the
  // edit.  If they try, we just silently move the end of the selection back to
  // the end of the real text.
  CHARRANGE new_sel;
  GetSelection(new_sel);
  const int length = GetTextLength();
  if ((new_sel.cpMin > length) || (new_sel.cpMax > length)) {
    if (new_sel.cpMin > length)
      new_sel.cpMin = length;
    if (new_sel.cpMax > length)
      new_sel.cpMax = length;
    SetSelectionRange(new_sel);
  }
  const bool selection_differs =
      ((new_sel.cpMin != new_sel.cpMax) ||
       (sel_before_change_.cpMin != sel_before_change_.cpMax)) &&
      ((new_sel.cpMin != sel_before_change_.cpMin) ||
       (new_sel.cpMax != sel_before_change_.cpMax));

  // See if the text or selection have changed since OnBeforePossibleChange().
  const string16 new_text(GetText());
  const bool text_differs = (new_text != text_before_change_) ||
      force_text_changed;

  // When the user has deleted text, we don't allow inline autocomplete.  Make
  // sure to not flag cases like selecting part of the text and then pasting
  // (or typing) the prefix of that selection.  (We detect these by making
  // sure the caret, which should be after any insertion, hasn't moved
  // forward of the old selection start.)
  const bool just_deleted_text =
      (text_before_change_.length() > new_text.length()) &&
      (new_sel.cpMin <= std::min(sel_before_change_.cpMin,
                                 sel_before_change_.cpMax));

  const bool something_changed = model_->OnAfterPossibleChange(
      text_before_change_, new_text, new_sel.cpMin, new_sel.cpMax,
      selection_differs, text_differs, just_deleted_text, !IsImeComposing());

  if (selection_differs)
    controller_->OnSelectionBoundsChanged();

  if (something_changed && text_differs)
    TextChanged();

  if (text_differs) {
    // Note that a TEXT_CHANGED event implies that the cursor/selection
    // probably changed too, so we don't need to send both.
    native_view_host_->GetWidget()->NotifyAccessibilityEvent(
        native_view_host_, ui::AccessibilityTypes::EVENT_TEXT_CHANGED, true);
  } else if (selection_differs) {
    // Notify assistive technology that the cursor or selection changed.
    native_view_host_->GetWidget()->NotifyAccessibilityEvent(
        native_view_host_,
        ui::AccessibilityTypes::EVENT_SELECTION_CHANGED,
        true);
  } else if (delete_at_end_pressed_) {
    model_->OnChanged();
  }

  return something_changed;
}

gfx::NativeView OmniboxViewWin::GetNativeView() const {
  return m_hWnd;
}

// static
gfx::NativeView OmniboxViewWin::GetRelativeWindowForNativeView(
    gfx::NativeView edit_native_view) {
  // When an IME is attached to the rich-edit control, retrieve its window
  // handle, and the popup window of AutocompletePopupView will be shown
  // under the IME windows.
  // Otherwise, the popup window will be shown under top-most windows.
  // TODO(hbono): http://b/1111369 if we exclude this popup window from the
  // display area of IME windows, this workaround becomes unnecessary.
  HWND ime_window = ImmGetDefaultIMEWnd(edit_native_view);
  return ime_window ? ime_window : HWND_NOTOPMOST;
}

gfx::NativeView OmniboxViewWin::GetRelativeWindowForPopup() const {
  return GetRelativeWindowForNativeView(GetNativeView());
}

CommandUpdater* OmniboxViewWin::GetCommandUpdater() {
  return command_updater_;
}

void OmniboxViewWin::SetInstantSuggestion(const string16& suggestion,
                                          bool animate_to_complete) {
  parent_view_->SetInstantSuggestion(suggestion, animate_to_complete);
}

int OmniboxViewWin::TextWidth() const {
  return WidthNeededToDisplay(GetText());
}

string16 OmniboxViewWin::GetInstantSuggestion() const {
  return parent_view_->GetInstantSuggestion();
}

bool OmniboxViewWin::IsImeComposing() const {
  bool ime_composing = false;
  HIMC context = ImmGetContext(m_hWnd);
  if (context) {
    ime_composing = !!ImmGetCompositionString(context, GCS_COMPSTR, NULL, 0);
    ImmReleaseContext(m_hWnd, context);
  }
  return ime_composing;
}

int OmniboxViewWin::GetMaxEditWidth(int entry_width) const {
  RECT formatting_rect;
  GetRect(&formatting_rect);
  RECT edit_bounds;
  GetClientRect(&edit_bounds);
  return entry_width - formatting_rect.left -
      (edit_bounds.right - formatting_rect.right);
}

views::View* OmniboxViewWin::AddToView(views::View* parent) {
  native_view_host_ = new OmniboxViewWrapper(this);
  parent->AddChildView(native_view_host_);
  native_view_host_->set_focus_view(parent);
  native_view_host_->Attach(GetNativeView());
  return native_view_host_;
}

int OmniboxViewWin::OnPerformDrop(const views::DropTargetEvent& event) {
  return OnPerformDropImpl(event, false);
}

int OmniboxViewWin::OnPerformDropImpl(const views::DropTargetEvent& event,
                                      bool in_drag) {
  const ui::OSExchangeData& data = event.data();

  if (data.HasURL()) {
    GURL url;
    string16 title;
    if (data.GetURLAndTitle(&url, &title)) {
      string16 text(StripJavascriptSchemas(UTF8ToUTF16(url.spec())));
      SetUserText(text);
      model()->AcceptInput(CURRENT_TAB, true);
      return CopyOrLinkDragOperation(event.source_operations());
    }
  } else if (data.HasString()) {
    int string_drop_position = drop_highlight_position();
    string16 text;
    if ((string_drop_position != -1 || !in_drag) && data.GetString(&text)) {
      DCHECK(string_drop_position == -1 ||
             ((string_drop_position >= 0) &&
              (string_drop_position <= GetTextLength())));
      if (in_drag) {
        if (event.source_operations()== ui::DragDropTypes::DRAG_MOVE)
          MoveSelectedText(string_drop_position);
        else
          InsertText(string_drop_position, text);
      } else {
        PasteAndGo(CollapseWhitespace(text, true));
      }
      return CopyOrLinkDragOperation(event.source_operations());
    }
  }

  return ui::DragDropTypes::DRAG_NONE;
}

void OmniboxViewWin::PasteAndGo(const string16& text) {
  if (CanPasteAndGo(text))
    model_->PasteAndGo();
}

bool OmniboxViewWin::SkipDefaultKeyEventProcessing(
    const views::KeyEvent& event) {
  ui::KeyboardCode key = event.key_code();
  // We don't process ALT + numpad digit as accelerators, they are used for
  // entering special characters.  We do translate alt-home.
  if (event.IsAltDown() && (key != ui::VKEY_HOME) &&
      views::NativeTextfieldWin::IsNumPadDigit(key,
          (event.flags() & ui::EF_EXTENDED) != 0))
    return true;

  // Skip accelerators for key combinations omnibox wants to crack. This list
  // should be synced with OnKeyDownOnlyWritable() (but for tab which is dealt
  // with above in LocationBarView::SkipDefaultKeyEventProcessing).
  //
  // We cannot return true for all keys because we still need to handle some
  // accelerators (e.g., F5 for reload the page should work even when the
  // Omnibox gets focused).
  switch (key) {
    case ui::VKEY_ESCAPE: {
      ScopedFreeze freeze(this, GetTextObjectModel());
      return model_->OnEscapeKeyPressed();
    }

    case ui::VKEY_RETURN:
      return true;

    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
      return !event.IsAltDown();

    case ui::VKEY_DELETE:
    case ui::VKEY_INSERT:
      return !event.IsAltDown() && event.IsShiftDown() &&
          !event.IsControlDown();

    case ui::VKEY_X:
    case ui::VKEY_V:
      return !event.IsAltDown() && event.IsControlDown();

    case ui::VKEY_BACK:
    case ui::VKEY_OEM_PLUS:
      return true;

    default:
      return false;
  }
}

void OmniboxViewWin::HandleExternalMsg(UINT msg,
                                       UINT flags,
                                       const CPoint& screen_point) {
  if (msg == WM_CAPTURECHANGED) {
    SendMessage(msg, 0, NULL);
    return;
  }

  CPoint client_point(screen_point);
  ::MapWindowPoints(NULL, m_hWnd, &client_point, 1);
  SendMessage(msg, flags, MAKELPARAM(client_point.x, client_point.y));
}

bool OmniboxViewWin::IsCommandIdChecked(int command_id) const {
  return false;
}

bool OmniboxViewWin::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDS_UNDO:         return !!CanUndo();
    case IDC_CUT:          return !!CanCut();
    case IDC_COPY:         return !!CanCopy();
    case IDC_PASTE:        return !!CanPaste();
    case IDS_PASTE_AND_GO: return CanPasteAndGo(GetClipboardText());
    case IDS_SELECT_ALL:   return !!CanSelectAll();
    case IDS_EDIT_SEARCH_ENGINES:
      return command_updater_->IsCommandEnabled(IDC_EDIT_SEARCH_ENGINES);
    default:
      NOTREACHED();
      return false;
  }
}

bool OmniboxViewWin::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return parent_view_->GetWidget()->GetAccelerator(command_id, accelerator);
}

bool OmniboxViewWin::IsItemForCommandIdDynamic(int command_id) const {
  // No need to change the default IDS_PASTE_AND_GO label unless this is a
  // search.
  return command_id == IDS_PASTE_AND_GO;
}

string16 OmniboxViewWin::GetLabelForCommandId(int command_id) const {
  DCHECK_EQ(IDS_PASTE_AND_GO, command_id);
  return l10n_util::GetStringUTF16(model_->is_paste_and_search() ?
      IDS_PASTE_AND_SEARCH : IDS_PASTE_AND_GO);
}

void OmniboxViewWin::ExecuteCommand(int command_id) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  if (command_id == IDS_PASTE_AND_GO) {
    // This case is separate from the switch() below since we don't want to wrap
    // it in OnBefore/AfterPossibleChange() calls.
    model_->PasteAndGo();
    return;
  }

  OnBeforePossibleChange();
  switch (command_id) {
    case IDS_UNDO:
      Undo();
      break;

    case IDC_CUT:
      Cut();
      break;

    case IDC_COPY:
      Copy();
      break;

    case IDC_PASTE:
      Paste();
      break;

    case IDS_SELECT_ALL:
      SelectAll(false);
      break;

    case IDS_EDIT_SEARCH_ENGINES:
      command_updater_->ExecuteCommand(IDC_EDIT_SEARCH_ENGINES);
      break;

    default:
      NOTREACHED();
      break;
  }
  OnAfterPossibleChange();
}

// static
int CALLBACK OmniboxViewWin::WordBreakProc(LPTSTR edit_text,
                                           int current_pos,
                                           int num_bytes,
                                           int action) {
  // TODO(pkasting): http://b/1111308 We should let other people, like ICU and
  // GURL, do the work for us here instead of writing all this ourselves.

  // Sadly, even though the MSDN docs claim that the third parameter here is a
  // number of characters, they lie.  It's a number of bytes.
  const int length = num_bytes / sizeof(wchar_t);

  // With no clear guidance from the MSDN docs on how to handle "not found" in
  // the "find the nearest xxx..." cases below, I cap the return values at
  // [0, length].  Since one of these (0) is also a valid position, the return
  // values are thus ambiguous :(
  switch (action) {
    // Find nearest character before current position that begins a word.
    case WB_LEFT:
    case WB_MOVEWORDLEFT: {
      if (current_pos < 2) {
        // Either current_pos == 0, so we have a "not found" case and return 0,
        // or current_pos == 1, and the only character before this position is
        // at 0.
        return 0;
      }

      // Look for a delimiter before the previous character; the previous word
      // starts immediately after.  (If we looked for a delimiter before the
      // current character, we could stop on the immediate prior character,
      // which would mean we'd return current_pos -- which isn't "before the
      // current position".)
      const int prev_delim =
          WordBreakProc(edit_text, current_pos - 1, num_bytes, WB_LEFTBREAK);

      if ((prev_delim == 0) &&
          !WordBreakProc(edit_text, 0, num_bytes, WB_ISDELIMITER)) {
        // Got back 0, but position 0 isn't a delimiter.  This was a "not
        // found" 0, so return one of our own.
        return 0;
      }

      return prev_delim + 1;
    }

    // Find nearest character after current position that begins a word.
    case WB_RIGHT:
    case WB_MOVEWORDRIGHT: {
      if (WordBreakProc(edit_text, current_pos, num_bytes, WB_ISDELIMITER)) {
        // The current character is a delimiter, so the next character starts
        // a new word.  Done.
        return current_pos + 1;
      }

      // Look for a delimiter after the current character; the next word starts
      // immediately after.
      const int next_delim =
          WordBreakProc(edit_text, current_pos, num_bytes, WB_RIGHTBREAK);
      if (next_delim == length) {
        // Didn't find a delimiter.  Return length to signal "not found".
        return length;
      }

      return next_delim + 1;
    }

    // Determine if the current character delimits words.
    case WB_ISDELIMITER:
      return !!(WordBreakProc(edit_text, current_pos, num_bytes, WB_CLASSIFY) &
                WBF_BREAKLINE);

    // Return the classification of the current character.
    case WB_CLASSIFY:
      if (IsWhitespace(edit_text[current_pos])) {
        // Whitespace normally breaks words, but the MSDN docs say that we must
        // not break on the CRs in a "CR, LF" or a "CR, CR, LF" sequence.  Just
        // check for an arbitrarily long sequence of CRs followed by LF and
        // report "not a delimiter" for the current CR in that case.
        while ((current_pos < (length - 1)) &&
               (edit_text[current_pos] == 0x13)) {
          if (edit_text[++current_pos] == 0x10)
            return WBF_ISWHITE;
        }
        return WBF_BREAKLINE | WBF_ISWHITE;
      }

      // Punctuation normally breaks words, but the first two characters in
      // "://" (end of scheme) should not be breaks, so that "http://" will be
      // treated as one word.
      if (ispunct(edit_text[current_pos], std::locale()) &&
          !SchemeEnd(edit_text, current_pos, length) &&
          !SchemeEnd(edit_text, current_pos - 1, length))
        return WBF_BREAKLINE;

      // Normal character, no flags.
      return 0;

    // Finds nearest delimiter before current position.
    case WB_LEFTBREAK:
      for (int i = current_pos - 1; i >= 0; --i) {
        if (WordBreakProc(edit_text, i, num_bytes, WB_ISDELIMITER))
          return i;
      }
      return 0;

    // Finds nearest delimiter after current position.
    case WB_RIGHTBREAK:
      for (int i = current_pos + 1; i < length; ++i) {
        if (WordBreakProc(edit_text, i, num_bytes, WB_ISDELIMITER))
          return i;
      }
      return length;
  }

  NOTREACHED();
  return 0;
}

// static
bool OmniboxViewWin::SchemeEnd(LPTSTR edit_text,
                               int current_pos,
                               int length) {
  return (current_pos >= 0) &&
         ((length - current_pos) > 2) &&
         (edit_text[current_pos] == ':') &&
         (edit_text[current_pos + 1] == '/') &&
         (edit_text[current_pos + 2] == '/');
}

void OmniboxViewWin::OnChar(TCHAR ch, UINT repeat_count, UINT flags) {
  // Don't let alt-enter beep.  Not sure this is necessary, as the standard
  // alt-enter will hit DiscardWMSysChar() and get thrown away, and
  // ctrl-alt-enter doesn't seem to reach here for some reason?  At least not on
  // my system... still, this is harmless and maybe necessary in other locales.
  if (ch == VK_RETURN && (flags & KF_ALTDOWN))
    return;

  // Escape is processed in OnKeyDown.  Don't let any WM_CHAR messages propagate
  // as we don't want the RichEdit to do anything funky.
  if (ch == VK_ESCAPE && !(flags & KF_ALTDOWN))
    return;

  if (ch == VK_TAB) {
    // Don't add tabs to the input.
    return;
  }

  HandleKeystroke(GetCurrentMessage()->message, ch, repeat_count, flags);
}

void OmniboxViewWin::OnContextMenu(HWND window, const CPoint& point) {
  BuildContextMenu();
  if (point.x == -1 || point.y == -1) {
    POINT p;
    GetCaretPos(&p);
    MapWindowPoints(HWND_DESKTOP, &p, 1);
    context_menu_->RunContextMenuAt(gfx::Point(p));
  } else {
    context_menu_->RunContextMenuAt(gfx::Point(point));
  }
}

void OmniboxViewWin::OnCopy() {
  string16 text(GetSelectedText());
  if (text.empty())
    return;

  CHARRANGE sel;
  GURL url;
  bool write_url = false;
  GetSel(sel);
  // GetSel() doesn't preserve selection direction, so sel.cpMin will always be
  // the smaller value.
  model_->AdjustTextForCopy(sel.cpMin, IsSelectAll(), &text, &url, &write_url);
  ui::ScopedClipboardWriter scw(g_browser_process->clipboard(),
                                ui::Clipboard::BUFFER_STANDARD);
  scw.WriteText(text);
  if (write_url) {
    scw.WriteBookmark(text, url.spec());
    scw.WriteHyperlink(net::EscapeForHTML(text), url.spec());
  }
}

void OmniboxViewWin::OnCut() {
  OnCopy();

  // This replace selection will have no effect (even on the undo stack) if the
  // current selection is empty.
  ReplaceSel(L"", true);
}

LRESULT OmniboxViewWin::OnGetObject(UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam) {
  // This is a request for the native accessibility object.
  if (lparam == OBJID_CLIENT) {
    return LresultFromObject(IID_IAccessible, wparam,
                             native_view_host_->GetNativeViewAccessible());
  }
  return 0;
}

LRESULT OmniboxViewWin::OnImeComposition(UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam) {
  if (ignore_ime_messages_) {
    // This message was sent while we're in the middle of meddling with the
    // underlying edit control.  If we handle it below, OnAfterPossibleChange()
    // can get bogus text for the edit, and rerun autocomplete, destructively
    // modifying the result set that we're in the midst of using.  For example,
    // if SetWindowTextAndCaretPos() was called due to the user clicking an
    // entry in the popup, we're in the middle of executing SetSelectedLine(),
    // and changing the results can cause checkfailures.
    return DefWindowProc(message, wparam, lparam);
  }

  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  LRESULT result = DefWindowProc(message, wparam, lparam);
  // Force an IME composition confirmation operation to trigger the text_changed
  // code in OnAfterPossibleChange(), even if identical contents are confirmed,
  // to make sure the model can update its internal states correctly.
  OnAfterPossibleChangeInternal((lparam & GCS_RESULTSTR) != 0);
  return result;
}

LRESULT OmniboxViewWin::OnImeNotify(UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam) {
  // Close the popup when the IME composition window is open, so they don't
  // overlap.
  switch (wparam) {
    case IMN_OPENCANDIDATE:
      ime_candidate_window_open_ = true;
      ClosePopup();
      break;
    case IMN_CLOSECANDIDATE:
      ime_candidate_window_open_ = false;

      // UpdatePopup assumes user input is in progress, so only call it if
      // that's the case. Otherwise, autocomplete may run on an empty user
      // text. For example, Baidu Japanese IME sends IMN_CLOSECANDIDATE when
      // composition mode is entered, but the user may not have input anything
      // yet.
      if (model_->user_input_in_progress())
        UpdatePopup();

      break;
    default:
      break;
  }
  return DefWindowProc(message, wparam, lparam);
}

LRESULT OmniboxViewWin::OnPointerDown(UINT message,
                                      WPARAM wparam,
                                      LPARAM lparam) {
  SetFocus();
  // ITextInputPanel is not supported on all platforms.  NULL is fine.
  if (keyboard_ != NULL)
    keyboard_->SetInPlaceVisibility(true);
  return DefWindowProc(message, wparam, lparam);
}

void OmniboxViewWin::OnKeyDown(TCHAR key,
                               UINT repeat_count,
                               UINT flags) {
  delete_at_end_pressed_ = false;

  if (OnKeyDownAllModes(key, repeat_count, flags))
    return;

  // Make sure that we handle system key events like Alt-F4.
  if (popup_window_mode_) {
    DefWindowProc(GetCurrentMessage()->message, key, MAKELPARAM(repeat_count,
                                                                flags));
    return;
  }

  if (OnKeyDownOnlyWritable(key, repeat_count, flags))
    return;

  // CRichEditCtrl changes its text on WM_KEYDOWN instead of WM_CHAR for many
  // different keys (backspace, ctrl-v, ...), so we call this in both cases.
  HandleKeystroke(GetCurrentMessage()->message, key, repeat_count, flags);
}

void OmniboxViewWin::OnKeyUp(TCHAR key,
                             UINT repeat_count,
                             UINT flags) {
  if (key == VK_CONTROL)
    model_->OnControlKeyChanged(false);

  // On systems with RTL input languages, ctrl+shift toggles the reading order
  // (depending on which shift key is pressed). But by default the CRichEditCtrl
  // only changes the current reading order, and as soon as the user deletes all
  // the text, or we call SetWindowText(), it reverts to the "default" order.
  // To work around this, if the user hits ctrl+shift, we pass it to
  // DefWindowProc() while the edit is empty, which toggles the default reading
  // order; then we restore the user's input.
  if (!(flags & KF_ALTDOWN) &&
      (((key == VK_CONTROL) && (GetKeyState(VK_SHIFT) < 0)) ||
       ((key == VK_SHIFT) && (GetKeyState(VK_CONTROL) < 0)))) {
    ScopedFreeze freeze(this, GetTextObjectModel());

    string16 saved_text(GetText());
    CHARRANGE saved_sel;
    GetSelection(saved_sel);

    SetWindowText(L"");

    DefWindowProc(WM_KEYUP, key, MAKELPARAM(repeat_count, flags));

    SetWindowText(saved_text.c_str());
    SetSelectionRange(saved_sel);
    return;
  }

  SetMsgHandled(false);
}

void OmniboxViewWin::OnKillFocus(HWND focus_wnd) {
  if (m_hWnd == focus_wnd) {
    // Focus isn't actually leaving.
    SetMsgHandled(false);
    return;
  }

  // This must be invoked before ClosePopup.
  model_->OnWillKillFocus(focus_wnd);

  // Close the popup.
  ClosePopup();

  // Save the user's existing selection to restore it later.
  GetSelection(saved_selection_for_focus_change_);

  // Tell the model to reset itself.
  model_->OnKillFocus();

  // Let the CRichEditCtrl do its default handling.  This will complete any
  // in-progress IME composition.  We must do this after setting has_focus_ to
  // false so that UpdatePopup() will know not to rerun autocomplete.
  ScopedFreeze freeze(this, GetTextObjectModel());
  DefWindowProc(WM_KILLFOCUS, reinterpret_cast<WPARAM>(focus_wnd), 0);

  // Cancel any user selection and scroll the text back to the beginning of the
  // URL.  We have to do this after calling DefWindowProc() because otherwise
  // an in-progress IME composition will be completed at the new caret position,
  // resulting in the string jumping unexpectedly to the front of the edit.
  //
  // Crazy hack: If we just do PlaceCaretAt(0), and the beginning of the text is
  // currently scrolled out of view, we can wind up with a blinking cursor in
  // the toolbar at the current X coordinate of the beginning of the text.  By
  // first doing a reverse-select-all to scroll the beginning of the text into
  // view, we work around this CRichEditCtrl bug.
  SelectAll(true);
  PlaceCaretAt(0);
}

void OmniboxViewWin::OnLButtonDblClk(UINT keys, const CPoint& point) {
  // Save the double click info for later triple-click detection.
  tracking_double_click_ = true;
  double_click_point_ = point;
  double_click_time_ = GetCurrentMessage()->time;
  possible_drag_ = false;

  // Modifying the selection counts as accepting any inline autocompletion, so
  // track "changes" made by clicking the mouse button.
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONDBLCLK, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, false), point.y));
  OnAfterPossibleChange();

  gaining_focus_.reset();  // See NOTE in OnMouseActivate().
}

void OmniboxViewWin::OnLButtonDown(UINT keys, const CPoint& point) {
  TrackMousePosition(kLeft, point);
  if (gaining_focus_.get()) {
    // When Chrome was already the activated app, we haven't reached
    // OnSetFocus() yet.  When we get there, don't restore the saved selection,
    // since it will just screw up the user's interaction with the edit.
    saved_selection_for_focus_change_.cpMin = -1;

    // Crazy hack: In this particular case, the CRichEditCtrl seems to have an
    // internal flag that discards the next WM_LBUTTONDOWN without processing
    // it, so that clicks on the edit when its owning app is not activated are
    // eaten rather than processed (despite whatever the return value of
    // DefWindowProc(WM_MOUSEACTIVATE, ...) may say).  This behavior is
    // confusing and we want the click to be treated normally.  So, to reset the
    // CRichEditCtrl's internal flag, we pass it an extra WM_LBUTTONDOWN here
    // (as well as a matching WM_LBUTTONUP, just in case we'd be confusing some
    // kind of state tracking otherwise).
    DefWindowProc(WM_LBUTTONDOWN, keys, MAKELPARAM(point.x, point.y));
    DefWindowProc(WM_LBUTTONUP, keys, MAKELPARAM(point.x, point.y));
  }

  // Check for triple click, then reset tracker.  Should be safe to subtract
  // double_click_time_ from the current message's time even if the timer has
  // wrapped in between.
  const bool is_triple_click = tracking_double_click_ &&
      views::NativeTextfieldWin::IsDoubleClick(double_click_point_, point,
          GetCurrentMessage()->time - double_click_time_);
  tracking_double_click_ = false;

  if (!gaining_focus_.get() && !is_triple_click)
    OnPossibleDrag(point);


  // Modifying the selection counts as accepting any inline autocompletion, so
  // track "changes" made by clicking the mouse button.
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONDOWN, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, is_triple_click),
                           point.y));
  OnAfterPossibleChange();

  gaining_focus_.reset();
}

void OmniboxViewWin::OnLButtonUp(UINT keys, const CPoint& point) {
  // default processing should happen first so we can see the result of the
  // selection
  ScopedFreeze freeze(this, GetTextObjectModel());
  DefWindowProc(WM_LBUTTONUP, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, false), point.y));

  SelectAllIfNecessary(kLeft, point);

  tracking_click_[kLeft] = false;

  possible_drag_ = false;
}

void OmniboxViewWin::OnMButtonDblClk(UINT /*keys*/, const CPoint& /*point*/) {
  gaining_focus_.reset();  // See NOTE in OnMouseActivate().

  // By default, the edit responds to middle-clicks by capturing the mouse and
  // ignoring all subsequent events until it receives another click (of any of
  // the left, middle, or right buttons).  This bizarre behavior is not only
  // useless but can cause the UI to appear unresponsive if a user accidentally
  // middle-clicks the edit (instead of a tab above it), so we purposefully eat
  // this message (instead of calling SetMsgHandled(false)) to avoid triggering
  // this.
}

void OmniboxViewWin::OnMButtonDown(UINT /*keys*/, const CPoint& /*point*/) {
  tracking_double_click_ = false;

  // See note in OnMButtonDblClk above.
}

void OmniboxViewWin::OnMButtonUp(UINT /*keys*/, const CPoint& /*point*/) {
  possible_drag_ = false;

  // See note in OnMButtonDblClk above.
}

LRESULT OmniboxViewWin::OnMouseActivate(HWND window,
                                        UINT hit_test,
                                        UINT mouse_message) {
  // First, give other handlers a chance to handle the message to see if we are
  // actually going to activate and gain focus.
  LRESULT result = DefWindowProc(WM_MOUSEACTIVATE,
                                 reinterpret_cast<WPARAM>(window),
                                 MAKELPARAM(hit_test, mouse_message));
  // Check if we're getting focus from a click.  We have to do this here rather
  // than in OnXButtonDown() since in many scenarios OnSetFocus() will be
  // reached before OnXButtonDown(), preventing us from detecting this properly
  // there.  Also in those cases, we need to already know in OnSetFocus() that
  // we should not restore the saved selection.
  if (!model_->has_focus() &&
      ((mouse_message == WM_LBUTTONDOWN || mouse_message == WM_RBUTTONDOWN)) &&
      (result == MA_ACTIVATE)) {
    DCHECK(!gaining_focus_.get());
    gaining_focus_.reset(new ScopedFreeze(this, GetTextObjectModel()));
    // NOTE: Despite |mouse_message| being WM_XBUTTONDOWN here, we're not
    // guaranteed to call OnXButtonDown() later!  Specifically, if this is the
    // second click of a double click, we'll reach here but later call
    // OnXButtonDblClk().  Make sure |gaining_focus_| gets reset both places,
    // or we'll have visual glitchiness and then DCHECK failures.

    // Don't restore saved selection, it will just screw up our interaction
    // with this edit.
    saved_selection_for_focus_change_.cpMin = -1;
  }
  return result;
}

void OmniboxViewWin::OnMouseMove(UINT keys, const CPoint& point) {
  if (possible_drag_) {
    StartDragIfNecessary(point);
    // Don't fall through to default mouse handling, otherwise a second
    // drag session may start.
    return;
  }

  if (tracking_click_[kLeft] && !IsDrag(click_point_[kLeft], point))
    return;

  tracking_click_[kLeft] = false;

  // Return quickly if this can't change the selection/cursor, so we don't
  // create a ScopedFreeze (and thus do an UpdateWindow()) on every
  // WM_MOUSEMOVE.
  if (!(keys & MK_LBUTTON)) {
    DefWindowProc(WM_MOUSEMOVE, keys, MAKELPARAM(point.x, point.y));
    return;
  }

  // Clamp the selection to the visible text so the user can't drag to select
  // the "phantom newline".  In theory we could achieve this by clipping the X
  // coordinate, but in practice the edit seems to behave nondeterministically
  // with similar sequences of clipped input coordinates fed to it.  Maybe it's
  // reading the mouse cursor position directly?
  //
  // This solution has a minor visual flaw, however: if there's a visible cursor
  // at the edge of the text (only true when there's no selection), dragging the
  // mouse around outside that edge repaints the cursor on every WM_MOUSEMOVE
  // instead of allowing it to blink normally.  To fix this, we special-case
  // this exact case and discard the WM_MOUSEMOVE messages instead of passing
  // them along.
  //
  // But even this solution has a flaw!  (Argh.)  In the case where the user has
  // a selection that starts at the edge of the edit, and proceeds to the middle
  // of the edit, and the user is dragging back past the start edge to remove
  // the selection, there's a redraw problem where the change between having the
  // last few bits of text still selected and having nothing selected can be
  // slow to repaint (which feels noticeably strange).  This occurs if you only
  // let the edit receive a single WM_MOUSEMOVE past the edge of the text.  I
  // think on each WM_MOUSEMOVE the edit is repainting its previous state, then
  // updating its internal variables to the new state but not repainting.  To
  // fix this, we allow one more WM_MOUSEMOVE through after the selection has
  // supposedly been shrunk to nothing; this makes the edit redraw the selection
  // quickly so it feels smooth.
  CHARRANGE selection;
  GetSel(selection);
  const bool possibly_can_discard_mousemove =
      (selection.cpMin == selection.cpMax) &&
      (((selection.cpMin == 0) &&
        (ClipXCoordToVisibleText(point.x, false) > point.x)) ||
       ((selection.cpMin == GetTextLength()) &&
        (ClipXCoordToVisibleText(point.x, false) < point.x)));
  if (!can_discard_mousemove_ || !possibly_can_discard_mousemove) {
    can_discard_mousemove_ = possibly_can_discard_mousemove;
    ScopedFreeze freeze(this, GetTextObjectModel());
    OnBeforePossibleChange();
    // Force the Y coordinate to the center of the clip rect.  The edit
    // behaves strangely when the cursor is dragged vertically: if the cursor
    // is in the middle of the text, drags inside the clip rect do nothing,
    // and drags outside the clip rect act as if the cursor jumped to the
    // left edge of the text.  When the cursor is at the right edge, drags of
    // just a few pixels vertically end up selecting the "phantom newline"...
    // sometimes.
    RECT r;
    GetRect(&r);
    DefWindowProc(WM_MOUSEMOVE, keys,
                  MAKELPARAM(point.x, (r.bottom - r.top) / 2));
    OnAfterPossibleChange();
  }
}

void OmniboxViewWin::OnPaint(HDC bogus_hdc) {
  // We need to paint over the top of the edit.  If we simply let the edit do
  // its default painting, then do ours into the window DC, the screen is
  // updated in between and we can get flicker.  To avoid this, we force the
  // edit to paint into a memory DC, which we also paint onto, then blit the
  // whole thing to the screen.

  // Don't paint if not necessary.
  CRect paint_clip_rect;
  if (!GetUpdateRect(&paint_clip_rect, true))
    return;

  // Begin painting, and create a memory DC for the edit to paint into.
  CPaintDC paint_dc(m_hWnd);
  CDC memory_dc(CreateCompatibleDC(paint_dc));
  CRect rect;
  GetClientRect(&rect);
  // NOTE: This next call uses |paint_dc| instead of |memory_dc| because
  // |memory_dc| contains a 1x1 monochrome bitmap by default, which will cause
  // |memory_bitmap| to be monochrome, which isn't what we want.
  CBitmap memory_bitmap(CreateCompatibleBitmap(paint_dc, rect.Width(),
                                               rect.Height()));
  HBITMAP old_bitmap = memory_dc.SelectBitmap(memory_bitmap);

  // Tell our intercept functions to supply our memory DC to the edit when it
  // tries to call BeginPaint().
  //
  // The sane way to do this would be to use WM_PRINTCLIENT to ask the edit to
  // paint into our desired DC.  Unfortunately, the Rich Edit 3.0 that ships
  // with Windows 2000/XP/Vista doesn't handle WM_PRINTCLIENT correctly; it
  // treats it just like WM_PAINT and calls BeginPaint(), ignoring our provided
  // DC.  The Rich Edit 6.0 that ships with Office 2007 handles this better, but
  // has other issues, and we can't redistribute that DLL anyway.  So instead,
  // we use this scary hack.
  //
  // NOTE: It's possible to get nested paint calls (!) (try setting the
  // permanent URL to something longer than the edit width, then selecting the
  // contents of the edit, typing a character, and hitting <esc>), so we can't
  // DCHECK(!edit_hwnd_) here.  Instead, just save off the old HWND, which most
  // of the time will be NULL.
  HWND old_edit_hwnd = edit_hwnd;
  edit_hwnd = m_hWnd;
  paint_struct = paint_dc.m_ps;
  paint_struct.hdc = memory_dc;
  DefWindowProc(WM_PAINT, reinterpret_cast<WPARAM>(bogus_hdc), 0);

  // Make the selection look better.
  EraseTopOfSelection(&memory_dc, rect, paint_clip_rect);

  // Draw a slash through the scheme if this is insecure.
  if (insecure_scheme_component_.is_nonempty())
    DrawSlashForInsecureScheme(memory_dc, rect, paint_clip_rect);

  // Draw the drop highlight.
  if (drop_highlight_position_ != -1)
    DrawDropHighlight(memory_dc, rect, paint_clip_rect);

  // Blit the memory DC to the actual paint DC and clean up.
  BitBlt(paint_dc, rect.left, rect.top, rect.Width(), rect.Height(), memory_dc,
         rect.left, rect.top, SRCCOPY);
  memory_dc.SelectBitmap(old_bitmap);
  edit_hwnd = old_edit_hwnd;
}

void OmniboxViewWin::OnPaste() {
  // Replace the selection if we have something to paste.
  const string16 text(GetClipboardText());
  if (!text.empty()) {
    // Record this paste, so we can do different behavior.
    model_->on_paste();
    // Force a Paste operation to trigger the text_changed code in
    // OnAfterPossibleChange(), even if identical contents are pasted into the
    // text box.
    text_before_change_.clear();
    ReplaceSel(text.c_str(), true);
  }
}

void OmniboxViewWin::OnRButtonDblClk(UINT /*keys*/, const CPoint& /*point*/) {
  gaining_focus_.reset();  // See NOTE in OnMouseActivate().
  SetMsgHandled(false);
}

void OmniboxViewWin::OnRButtonDown(UINT /*keys*/, const CPoint& point) {
  TrackMousePosition(kRight, point);
  tracking_double_click_ = false;
  possible_drag_ = false;
  gaining_focus_.reset();
  SetMsgHandled(false);
}

void OmniboxViewWin::OnRButtonUp(UINT /*keys*/, const CPoint& point) {
  SelectAllIfNecessary(kRight, point);
  tracking_click_[kRight] = false;
  SetMsgHandled(false);
}

void OmniboxViewWin::OnSetFocus(HWND focus_wnd) {
  views::FocusManager* focus_manager = parent_view_->GetFocusManager();
  if (focus_manager) {
    // Notify the FocusManager that the focused view is now the location bar
    // (our parent view).
    focus_manager->SetFocusedView(parent_view_);
  } else {
    NOTREACHED();
  }

  model_->OnSetFocus(GetKeyState(VK_CONTROL) < 0);

  // Restore saved selection if available.
  if (saved_selection_for_focus_change_.cpMin != -1) {
    SetSelectionRange(saved_selection_for_focus_change_);
    saved_selection_for_focus_change_.cpMin = -1;
  }

  SetMsgHandled(false);
}

LRESULT OmniboxViewWin::OnSetText(const wchar_t* text) {
  // Ignore all IME messages while we process this WM_SETTEXT message.
  // When SetWindowText() is called while an IME is composing text, the IME
  // calls SendMessage() to send a WM_IME_COMPOSITION message. When we receive
  // this WM_IME_COMPOSITION message, we update the omnibox and may call
  // SetWindowText() again. To stop this recursive message-handler call, we
  // stop updating the omnibox while we process a WM_SETTEXT message.
  // We wouldn't need to do this update anyway, because either we're in the
  // middle of updating the omnibox already or the caller of SetWindowText()
  // is going to update the omnibox next.
  AutoReset<bool> auto_reset_ignore_ime_messages(&ignore_ime_messages_, true);
  return DefWindowProc(WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text));
}

void OmniboxViewWin::OnSysChar(TCHAR ch,
                               UINT repeat_count,
                               UINT flags) {
  // Nearly all alt-<xxx> combos result in beeping rather than doing something
  // useful, so we discard most.  Exceptions:
  //   * ctrl-alt-<xxx>, which is sometimes important, generates WM_CHAR instead
  //     of WM_SYSCHAR, so it doesn't need to be handled here.
  //   * alt-space gets translated by the default WM_SYSCHAR handler to a
  //     WM_SYSCOMMAND to open the application context menu, so we need to allow
  //     it through.
  if (ch == VK_SPACE)
    SetMsgHandled(false);
}

void OmniboxViewWin::OnWindowPosChanging(WINDOWPOS* window_pos) {
  if (force_hidden_)
    window_pos->flags &= ~SWP_SHOWWINDOW;
  SetMsgHandled(true);
}

BOOL OmniboxViewWin::OnMouseWheel(UINT flags, short delta, CPoint point) {
  // Forward the mouse-wheel message to the window under the mouse.
  if (!ui::RerouteMouseWheel(m_hWnd, MAKEWPARAM(flags, delta),
                             MAKELPARAM(point.x, point.y)))
    SetMsgHandled(false);
  return 0;
}

void OmniboxViewWin::HandleKeystroke(UINT message,
                                     TCHAR key,
                                     UINT repeat_count,
                                     UINT flags) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();

  if (key == ui::VKEY_HOME || key == ui::VKEY_END) {
    // DefWindowProc() might reset the keyboard layout when it receives a
    // keydown event for VKEY_HOME or VKEY_END. When the window was created
    // with WS_EX_LAYOUTRTL and the current keyboard layout is not a RTL one,
    // if the input text is pure LTR text, the layout changes to the first RTL
    // keyboard layout in keyboard layout queue; if the input text is
    // bidirectional text, the layout changes to the keyboard layout of the
    // first RTL character in input text. When the window was created without
    // WS_EX_LAYOUTRTL and the current keyboard layout is not a LTR one, if the
    // input text is pure RTL text, the layout changes to English; if the input
    // text is bidirectional text, the layout changes to the keyboard layout of
    // the first LTR character in input text. Such keyboard layout change
    // behavior is surprising and inconsistent with keyboard behavior
    // elsewhere, so reset the layout in this case.
    HKL layout = GetKeyboardLayout(0);
    DefWindowProc(message, key, MAKELPARAM(repeat_count, flags));
    ActivateKeyboardLayout(layout, KLF_REORDER);
  } else {
    DefWindowProc(message, key, MAKELPARAM(repeat_count, flags));
  }

  // CRichEditCtrl automatically turns on IMF_AUTOKEYBOARD when the user
  // inputs an RTL character, making it difficult for the user to control
  // what language is set as they type. Force this off to make the edit's
  // behavior more stable.
  const int lang_options = SendMessage(EM_GETLANGOPTIONS, 0, 0);
  if (lang_options & IMF_AUTOKEYBOARD)
    SendMessage(EM_SETLANGOPTIONS, 0, lang_options & ~IMF_AUTOKEYBOARD);

  OnAfterPossibleChange();
}

bool OmniboxViewWin::OnKeyDownOnlyWritable(TCHAR key,
                                           UINT repeat_count,
                                           UINT flags) {
  // NOTE: Annoyingly, ctrl-alt-<key> generates WM_KEYDOWN rather than
  // WM_SYSKEYDOWN, so we need to check (flags & KF_ALTDOWN) in various places
  // in this function even with a WM_SYSKEYDOWN handler.

  // If adding a new key that could possibly be an accelerator then you'll need
  // to update LocationBarView::SkipDefaultKeyEventProcessing() as well.
  int count = repeat_count;
  switch (key) {
    case VK_RIGHT:
      // TODO(sky): figure out RTL.
      if (base::i18n::IsRTL())
        return false;
      {
        CHARRANGE selection;
        GetSel(selection);
        return (selection.cpMin == selection.cpMax) &&
            (selection.cpMin == GetTextLength()) &&
            model_->CommitSuggestedText(true);
      }

    case VK_RETURN:
      model_->AcceptInput((flags & KF_ALTDOWN) ?
          NEW_FOREGROUND_TAB : CURRENT_TAB, false);
      return true;

    case VK_PRIOR:
    case VK_NEXT:
      count = model_->result().size();
      // FALL THROUGH
    case VK_UP:
    case VK_DOWN:
      // Ignore alt + numpad, but treat alt + (non-numpad) like (non-numpad).
      if ((flags & KF_ALTDOWN) && !(flags & KF_EXTENDED))
        return false;

      model_->OnUpOrDownKeyPressed(((key == VK_PRIOR) || (key == VK_UP)) ?
          -count : count);
      return true;

    // Hijacking Editing Commands
    //
    // We hijack the keyboard short-cuts for Cut, Copy, and Paste here so that
    // they go through our clipboard routines.  This allows us to be smarter
    // about how we interact with the clipboard and avoid bugs in the
    // CRichEditCtrl.  If we didn't hijack here, the edit control would handle
    // these internally with sending the WM_CUT, WM_COPY, or WM_PASTE messages.
    //
    // Cut:   Shift-Delete and Ctrl-x are treated as cut.  Ctrl-Shift-Delete and
    //        Ctrl-Shift-x are not treated as cut even though the underlying
    //        CRichTextEdit would treat them as such.  Also note that we bring
    //        up 'clear browsing data' on control-shift-delete.
    // Copy:  Ctrl-Insert and Ctrl-c is treated as copy.  Shift-Ctrl-c is not.
    //        (This is handled in OnKeyDownAllModes().)
    // Paste: Shift-Insert and Ctrl-v are treated as paste.  Ctrl-Shift-Insert
    //        and Ctrl-Shift-v are not.
    //
    // This behavior matches most, but not all Windows programs, and largely
    // conforms to what users expect.

    case VK_DELETE:
      if (flags & KF_ALTDOWN)
        return false;
      if (GetKeyState(VK_SHIFT) >= 0) {
        if (GetKeyState(VK_CONTROL) >= 0) {
          CHARRANGE selection;
          GetSel(selection);
          delete_at_end_pressed_ = ((selection.cpMin == selection.cpMax) &&
                                    (selection.cpMin == GetTextLength()));
        }
        return false;
      }
      if (GetKeyState(VK_CONTROL) >= 0) {
        // Cut text if possible.
        CHARRANGE selection;
        GetSel(selection);
        if (selection.cpMin != selection.cpMax) {
          ScopedFreeze freeze(this, GetTextObjectModel());
          OnBeforePossibleChange();
          Cut();
          OnAfterPossibleChange();
        } else {
          if (model_->popup_model()->IsOpen()) {
            // This is a bit overloaded, but we hijack Shift-Delete in this
            // case to delete the current item from the pop-up.  We prefer
            // cutting to this when possible since that's the behavior more
            // people expect from Shift-Delete, and it's more commonly useful.
            model_->popup_model()->TryDeletingCurrentItem();
          }
        }
      }
      return true;

    case 'X':
      if ((flags & KF_ALTDOWN) || (GetKeyState(VK_CONTROL) >= 0))
        return false;
      if (GetKeyState(VK_SHIFT) >= 0) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Cut();
        OnAfterPossibleChange();
      }
      return true;

    case VK_INSERT:
      // Ignore insert by itself, so we don't turn overtype mode on/off.
      if (!(flags & KF_ALTDOWN) && (GetKeyState(VK_SHIFT) >= 0) &&
          (GetKeyState(VK_CONTROL) >= 0))
        return true;
      // FALL THROUGH
    case 'V':
      if ((flags & KF_ALTDOWN) ||
          (GetKeyState((key == 'V') ? VK_CONTROL : VK_SHIFT) >= 0))
        return false;
      if (GetKeyState((key == 'V') ? VK_SHIFT : VK_CONTROL) >= 0) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Paste();
        OnAfterPossibleChange();
      }
      return true;

    case VK_BACK: {
      if ((flags & KF_ALTDOWN) || model_->is_keyword_hint() ||
          model_->keyword().empty())
        return false;

      {
        CHARRANGE selection;
        GetSel(selection);
        if ((selection.cpMin != selection.cpMax) || (selection.cpMin != 0))
          return false;
      }

      // We're showing a keyword and the user pressed backspace at the beginning
      // of the text. Delete the selected keyword.
      ScopedFreeze freeze(this, GetTextObjectModel());
      model_->ClearKeyword(GetText());
      return true;
    }

    case VK_TAB: {
      const bool shift_pressed = GetKeyState(VK_SHIFT) < 0;
      if (model_->is_keyword_hint() && !shift_pressed) {
        // Accept the keyword.
        ScopedFreeze freeze(this, GetTextObjectModel());
        model_->AcceptKeyword();
      } else if (shift_pressed &&
                 model_->popup_model()->selected_line_state() ==
                    AutocompletePopupModel::KEYWORD) {
        model_->ClearKeyword(GetText());
      } else {
        model_->OnUpOrDownKeyPressed(shift_pressed ? -count : count);
      }
      return true;
    }

    case 0xbb:  // Ctrl-'='.  Triggers subscripting (even in plain text mode).
                // We don't use VK_OEM_PLUS in case the macro isn't defined.
                // (e.g., we don't have this symbol in embeded environment).
      return true;

    default:
      return false;
  }
}

bool OmniboxViewWin::OnKeyDownAllModes(TCHAR key,
                                       UINT repeat_count,
                                       UINT flags) {
  // See KF_ALTDOWN comment atop OnKeyDownOnlyWritable().

  switch (key) {
    case VK_CONTROL:
      model_->OnControlKeyChanged(true);
      return false;

    case VK_INSERT:
    case 'C':
      // See more detailed comments in OnKeyDownOnlyWritable().
      if ((flags & KF_ALTDOWN) || (GetKeyState(VK_CONTROL) >= 0))
        return false;
      if (GetKeyState(VK_SHIFT) >= 0)
        Copy();
      return true;

    default:
      return false;
  }
}

void OmniboxViewWin::GetSelection(CHARRANGE& sel) const {
  GetSel(sel);

  // See if we need to reverse the direction of the selection.
  ITextDocument* const text_object_model = GetTextObjectModel();
  if (!text_object_model)
    return;
  base::win::ScopedComPtr<ITextSelection> selection;
  const HRESULT hr = text_object_model->GetSelection(selection.Receive());
  DCHECK_EQ(S_OK, hr);
  long flags;
  selection->GetFlags(&flags);
  if (flags & tomSelStartActive)
    std::swap(sel.cpMin, sel.cpMax);
}

string16 OmniboxViewWin::GetSelectedText() const {
  CHARRANGE sel;
  GetSel(sel);
  string16 str;
  if (sel.cpMin != sel.cpMax)
    GetSelText(WriteInto(&str, sel.cpMax - sel.cpMin + 1));
  return str;
}

void OmniboxViewWin::SetSelection(LONG start, LONG end) {
  SetSel(start, end);

  if (start <= end)
    return;

  // We need to reverse the direction of the selection.
  ITextDocument* const text_object_model = GetTextObjectModel();
  if (!text_object_model)
    return;
  base::win::ScopedComPtr<ITextSelection> selection;
  const HRESULT hr = text_object_model->GetSelection(selection.Receive());
  DCHECK_EQ(S_OK, hr);
  selection->SetFlags(tomSelStartActive);
}

void OmniboxViewWin::PlaceCaretAt(string16::size_type pos) {
  SetSelection(static_cast<LONG>(pos), static_cast<LONG>(pos));
}

bool OmniboxViewWin::IsSelectAllForRange(const CHARRANGE& sel) const {
  const int text_length = GetTextLength();
  return ((sel.cpMin == 0) && (sel.cpMax >= text_length)) ||
      ((sel.cpMax == 0) && (sel.cpMin >= text_length));
}

LONG OmniboxViewWin::ClipXCoordToVisibleText(LONG x,
                                             bool is_triple_click) const {
  // Clip the X coordinate to the left edge of the text. Careful:
  // PosFromChar(0) may return a negative X coordinate if the beginning of the
  // text has scrolled off the edit, so don't go past the clip rect's edge.
  PARAFORMAT2 pf2;
  GetParaFormat(pf2);
  // Calculation of the clipped coordinate is more complicated if the paragraph
  // layout is RTL layout, or if there is RTL characters inside the LTR layout
  // paragraph.
  const bool ltr_text_in_ltr_layout = !(pf2.wEffects & PFE_RTLPARA) &&
      !base::i18n::StringContainsStrongRTLChars(GetText());
  const int length = GetTextLength();
  RECT r;
  GetRect(&r);
  // The values returned by PosFromChar() seem to refer always
  // to the left edge of the character's bounding box.
  const LONG first_position_x = PosFromChar(0).x;
  LONG min_x = first_position_x;
  if (!ltr_text_in_ltr_layout) {
    for (int i = 1; i < length; ++i)
      min_x = std::min(min_x, PosFromChar(i).x);
  }
  const LONG left_bound = std::max(r.left, min_x);
  // PosFromChar(length) is a phantom character past the end of the text. It is
  // not necessarily a right bound; in RTL controls it may be a left bound. So
  // treat it as a right bound only if it is to the right of the first
  // character.
  LONG right_bound = r.right;
  LONG end_position_x = PosFromChar(length).x;
  if (end_position_x >= first_position_x) {
    right_bound = std::min(right_bound, end_position_x);  // LTR case.
  }
  // For trailing characters that are 2 pixels wide or less (like "l" in some
  // fonts), we have a problem:
  //   * Clicks on any pixel within the character will place the cursor before
  //     the character.
  //   * Clicks on the pixel just after the character will not allow triple-
  //     click to work properly (true for any last character width).
  // So, we move to the last pixel of the character when this is a
  // triple-click, and moving to one past the last pixel in all other
  // scenarios.  This way, all clicks that can move the cursor will place it at
  // the end of the text, but triple-click will still work.
  if (x < left_bound) {
    return (is_triple_click && ltr_text_in_ltr_layout) ? left_bound - 1 :
                                                         left_bound;
  }
  if ((length == 0) || (x < right_bound))
    return x;
  return is_triple_click ? (right_bound - 1) : right_bound;
}

void OmniboxViewWin::EmphasizeURLComponents() {
  ITextDocument* const text_object_model = GetTextObjectModel();
  ScopedFreeze freeze(this, text_object_model);
  ScopedSuspendUndo suspend_undo(text_object_model);

  // Save the selection.
  CHARRANGE saved_sel;
  GetSelection(saved_sel);

  // See whether the contents are a URL with a non-empty host portion, which we
  // should emphasize.  To check for a URL, rather than using the type returned
  // by Parse(), ask the model, which will check the desired page transition for
  // this input.  This can tell us whether an UNKNOWN input string is going to
  // be treated as a search or a navigation, and is the same method the Paste
  // And Go system uses.
  url_parse::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      GetText(), model_->GetDesiredTLD(), &scheme, &host);
  const bool emphasize = model_->CurrentTextIsURL() && (host.len > 0);

  // Set the baseline emphasis.
  CHARFORMAT cf = {0};
  cf.dwMask = CFM_COLOR;
  // If we're going to emphasize parts of the text, then the baseline state
  // should be "de-emphasized".  If not, then everything should be rendered in
  // the standard text color.
  cf.crTextColor = skia::SkColorToCOLORREF(LocationBarView::GetColor(
      security_level_,
      emphasize ? LocationBarView::DEEMPHASIZED_TEXT : LocationBarView::TEXT));
  // NOTE: Don't use SetDefaultCharFormat() instead of the below; that sets the
  // format that will get applied to text added in the future, not to text
  // already in the edit.
  SelectAll(false);
  SetSelectionCharFormat(cf);

  if (emphasize) {
    // We've found a host name, give it more emphasis.
    cf.crTextColor = skia::SkColorToCOLORREF(LocationBarView::GetColor(
        security_level_, LocationBarView::TEXT));
    SetSelection(host.begin, host.end());
    SetSelectionCharFormat(cf);
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  insecure_scheme_component_.reset();
  if (!model_->user_input_in_progress() && scheme.is_nonempty() &&
      (security_level_ != ToolbarModel::NONE)) {
    if (security_level_ == ToolbarModel::SECURITY_ERROR) {
      insecure_scheme_component_.begin = scheme.begin;
      insecure_scheme_component_.len = scheme.len;
    }
    cf.crTextColor = skia::SkColorToCOLORREF(LocationBarView::GetColor(
        security_level_, LocationBarView::SECURITY_TEXT));
    SetSelection(scheme.begin, scheme.end());
    SetSelectionCharFormat(cf);
  }

  // Restore the selection.
  SetSelectionRange(saved_sel);
}

void OmniboxViewWin::EraseTopOfSelection(CDC* dc,
                                         const CRect& client_rect,
                                         const CRect& paint_clip_rect) {
  // Find the area we care about painting.   We could calculate the rect
  // containing just the selected portion, but there's no harm in simply erasing
  // the whole top of the client area, and at least once I saw us manage to
  // select the "phantom newline" briefly, which looks very weird if not clipped
  // off at the same height.
  CRect erase_rect(client_rect.left, client_rect.top, client_rect.right,
                   client_rect.top + font_y_adjustment_);
  erase_rect.IntersectRect(erase_rect, paint_clip_rect);

  // Erase to the background color.
  if (!erase_rect.IsRectNull())
    dc->FillSolidRect(&erase_rect, background_color_);
}

void OmniboxViewWin::DrawSlashForInsecureScheme(HDC hdc,
                                                const CRect& client_rect,
                                                const CRect& paint_clip_rect) {
  DCHECK(insecure_scheme_component_.is_nonempty());

  // Calculate the rect, in window coordinates, containing the portion of the
  // scheme where we'll be drawing the slash.  Vertically, we draw across one
  // x-height of text, plus an additional 3 stroke diameters (the stroke width
  // plus a half-stroke width of space between the stroke and the text, both
  // above and below the text).
  const int font_top = client_rect.top + font_y_adjustment_;
  const SkScalar kStrokeWidthPixels = SkIntToScalar(2);
  const int kAdditionalSpaceOutsideFont =
      static_cast<int>(ceil(kStrokeWidthPixels * 1.5f));
  const CRect scheme_rect(PosFromChar(insecure_scheme_component_.begin).x,
                          font_top + font_.GetBaseline() - font_x_height_ -
                              kAdditionalSpaceOutsideFont,
                          PosFromChar(insecure_scheme_component_.end()).x,
                          font_top + font_.GetBaseline() +
                              kAdditionalSpaceOutsideFont);

  // Clip to the portion we care about and translate to canvas coordinates
  // (see the canvas creation below) for use later.
  CRect canvas_clip_rect, canvas_paint_clip_rect;
  canvas_clip_rect.IntersectRect(scheme_rect, client_rect);
  canvas_paint_clip_rect.IntersectRect(canvas_clip_rect, paint_clip_rect);
  if (canvas_paint_clip_rect.IsRectNull())
    return;  // We don't need to paint any of this region, so just bail early.
  canvas_clip_rect.OffsetRect(-scheme_rect.left, -scheme_rect.top);
  canvas_paint_clip_rect.OffsetRect(-scheme_rect.left, -scheme_rect.top);

  // Create a paint context for drawing the antialiased stroke.
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStrokeWidth(kStrokeWidthPixels);
  paint.setStrokeCap(SkPaint::kRound_Cap);

  // Create a canvas as large as |scheme_rect| to do our drawing, and initialize
  // it to fully transparent so any antialiasing will look nice when painted
  // atop the edit.
  gfx::Canvas canvas(gfx::Size(scheme_rect.Width(), scheme_rect.Height()),
                     false);
  SkCanvas* sk_canvas = canvas.sk_canvas();
  sk_canvas->getDevice()->accessBitmap(true).eraseARGB(0, 0, 0, 0);

  // Calculate the start and end of the stroke, which are just the lower left
  // and upper right corners of the canvas, inset by the radius of the endcap
  // so we don't clip the endcap off.
  const SkScalar kEndCapRadiusPixels = kStrokeWidthPixels / SkIntToScalar(2);
  const SkPoint start_point = {
      kEndCapRadiusPixels,
      SkIntToScalar(scheme_rect.Height()) - kEndCapRadiusPixels };
  const SkPoint end_point = {
      SkIntToScalar(scheme_rect.Width()) - kEndCapRadiusPixels,
      kEndCapRadiusPixels };

  // Calculate the selection rectangle in canvas coordinates, which we'll use
  // to clip the stroke so we can draw the unselected and selected portions.
  CHARRANGE sel;
  GetSel(sel);
  const SkRect selection_rect = {
      SkIntToScalar(PosFromChar(sel.cpMin).x - scheme_rect.left),
      SkIntToScalar(0),
      SkIntToScalar(PosFromChar(sel.cpMax).x - scheme_rect.left),
      SkIntToScalar(scheme_rect.Height()) };

  // Draw the unselected portion of the stroke.
  sk_canvas->save();
  if (selection_rect.isEmpty() ||
      sk_canvas->clipRect(selection_rect, SkRegion::kDifference_Op)) {
    paint.setColor(LocationBarView::GetColor(security_level_,
                                             LocationBarView::SECURITY_TEXT));
    sk_canvas->drawLine(start_point.fX, start_point.fY,
                        end_point.fX, end_point.fY, paint);
  }
  sk_canvas->restore();

  // Draw the selected portion of the stroke.
  if (!selection_rect.isEmpty() && sk_canvas->clipRect(selection_rect)) {
    paint.setColor(LocationBarView::GetColor(security_level_,
                                             LocationBarView::SELECTED_TEXT));
    sk_canvas->drawLine(start_point.fX, start_point.fY,
                        end_point.fX, end_point.fY, paint);
  }

  // Now copy what we drew to the target HDC.
  skia::DrawToNativeContext(sk_canvas, hdc,
      scheme_rect.left + canvas_paint_clip_rect.left - canvas_clip_rect.left,
      std::max(scheme_rect.top, client_rect.top) + canvas_paint_clip_rect.top -
          canvas_clip_rect.top, &canvas_paint_clip_rect);
}

void OmniboxViewWin::DrawDropHighlight(HDC hdc,
                                       const CRect& client_rect,
                                       const CRect& paint_clip_rect) {
  DCHECK_NE(-1, drop_highlight_position_);

  const int highlight_y = client_rect.top + font_y_adjustment_;
  const int highlight_x = PosFromChar(drop_highlight_position_).x - 1;
  const CRect highlight_rect(highlight_x,
                             highlight_y,
                             highlight_x + 1,
                             highlight_y + font_.GetHeight());

  // Clip the highlight to the region being painted.
  CRect clip_rect;
  clip_rect.IntersectRect(highlight_rect, paint_clip_rect);
  if (clip_rect.IsRectNull())
    return;

  HGDIOBJ last_pen = SelectObject(hdc, CreatePen(PS_SOLID, 1, RGB(0, 0, 0)));
  MoveToEx(hdc, clip_rect.left, clip_rect.top, NULL);
  LineTo(hdc, clip_rect.left, clip_rect.bottom);
  DeleteObject(SelectObject(hdc, last_pen));
}

void OmniboxViewWin::TextChanged() {
  ScopedFreeze freeze(this, GetTextObjectModel());
  EmphasizeURLComponents();
  model_->OnChanged();
}

string16 OmniboxViewWin::GetClipboardText() const {
  // Try text format.
  ui::Clipboard* clipboard = g_browser_process->clipboard();
  if (clipboard->IsFormatAvailable(ui::Clipboard::GetPlainTextWFormatType(),
                                   ui::Clipboard::BUFFER_STANDARD)) {
    string16 text;
    clipboard->ReadText(ui::Clipboard::BUFFER_STANDARD, &text);
    // Note: Unlike in the find popup and textfield view, here we completely
    // remove whitespace strings containing newlines.  We assume users are
    // most likely pasting in URLs that may have been split into multiple
    // lines in terminals, email programs, etc., and so linebreaks indicate
    // completely bogus whitespace that would just cause the input to be
    // invalid.
    return StripJavascriptSchemas(CollapseWhitespace(text, true));
  }

  // Try bookmark format.
  //
  // It is tempting to try bookmark format first, but the URL we get out of a
  // bookmark has been cannonicalized via GURL.  This means if a user copies
  // and pastes from the URL bar to itself, the text will get fixed up and
  // cannonicalized, which is not what the user expects.  By pasting in this
  // order, we are sure to paste what the user copied.
  if (clipboard->IsFormatAvailable(ui::Clipboard::GetUrlWFormatType(),
                                   ui::Clipboard::BUFFER_STANDARD)) {
    std::string url_str;
    clipboard->ReadBookmark(NULL, &url_str);
    // pass resulting url string through GURL to normalize
    GURL url(url_str);
    if (url.is_valid())
      return StripJavascriptSchemas(UTF8ToUTF16(url.spec()));
  }

  return string16();
}

bool OmniboxViewWin::CanPasteAndGo(const string16& text) const {
  return !popup_window_mode_ && model_->CanPasteAndGo(text);
}

ITextDocument* OmniboxViewWin::GetTextObjectModel() const {
  if (!text_object_model_) {
    // This is lazily initialized, instead of being initialized in the
    // constructor, in order to avoid hurting startup performance.
    base::win::ScopedComPtr<IRichEditOle, NULL> ole_interface;
    ole_interface.Attach(GetOleInterface());
    if (ole_interface) {
      ole_interface.QueryInterface(
          __uuidof(ITextDocument),
          reinterpret_cast<void**>(&text_object_model_));
    }
  }
  return text_object_model_;
}

void OmniboxViewWin::StartDragIfNecessary(const CPoint& point) {
  if (initiated_drag_ || !IsDrag(click_point_[kLeft], point))
    return;

  ui::OSExchangeData data;

  DWORD supported_modes = DROPEFFECT_COPY;

  CHARRANGE sel;
  GetSelection(sel);

  // We're about to start a drag session, but the edit is expecting a mouse up
  // that it uses to reset internal state.  If we don't send a mouse up now,
  // when the mouse moves back into the edit the edit will reset the selection.
  // So, we send the event now which resets the selection.  We then restore the
  // selection and start the drag.  We always send lbuttonup as otherwise we
  // might trigger a context menu (right up).  This seems scary, but doesn't
  // seem to cause problems.
  {
    ScopedFreeze freeze(this, GetTextObjectModel());
    DefWindowProc(WM_LBUTTONUP, 0,
                  MAKELPARAM(click_point_[kLeft].x, click_point_[kLeft].y));
    SetSelectionRange(sel);
  }

  const string16 start_text(GetText());
  string16 text_to_write(GetSelectedText());
  GURL url;
  bool write_url;
  const bool is_all_selected = IsSelectAllForRange(sel);

  // |sel| was set by GetSelection(), which preserves selection direction, so
  // sel.cpMin may not be the smaller value.
  model()->AdjustTextForCopy(std::min(sel.cpMin, sel.cpMax), is_all_selected,
                             &text_to_write, &url, &write_url);

  if (write_url) {
    string16 title;
    SkBitmap favicon;
    if (is_all_selected)
      model_->GetDataForURLExport(&url, &title, &favicon);
    button_drag_utils::SetURLAndDragImage(url, title, favicon, &data);
    supported_modes |= DROPEFFECT_LINK;
    content::RecordAction(UserMetricsAction("Omnibox_DragURL"));
  } else {
    supported_modes |= DROPEFFECT_MOVE;
    content::RecordAction(UserMetricsAction("Omnibox_DragString"));
  }

  data.SetString(text_to_write);

  scoped_refptr<ui::DragSource> drag_source(new ui::DragSource);
  DWORD dropped_mode;
  AutoReset<bool> auto_reset_in_drag(&in_drag_, true);
  if (DoDragDrop(ui::OSExchangeDataProviderWin::GetIDataObject(data),
                 drag_source, supported_modes, &dropped_mode) ==
          DRAGDROP_S_DROP) {
    if ((dropped_mode == DROPEFFECT_MOVE) && (start_text == GetText())) {
      ScopedFreeze freeze(this, GetTextObjectModel());
      OnBeforePossibleChange();
      SetSelectionRange(sel);
      ReplaceSel(L"", true);
      OnAfterPossibleChange();
    }
    // else case, not a move or it was a move and the drop was on us.
    // If the drop was on us, EditDropTarget took care of the move so that
    // we don't have to delete the text.
    possible_drag_ = false;
  } else {
    // Drag was canceled or failed. The mouse may still be down and
    // over us, in which case we need possible_drag_ to remain true so
    // that we don't forward mouse move events to the edit which will
    // start another drag.
    //
    // NOTE: we didn't use mouse capture during the mouse down as DoDragDrop
    // does its own capture.
    CPoint cursor_location;
    GetCursorPos(&cursor_location);

    CRect client_rect;
    GetClientRect(&client_rect);

    CPoint client_origin_on_screen(client_rect.left, client_rect.top);
    ClientToScreen(&client_origin_on_screen);
    client_rect.MoveToXY(client_origin_on_screen.x,
                         client_origin_on_screen.y);
    possible_drag_ = (client_rect.PtInRect(cursor_location) &&
                      ((GetKeyState(VK_LBUTTON) != 0) ||
                       (GetKeyState(VK_MBUTTON) != 0) ||
                       (GetKeyState(VK_RBUTTON) != 0)));
  }

  initiated_drag_ = true;
  tracking_click_[kLeft] = false;
}

void OmniboxViewWin::OnPossibleDrag(const CPoint& point) {
  if (possible_drag_)
    return;

  click_point_[kLeft] = point;
  initiated_drag_ = false;

  CHARRANGE selection;
  GetSel(selection);
  if (selection.cpMin != selection.cpMax) {
    const POINT min_sel_location(PosFromChar(selection.cpMin));
    const POINT max_sel_location(PosFromChar(selection.cpMax));
    // NOTE: we don't consider the y location here as we always pass a
    // y-coordinate in the middle to the default handler which always triggers
    // a drag regardless of the y-coordinate.
    possible_drag_ = (point.x >= min_sel_location.x) &&
                     (point.x < max_sel_location.x);
  }
}

void OmniboxViewWin::RepaintDropHighlight(int position) {
  if ((position != -1) && (position <= GetTextLength())) {
    const POINT min_loc(PosFromChar(position));
    const RECT highlight_bounds = {min_loc.x - 1, font_y_adjustment_,
        min_loc.x + 2, font_.GetHeight() + font_y_adjustment_};
    InvalidateRect(&highlight_bounds, false);
  }
}

void OmniboxViewWin::BuildContextMenu() {
  if (context_menu_contents_.get())
    return;

  context_menu_contents_.reset(new ui::SimpleMenuModel(this));
  // Set up context menu.
  if (popup_window_mode_) {
    context_menu_contents_->AddItemWithStringId(IDC_COPY, IDS_COPY);
  } else {
    context_menu_contents_->AddItemWithStringId(IDS_UNDO, IDS_UNDO);
    context_menu_contents_->AddSeparator();
    context_menu_contents_->AddItemWithStringId(IDC_CUT, IDS_CUT);
    context_menu_contents_->AddItemWithStringId(IDC_COPY, IDS_COPY);
    context_menu_contents_->AddItemWithStringId(IDC_PASTE, IDS_PASTE);
    // GetContextualLabel() will override this next label with the
    // IDS_PASTE_AND_SEARCH label as needed.
    context_menu_contents_->AddItemWithStringId(IDS_PASTE_AND_GO,
                                                IDS_PASTE_AND_GO);
    context_menu_contents_->AddSeparator();
    context_menu_contents_->AddItemWithStringId(IDS_SELECT_ALL, IDS_SELECT_ALL);
    context_menu_contents_->AddSeparator();
    context_menu_contents_->AddItemWithStringId(IDS_EDIT_SEARCH_ENGINES,
                                                IDS_EDIT_SEARCH_ENGINES);
  }
  context_menu_.reset(new views::Menu2(context_menu_contents_.get()));
}

void OmniboxViewWin::SelectAllIfNecessary(MouseButton button,
                                          const CPoint& point) {
  // When the user has clicked and released to give us focus, select all.
  if (tracking_click_[button] &&
      !IsDrag(click_point_[button], point)) {
    // Select all in the reverse direction so as not to scroll the caret
    // into view and shift the contents jarringly.
    SelectAll(true);
    possible_drag_ = false;
  }
}

void OmniboxViewWin::TrackMousePosition(MouseButton button,
                                        const CPoint& point) {
  if (gaining_focus_.get()) {
    // This click is giving us focus, so we need to track how much the mouse
    // moves to see if it's a drag or just a click. Clicks should select all
    // the text.
    tracking_click_[button] = true;
    click_point_[button] = point;
  }
}

int OmniboxViewWin::GetHorizontalMargin() const {
  RECT rect;
  GetRect(&rect);
  RECT client_rect;
  GetClientRect(&client_rect);
  return (rect.left - client_rect.left) + (client_rect.right - rect.right);
}

int OmniboxViewWin::WidthNeededToDisplay(const string16& text) const {
  // Use font_.GetStringWidth() instead of
  // PosFromChar(location_entry_->GetTextLength()) because PosFromChar() is
  // apparently buggy. In both LTR UI and RTL UI with left-to-right layout,
  // PosFromChar(i) might return 0 when i is greater than 1.
  return font_.GetStringWidth(text) + GetHorizontalMargin();
}

bool OmniboxViewWin::IsCaretAtEnd() const {
  long length = GetTextLength();
  CHARRANGE sel;
  GetSelection(sel);
  return sel.cpMin == sel.cpMax && sel.cpMin == length;
}

#if !defined(USE_AURA)
// static
OmniboxView* OmniboxView::CreateOmniboxView(
    AutocompleteEditController* controller,
    ToolbarModel* toolbar_model,
    Profile* profile,
    CommandUpdater* command_updater,
    bool popup_window_mode,
    LocationBarView* location_bar) {
  return new OmniboxViewWin(controller,
                            toolbar_model,
                            location_bar,
                            command_updater,
                            popup_window_mode,
                            location_bar);
}
#endif
