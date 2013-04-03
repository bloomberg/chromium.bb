// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/app_locale_settings.h"
#include "grit/generated_resources.h"
#include "grit/ui_strings.h"
#include "net/base/escape.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/events/event.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/selection_model.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/layer.h"
#endif

namespace {

// Stores omnibox state for each tab.
struct OmniboxState : public base::SupportsUserData::Data {
  static const char kKey[];

  OmniboxState(const OmniboxEditModel::State& model_state,
               const gfx::SelectionModel& selection_model);
  virtual ~OmniboxState();

  const OmniboxEditModel::State model_state;
  const gfx::SelectionModel selection_model;
};

// static
const char OmniboxState::kKey[] = "OmniboxState";

OmniboxState::OmniboxState(const OmniboxEditModel::State& model_state,
                           const gfx::SelectionModel& selection_model)
    : model_state(model_state),
      selection_model(selection_model) {
}

OmniboxState::~OmniboxState() {}

// The following const value is the same as in browser_defaults.
const int kAutocompleteEditFontPixelSize = 15;
// Font size 10px (as defined in browser_defaults) is too small for many
// non-Latin/Greek/Cyrillic (non-LGC) scripts. For pop-up window, the total
// rectangle is 21px tall and the height available for "ink" is 17px (please
// refer to kAutocompleteVerticalMarginInPopup). With 12px font size, the
// tallest glyphs in UI fonts we're building for ChromeOS (across all scripts)
// still fit within 17px "ink" height.
const int kAutocompleteEditFontPixelSizeInPopup = 12;

// The following 2 values are based on kAutocompleteEditFontPixelSize and
// kAutocompleteEditFontPixelSizeInPopup. They should be changed accordingly
// if font size for autocomplete edit (in popup) change.
const int kAutocompleteVerticalMargin = 1;
const int kAutocompleteVerticalMarginInPopup = 2;

int GetEditFontPixelSize(bool popup_window_mode) {
  return popup_window_mode ? kAutocompleteEditFontPixelSizeInPopup :
                             kAutocompleteEditFontPixelSize;
}

// This will write |url| and |text| to the clipboard as a well-formed URL.
void DoCopyURL(const GURL& url, const string16& text, Profile* profile) {
  BookmarkNodeData data;
  data.ReadFromTuple(url, text);
  data.WriteToClipboard(profile);
}

}  // namespace

// static
const char OmniboxViewViews::kViewClassName[] = "BrowserOmniboxViewViews";

OmniboxViewViews::OmniboxViewViews(OmniboxEditController* controller,
                                   ToolbarModel* toolbar_model,
                                   Profile* profile,
                                   CommandUpdater* command_updater,
                                   bool popup_window_mode,
                                   LocationBarView* location_bar)
    : OmniboxView(profile, controller, toolbar_model, command_updater),
      popup_window_mode_(popup_window_mode),
      security_level_(ToolbarModel::NONE),
      ime_composing_before_change_(false),
      delete_at_end_pressed_(false),
      location_bar_view_(location_bar),
      ime_candidate_window_open_(false),
      select_all_on_mouse_release_(false),
      select_all_on_gesture_tap_(false) {
  RemoveBorder();
  set_id(VIEW_ID_OMNIBOX);
}

OmniboxViewViews::~OmniboxViewViews() {
#if defined(OS_CHROMEOS)
  chromeos::input_method::GetInputMethodManager()->
      RemoveCandidateWindowObserver(this);
#endif

  // Explicitly teardown members which have a reference to us.  Just to be safe
  // we want them to be destroyed before destroying any other internal state.
  popup_view_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxViewViews public:

void OmniboxViewViews::Init() {
  // The height of the text view is going to change based on the font used.  We
  // don't want to stretch the height, and we want it vertically centered.
  // TODO(oshima): make sure the above happens with views.
  SetController(this);
  SetTextInputType(ui::TEXT_INPUT_TYPE_URL);
  SetBackgroundColor(location_bar_view_->GetColor(
      ToolbarModel::NONE, LocationBarView::BACKGROUND));

  if (popup_window_mode_)
    SetReadOnly(true);

  const int font_size = GetEditFontPixelSize(popup_window_mode_);
  if (font_size != font().GetFontSize())
    SetFont(font().DeriveFont(font_size - font().GetFontSize()));

  // Initialize the popup view using the same font.
  popup_view_.reset(OmniboxPopupContentsView::Create(
      font(), this, model(), location_bar_view_));

  // A null-border to zero out the focused border on textfield.
  const int vertical_margin = !popup_window_mode_ ?
      kAutocompleteVerticalMargin : kAutocompleteVerticalMarginInPopup;
  set_border(views::Border::CreateEmptyBorder(vertical_margin, 0,
                                              vertical_margin, 0));
#if defined(OS_CHROMEOS)
  chromeos::input_method::GetInputMethodManager()->
      AddCandidateWindowObserver(this);
#endif
}

gfx::Font OmniboxViewViews::GetFont() {
  return font();
}

int OmniboxViewViews::WidthOfTextAfterCursor() {
  // See comments in LocationBarView::Layout as to why this uses -1.
  const int start = std::max(0, static_cast<int>(GetSelectedRange().end()) - 1);
  // TODO: add horizontal margin.
  return font().GetStringWidth(text().substr(start));
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxViewViews, views::Textfield implementation:

std::string OmniboxViewViews::GetClassName() const {
  return kViewClassName;
}

void OmniboxViewViews::OnGestureEvent(ui::GestureEvent* event) {
  views::Textfield::OnGestureEvent(event);
  if (!HasFocus() && event->type() == ui::ET_GESTURE_TAP_DOWN) {
    select_all_on_gesture_tap_ = true;
    return;
  }
  if (select_all_on_gesture_tap_ && event->type() == ui::ET_GESTURE_TAP)
    SelectAll(false);
  select_all_on_gesture_tap_ = false;
}

void OmniboxViewViews::GetAccessibleState(ui::AccessibleViewState* state) {
  location_bar_view_->GetAccessibleState(state);
}

void OmniboxViewViews::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (popup_view_->IsOpen())
    popup_view_->UpdatePopupAppearance();
}

bool OmniboxViewViews::OnMousePressed(const ui::MouseEvent& event) {
  select_all_on_mouse_release_ =
      (event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      (!HasFocus() || (model()->focus_state() == OMNIBOX_FOCUS_INVISIBLE));
  // Restore caret visibility whenever the user clicks in the omnibox in a way
  // that would give it focus.  We must handle this case separately here because
  // if the omnibox currently has invisible focus, the mouse event won't trigger
  // either SetFocus() or OmniboxEditModel::OnSetFocus().
  if (select_all_on_mouse_release_)
    model()->SetCaretVisibility(true);
  return views::Textfield::OnMousePressed(event);
}

bool OmniboxViewViews::OnMouseDragged(const ui::MouseEvent& event) {
  select_all_on_mouse_release_ = false;
  return views::Textfield::OnMouseDragged(event);
}

void OmniboxViewViews::OnMouseReleased(const ui::MouseEvent& event) {
  views::Textfield::OnMouseReleased(event);
  if ((event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      select_all_on_mouse_release_) {
    // Select all in the reverse direction so as not to scroll the caret
    // into view and shift the contents jarringly.
    SelectAll(true);
  }
  select_all_on_mouse_release_ = false;
}

bool OmniboxViewViews::OnKeyPressed(const ui::KeyEvent& event) {
  // Skip processing of [Alt]+<num-pad digit> Unicode alt key codes.
  // Otherwise, if num-lock is off, the events are handled as [Up], [Down], etc.
  if (event.IsUnicodeKeyCode())
    return views::Textfield::OnKeyPressed(event);

  switch (event.key_code()) {
    case ui::VKEY_RETURN:
      model()->AcceptInput(event.IsAltDown() ? NEW_FOREGROUND_TAB : CURRENT_TAB,
                           false);
      return true;
    case ui::VKEY_ESCAPE:
      return model()->OnEscapeKeyPressed();
    case ui::VKEY_CONTROL:
      model()->OnControlKeyChanged(true);
      break;
    case ui::VKEY_DELETE:
      if (event.IsShiftDown() && model()->popup_model()->IsOpen())
        model()->popup_model()->TryDeletingCurrentItem();
      break;
    case ui::VKEY_UP:
      model()->OnUpOrDownKeyPressed(-1);
      return true;
    case ui::VKEY_DOWN:
      model()->OnUpOrDownKeyPressed(1);
      return true;
    case ui::VKEY_PRIOR:
      model()->OnUpOrDownKeyPressed(-1 * model()->result().size());
      return true;
    case ui::VKEY_NEXT:
      model()->OnUpOrDownKeyPressed(model()->result().size());
      return true;
    case ui::VKEY_V:
      if (event.IsControlDown() && !read_only()) {
        OnBeforePossibleChange();
        OnPaste();
        OnAfterPossibleChange();
        return true;
      }
      break;
    default:
      break;
  }

  bool handled = views::Textfield::OnKeyPressed(event);
#if !defined(OS_WIN) || defined(USE_AURA)
  // TODO(msw): Avoid this complexity, consolidate cross-platform behavior.
  handled |= SkipDefaultKeyEventProcessing(event);
#endif
  return handled;
}

bool OmniboxViewViews::OnKeyReleased(const ui::KeyEvent& event) {
  // The omnibox contents may change while the control key is pressed.
  if (event.key_code() == ui::VKEY_CONTROL)
    model()->OnControlKeyChanged(false);
  return views::Textfield::OnKeyReleased(event);
}

bool OmniboxViewViews::SkipDefaultKeyEventProcessing(
    const ui::KeyEvent& event) {
  // Handle keyword hint tab-to-search and tabbing through dropdown results.
  // This must run before acclerator handling invokes a focus change on tab.
  if (views::FocusManager::IsTabTraversalKeyEvent(event)) {
    if (model()->is_keyword_hint() && !event.IsShiftDown()) {
      model()->AcceptKeyword();
      return true;
    }
    if (model()->popup_model()->IsOpen()) {
      if (event.IsShiftDown() &&
          model()->popup_model()->selected_line_state() ==
              OmniboxPopupModel::KEYWORD) {
        model()->ClearKeyword(text());
      } else {
        model()->OnUpOrDownKeyPressed(event.IsShiftDown() ? -1 : 1);
      }
      return true;
    }
  }

  return Textfield::SkipDefaultKeyEventProcessing(event);
}

void OmniboxViewViews::OnFocus() {
  views::Textfield::OnFocus();
  // TODO(oshima): Get control key state.
  model()->OnSetFocus(false);
  // Don't call controller()->OnSetFocus, this view has already acquired focus.

  // Restore a valid saved selection on tab-to-focus.
  if (saved_temporary_selection_.IsValid() && !select_all_on_mouse_release_)
    SelectRange(saved_temporary_selection_);
}

void OmniboxViewViews::OnBlur() {
  // Save the selection to restore on tab-to-focus.
  saved_temporary_selection_ = GetSelectedRange();

  views::Textfield::OnBlur();
  gfx::NativeView native_view = NULL;
#if defined(USE_AURA)
  views::Widget* widget = GetWidget();
  if (widget) {
    aura::client::FocusClient* client =
        aura::client::GetFocusClient(widget->GetNativeView());
    if (client)
      native_view = client->GetFocusedWindow();
  }
#endif
  model()->OnWillKillFocus(native_view);
  // Close the popup.
  CloseOmniboxPopup();
  // Tell the model to reset itself.
  model()->OnKillFocus();
  controller()->OnKillFocus();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxViewViews, OmniboxView implementation:

void OmniboxViewViews::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);

  // We don't want to keep the IME status, so force quit the current
  // session here.  It may affect the selection status, so order is
  // also important.
  if (IsIMEComposing()) {
    GetTextInputClient()->ConfirmCompositionText();
    GetInputMethod()->CancelComposition(this);
  }

  // NOTE: GetStateForTabSwitch may affect GetSelection, so order is important.
  OmniboxEditModel::State state = model()->GetStateForTabSwitch();
  const gfx::SelectionModel selection = GetSelectionModel();
  tab->SetUserData(OmniboxState::kKey, new OmniboxState(state, selection));
}

void OmniboxViewViews::Update(const content::WebContents* contents) {
  // NOTE: We're getting the URL text here from the ToolbarModel.
  bool visibly_changed_permanent_text =
      model()->UpdatePermanentText(toolbar_model()->GetText(true));
  ToolbarModel::SecurityLevel security_level =
        toolbar_model()->GetSecurityLevel();
  bool changed_security_level = (security_level != security_level_);
  security_level_ = security_level;

  // TODO(msw|oshima): Copied from GTK, determine correct Win/CrOS behavior.
  if (contents) {
    RevertAll();
    const OmniboxState* state = static_cast<OmniboxState*>(
        contents->GetUserData(&OmniboxState::kKey));
    if (state) {
      // Restore the saved state and selection.
      model()->RestoreState(state->model_state);
      SelectSelectionModel(state->selection_model);
      // TODO(oshima): Consider saving/restoring edit history.
      ClearEditHistory();
    }
  } else if (visibly_changed_permanent_text) {
    RevertAll();
  } else if (changed_security_level) {
    EmphasizeURLComponents();
  }
}

string16 OmniboxViewViews::GetText() const {
  // TODO(oshima): IME support
  return text();
}

void OmniboxViewViews::SetWindowTextAndCaretPos(const string16& text,
                                                size_t caret_pos,
                                                bool update_popup,
                                                bool notify_text_changed) {
  const ui::Range range(caret_pos, caret_pos);
  SetTextAndSelectedRange(text, range);

  if (update_popup)
    UpdatePopup();

  if (notify_text_changed)
    TextChanged();
}

void OmniboxViewViews::SetForcedQuery() {
  const string16 current_text(text());
  const size_t start = current_text.find_first_not_of(kWhitespaceUTF16);
  if (start == string16::npos || (current_text[start] != '?'))
    SetUserText(ASCIIToUTF16("?"));
  else
    SelectRange(ui::Range(current_text.size(), start + 1));
}

bool OmniboxViewViews::IsSelectAll() const {
  // TODO(oshima): IME support.
  return text() == GetSelectedText();
}

bool OmniboxViewViews::DeleteAtEndPressed() {
  return delete_at_end_pressed_;
}

void OmniboxViewViews::GetSelectionBounds(string16::size_type* start,
                                          string16::size_type* end) const {
  const ui::Range range = GetSelectedRange();
  *start = static_cast<size_t>(range.start());
  *end = static_cast<size_t>(range.end());
}

void OmniboxViewViews::SelectAll(bool reversed) {
  views::Textfield::SelectAll(reversed);
}

void OmniboxViewViews::UpdatePopup() {
  model()->SetInputInProgress(true);
  if (ime_candidate_window_open_)
    return;
  if (!model()->has_focus())
    return;

  // Prevent inline autocomplete when the caret isn't at the end of the text,
  // and during IME composition editing.
  const ui::Range sel = GetSelectedRange();
  model()->StartAutocomplete(
      !sel.is_empty(),
      sel.GetMax() < text().length() || IsIMEComposing());
}

void OmniboxViewViews::SetFocus() {
  RequestFocus();
  // Restore caret visibility if focus is explicitly requested. This is
  // necessary because if we already have invisible focus, the RequestFocus()
  // call above will short-circuit, preventing us from reaching
  // OmniboxEditModel::OnSetFocus(), which handles restoring visibility when the
  // omnibox regains focus after losing focus.
  model()->SetCaretVisibility(true);
}

void OmniboxViewViews::ApplyCaretVisibility() {
  SetCursorEnabled(model()->is_caret_visible());
}

void OmniboxViewViews::OnTemporaryTextMaybeChanged(
    const string16& display_text,
    bool save_original_selection,
    bool notify_text_changed) {
  if (save_original_selection)
    saved_temporary_selection_ = GetSelectedRange();

  SetWindowTextAndCaretPos(display_text, display_text.length(), false,
                           notify_text_changed);
}

bool OmniboxViewViews::OnInlineAutocompleteTextMaybeChanged(
    const string16& display_text,
    size_t user_text_length) {
  if (display_text == text())
    return false;
  ui::Range range(display_text.size(), user_text_length);
  SetTextAndSelectedRange(display_text, range);
  TextChanged();
  return true;
}

void OmniboxViewViews::OnRevertTemporaryText() {
  SelectRange(saved_temporary_selection_);
  // We got here because the user hit the Escape key. We explicitly don't call
  // TextChanged(), since calling it breaks Instant-Extended, and isn't needed
  // otherwise (in regular non-Instant or Instant-but-not-Extended modes).
  //
  // Why it breaks Instant-Extended: Instant handles the Escape key separately
  // (cf: OmniboxEditModel::RevertTemporaryText). Calling TextChanged() makes
  // the page think the user additionally typed some text, causing it to update
  // its suggestions dropdown with new suggestions, which is wrong.
  //
  // Why it isn't needed: OmniboxPopupModel::ResetToDefaultMatch() has already
  // been called by now; it would've called TextChanged() if it was warranted.
}

void OmniboxViewViews::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = text();
  sel_before_change_ = GetSelectedRange();
  ime_composing_before_change_ = IsIMEComposing();
}

bool OmniboxViewViews::OnAfterPossibleChange() {
  // See if the text or selection have changed since OnBeforePossibleChange().
  const string16 new_text = text();
  const ui::Range new_sel = GetSelectedRange();
  const bool text_changed = (new_text != text_before_change_) ||
      (ime_composing_before_change_ != IsIMEComposing());
  const bool selection_differs =
      !((sel_before_change_.is_empty() && new_sel.is_empty()) ||
        sel_before_change_.EqualsIgnoringDirection(new_sel));

  // When the user has deleted text, we don't allow inline autocomplete.  Make
  // sure to not flag cases like selecting part of the text and then pasting
  // (or typing) the prefix of that selection.  (We detect these by making
  // sure the caret, which should be after any insertion, hasn't moved
  // forward of the old selection start.)
  const bool just_deleted_text =
      (text_before_change_.length() > new_text.length()) &&
      (new_sel.start() <= sel_before_change_.GetMin());

  const bool something_changed = model()->OnAfterPossibleChange(
      text_before_change_, new_text, new_sel.start(), new_sel.end(),
      selection_differs, text_changed, just_deleted_text, !IsIMEComposing());

  // If only selection was changed, we don't need to call model()'s
  // OnChanged() method, which is called in TextChanged().
  // But we still need to call EmphasizeURLComponents() to make sure the text
  // attributes are updated correctly.
  if (something_changed && text_changed)
    TextChanged();
  else if (selection_differs)
    EmphasizeURLComponents();
  else if (delete_at_end_pressed_)
    model()->OnChanged();

  return something_changed;
}

gfx::NativeView OmniboxViewViews::GetNativeView() const {
  return GetWidget()->GetNativeView();
}

gfx::NativeView OmniboxViewViews::GetRelativeWindowForPopup() const {
  return GetWidget()->GetTopLevelWidget()->GetNativeView();
}

void OmniboxViewViews::SetInstantSuggestion(const string16& input) {
#if defined(OS_WIN) || defined(USE_AURA)
  location_bar_view_->SetInstantSuggestion(input);
#endif
}

string16 OmniboxViewViews::GetInstantSuggestion() const {
#if defined(OS_WIN) || defined(USE_AURA)
  return location_bar_view_->GetInstantSuggestion();
#else
  return string16();
#endif
}

int OmniboxViewViews::TextWidth() const {
  // TODO(oshima): add horizontal margin.
  return font().GetStringWidth(text());
}

bool OmniboxViewViews::IsImeComposing() const {
  return false;
}

int OmniboxViewViews::GetMaxEditWidth(int entry_width) const {
  return entry_width;
}

views::View* OmniboxViewViews::AddToView(views::View* parent) {
  parent->AddChildView(this);
  return this;
}

int OmniboxViewViews::OnPerformDrop(const ui::DropTargetEvent& event) {
  NOTIMPLEMENTED();
  return ui::DragDropTypes::DRAG_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxViewViews, views::TextfieldController implementation:

void OmniboxViewViews::ContentsChanged(views::Textfield* sender,
                                       const string16& new_contents) {
}

bool OmniboxViewViews::HandleKeyEvent(views::Textfield* textfield,
                                      const ui::KeyEvent& event) {
  delete_at_end_pressed_ = false;

  if (event.key_code() == ui::VKEY_BACK) {
    // No extra handling is needed in keyword search mode, if there is a
    // non-empty selection, or if the cursor is not leading the text.
    if (model()->is_keyword_hint() || model()->keyword().empty() ||
        HasSelection() || GetCursorPosition() != 0)
      return false;
    model()->ClearKeyword(text());
    return true;
  }

  if (event.key_code() == ui::VKEY_DELETE && !event.IsAltDown()) {
    delete_at_end_pressed_ =
        (!HasSelection() && GetCursorPosition() == text().length());
  }

  // Handle the right-arrow key for LTR text and the left-arrow key for RTL text
  // if there is an Instant suggestion (gray text) that needs to be committed.
  if (GetCursorPosition() == text().length()) {
    base::i18n::TextDirection direction = GetTextDirection();
    if ((direction == base::i18n::LEFT_TO_RIGHT &&
         event.key_code() == ui::VKEY_RIGHT) ||
        (direction == base::i18n::RIGHT_TO_LEFT &&
         event.key_code() == ui::VKEY_LEFT)) {
      return model()->CommitSuggestedText(true);
    }
  }

  return false;
}

void OmniboxViewViews::OnBeforeUserAction(views::Textfield* sender) {
  OnBeforePossibleChange();
}

void OmniboxViewViews::OnAfterUserAction(views::Textfield* sender) {
  OnAfterPossibleChange();
}

void OmniboxViewViews::OnAfterCutOrCopy() {
  ui::Clipboard* cb = ui::Clipboard::GetForCurrentThread();
  string16 selected_text;
  cb->ReadText(ui::Clipboard::BUFFER_STANDARD, &selected_text);
  GURL url;
  bool write_url;
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), IsSelectAll(),
                             &selected_text, &url, &write_url);
  if (write_url) {
    DoCopyURL(url, selected_text, model()->profile());
  } else {
    ui::ScopedClipboardWriter scoped_clipboard_writer(
        ui::Clipboard::GetForCurrentThread(),
        ui::Clipboard::BUFFER_STANDARD,
        content::BrowserContext::GetMarkerForOffTheRecordContext(
            model()->profile()));
    scoped_clipboard_writer.WriteText(selected_text);
  }
}

void OmniboxViewViews::OnGetDragOperationsForTextfield(int* drag_operations) {
  string16 selected_text = GetSelectedText();
  GURL url;
  bool write_url;
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), IsSelectAll(),
                             &selected_text, &url, &write_url);
  if (write_url)
    *drag_operations |= ui::DragDropTypes::DRAG_LINK;
}

void OmniboxViewViews::OnWriteDragData(ui::OSExchangeData* data) {
  string16 selected_text = GetSelectedText();
  GURL url;
  bool write_url;
  bool is_all_selected = IsSelectAll();
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), is_all_selected,
                             &selected_text, &url, &write_url);
  data->SetString(selected_text);
  if (write_url) {
    gfx::Image favicon;
    string16 title = selected_text;
    if (is_all_selected)
      model()->GetDataForURLExport(&url, &title, &favicon);
    button_drag_utils::SetURLAndDragImage(url, title, favicon.AsImageSkia(),
                                          data, GetWidget());
    data->SetURL(url, title);
  }
}

void OmniboxViewViews::AppendDropFormats(
    int* formats,
    std::set<ui::OSExchangeData::CustomFormat>* custom_formats) {
  *formats = *formats | ui::OSExchangeData::URL;
}

int OmniboxViewViews::OnDrop(const ui::OSExchangeData& data) {
  if (data.HasURL()) {
    GURL url;
    string16 title;
    if (data.GetURLAndTitle(&url, &title)) {
      string16 text(StripJavascriptSchemas(UTF8ToUTF16(url.spec())));
      if (model()->CanPasteAndGo(text)) {
        model()->PasteAndGo(text);
        return ui::DragDropTypes::DRAG_COPY;
      }
    }
  }
  return ui::DragDropTypes::DRAG_NONE;
}

void OmniboxViewViews::UpdateContextMenu(ui::SimpleMenuModel* menu_contents) {
  // Minor note: We use IDC_ for command id here while the underlying textfield
  // is using IDS_ for all its command ids. This is because views cannot depend
  // on IDC_ for now.
  menu_contents->AddItemWithStringId(IDC_EDIT_SEARCH_ENGINES,
      IDS_EDIT_SEARCH_ENGINES);

  if (chrome::IsQueryExtractionEnabled()) {
    int copy_position = menu_contents->GetIndexOfCommandId(IDS_APP_COPY);
    DCHECK(copy_position >= 0);
    menu_contents->InsertItemWithStringIdAt(
        copy_position + 1, IDC_COPY_URL, IDS_COPY_URL);
  }

  int paste_position = menu_contents->GetIndexOfCommandId(IDS_APP_PASTE);
  DCHECK(paste_position >= 0);
  menu_contents->InsertItemWithStringIdAt(
      paste_position + 1, IDS_PASTE_AND_GO, IDS_PASTE_AND_GO);
}

bool OmniboxViewViews::IsCommandIdEnabled(int command_id) const {
  if (command_id == IDS_PASTE_AND_GO)
    return model()->CanPasteAndGo(GetClipboardText());
  if (command_id == IDC_COPY_URL) {
    return toolbar_model()->WouldReplaceSearchURLWithSearchTerms() &&
      !model()->user_input_in_progress();
  }
  return command_updater()->IsCommandEnabled(command_id);
}

bool OmniboxViewViews::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDS_PASTE_AND_GO;
}

string16 OmniboxViewViews::GetLabelForCommandId(int command_id) const {
  if (command_id == IDS_PASTE_AND_GO) {
    return l10n_util::GetStringUTF16(
        model()->IsPasteAndSearch(GetClipboardText()) ?
        IDS_PASTE_AND_SEARCH : IDS_PASTE_AND_GO);
  }

  return string16();
}

bool OmniboxViewViews::HandlesCommand(int command_id) const {
  // See description in OnPaste() for details on why we need to handle paste.
  return command_id == IDS_APP_PASTE;
}

void OmniboxViewViews::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case IDS_APP_PASTE:
      OnPaste();
      break;
    case IDS_PASTE_AND_GO:
      model()->PasteAndGo(GetClipboardText());
      break;
    case IDC_COPY_URL:
      CopyURL();
      break;
    default:
      command_updater()->ExecuteCommand(command_id);
      break;
  }
}

#if defined(OS_CHROMEOS)
void OmniboxViewViews::CandidateWindowOpened(
      chromeos::input_method::InputMethodManager* manager) {
  ime_candidate_window_open_ = true;
  CloseOmniboxPopup();
}

void OmniboxViewViews::CandidateWindowClosed(
      chromeos::input_method::InputMethodManager* manager) {
  ime_candidate_window_open_ = false;
  UpdatePopup();
}
#endif

////////////////////////////////////////////////////////////////////////////////
// OmniboxViewViews, private:

int OmniboxViewViews::GetOmniboxTextLength() const {
  // TODO(oshima): Support instant, IME.
  return static_cast<int>(text().length());
}

void OmniboxViewViews::EmphasizeURLComponents() {
  // See whether the contents are a URL with a non-empty host portion, which we
  // should emphasize.  To check for a URL, rather than using the type returned
  // by Parse(), ask the model, which will check the desired page transition for
  // this input.  This can tell us whether an UNKNOWN input string is going to
  // be treated as a search or a navigation, and is the same method the Paste
  // And Go system uses.
  url_parse::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(text(), &scheme, &host);
  const bool emphasize = model()->CurrentTextIsURL() && (host.len > 0);
  SetColor(location_bar_view_->GetColor(security_level_,
      emphasize ? LocationBarView::DEEMPHASIZED_TEXT : LocationBarView::TEXT));
  if (emphasize) {
    ApplyColor(
        location_bar_view_->GetColor(security_level_, LocationBarView::TEXT),
        ui::Range(host.begin, host.end()));
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  // Note that we check CurrentTextIsURL() because if we're replacing search
  // URLs with search terms, we may have a non-URL even when the user is not
  // editing; and in some cases, e.g. for "site:foo.com" searches, the parser
  // may have incorrectly identified a qualifier as a scheme.
  SetStyle(gfx::DIAGONAL_STRIKE, false);
  if (!model()->user_input_in_progress() && model()->CurrentTextIsURL() &&
      scheme.is_nonempty() && (security_level_ != ToolbarModel::NONE)) {
    SkColor security_color = location_bar_view_->GetColor(
        security_level_, LocationBarView::SECURITY_TEXT);
    const bool strike = (security_level_ == ToolbarModel::SECURITY_ERROR);
    const ui::Range scheme_range(scheme.begin, scheme.end());
    ApplyColor(security_color, scheme_range);
    ApplyStyle(gfx::DIAGONAL_STRIKE, strike, scheme_range);
  }
}

void OmniboxViewViews::SetTextAndSelectedRange(const string16& text,
                                               const ui::Range& range) {
  SetText(text);
  SelectRange(range);
}

string16 OmniboxViewViews::GetSelectedText() const {
  // TODO(oshima): Support instant, IME.
  return views::Textfield::GetSelectedText();
}

void OmniboxViewViews::CopyURL() {
  DoCopyURL(toolbar_model()->GetURL(),
            toolbar_model()->GetText(false),
            model()->profile());
}

void OmniboxViewViews::OnPaste() {
  // Replace the selection if we have something to paste.
  const string16 text(GetClipboardText());
  if (!text.empty()) {
    // Record this paste, so we can do different behavior.
    model()->on_paste();
    // Force a Paste operation to trigger the text_changed code in
    // OnAfterPossibleChange(), even if identical contents are pasted.
    text_before_change_.clear();
    ReplaceSelection(text);
  }
}
