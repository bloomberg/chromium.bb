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
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/search/search.h"
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
#include "ui/gfx/render_text.h"
#include "ui/views/border.h"
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

using content::WebContents;

namespace {

// Stores omnibox state for each tab.
struct ViewState {
  explicit ViewState(const gfx::SelectionModel& selection_model)
      : selection_model(selection_model) {
  }

  // SelectionModel of selected text.
  gfx::SelectionModel selection_model;
};

const char kAutocompleteEditStateKey[] = "AutocompleteEditState";

struct AutocompleteEditState : public base::SupportsUserData::Data {
  AutocompleteEditState(const OmniboxEditModel::State& model_state,
                        const ViewState& view_state)
      : model_state(model_state),
        view_state(view_state) {
  }
  virtual ~AutocompleteEditState() {}

  const OmniboxEditModel::State model_state;
  const ViewState view_state;
};

// A convenience method for applying URL styles.
void ApplyURLStyle(views::Textfield* textfield,
                   size_t start,
                   size_t end,
                   SkColor color,
                   bool diagonal_strike) {
  gfx::StyleRange style;
  style.foreground = color;
  style.range = ui::Range(start, end);
  style.diagonal_strike = diagonal_strike;
  textfield->ApplyStyleRange(style);
}

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

// Copies |selected_text| as text to the primary clipboard.
void DoCopyText(const string16& selected_text) {
  ui::ScopedClipboardWriter scw(ui::Clipboard::GetForCurrentThread(),
                                ui::Clipboard::BUFFER_STANDARD);
  scw.WriteText(selected_text);
}

// This will write |url| and |text| to the clipboard as a well-formed URL.
void DoCopyURL(const GURL& url, const string16& text) {
  BookmarkNodeData data;
  data.ReadFromTuple(url, text);
  data.WriteToClipboard(NULL);
}

}  // namespace

// Textfield for autocomplete that intercepts events that are necessary
// for OmniboxViewViews.
class OmniboxViewViews::AutocompleteTextfield : public views::Textfield {
 public:
  AutocompleteTextfield(OmniboxViewViews* omnibox_view,
                        LocationBarView* location_bar_view)
      : views::Textfield(views::Textfield::STYLE_DEFAULT),
        omnibox_view_(omnibox_view),
        location_bar_view_(location_bar_view) {
    DCHECK(omnibox_view_);
    RemoveBorder();
    set_id(VIEW_ID_OMNIBOX);
  }

  // views::View implementation
  virtual void OnFocus() OVERRIDE {
    views::Textfield::OnFocus();
    omnibox_view_->HandleFocusIn();
  }

  virtual void OnBlur() OVERRIDE {
    views::Textfield::OnBlur();
    omnibox_view_->HandleFocusOut();
  }

  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE {
    // Use our own implementation of paste. See OnPaste() for details.
    if (!read_only() && event.IsControlDown() &&
        event.key_code() == ui::VKEY_V) {
      omnibox_view_->OnBeforePossibleChange();
      omnibox_view_->OnPaste();
      omnibox_view_->OnAfterPossibleChange();
      return true;
    }
    bool handled = views::Textfield::OnKeyPressed(event);
    return omnibox_view_->HandleAfterKeyEvent(event, handled) || handled;
  }

  virtual bool OnKeyReleased(const ui::KeyEvent& event) OVERRIDE {
    return omnibox_view_->HandleKeyReleaseEvent(event);
  }

  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    // Pass through the views::Textfield's return value; we don't need to
    // override its behavior.
    bool result = views::Textfield::OnMousePressed(event);
    omnibox_view_->HandleMousePressEvent(event);
    return result;
  }

  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE {
    bool result = views::Textfield::OnMouseDragged(event);
    omnibox_view_->HandleMouseDragEvent(event);
    return result;
  }

  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE {
    views::Textfield::OnMouseReleased(event);
    omnibox_view_->HandleMouseReleaseEvent(event);
  }

 private:
  OmniboxViewViews* omnibox_view_;
  LocationBarView* location_bar_view_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteTextfield);
};

// static
const char OmniboxViewViews::kViewClassName[] = "BrowserOmniboxViewViews";

OmniboxViewViews::OmniboxViewViews(OmniboxEditController* controller,
                                   ToolbarModel* toolbar_model,
                                   Profile* profile,
                                   CommandUpdater* command_updater,
                                   bool popup_window_mode,
                                   LocationBarView* location_bar)
    : OmniboxView(profile, controller, toolbar_model, command_updater),
      textfield_(NULL),
      popup_window_mode_(popup_window_mode),
      security_level_(ToolbarModel::NONE),
      ime_composing_before_change_(false),
      delete_at_end_pressed_(false),
      location_bar_view_(location_bar),
      ime_candidate_window_open_(false),
      select_all_on_mouse_release_(false) {
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
  textfield_ = new AutocompleteTextfield(this, location_bar_view_);
  textfield_->SetController(this);
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_URL);
  textfield_->SetBackgroundColor(location_bar_view_->GetColor(
      ToolbarModel::NONE, LocationBarView::BACKGROUND));

  if (popup_window_mode_)
    textfield_->SetReadOnly(true);

  const int font_size = GetEditFontPixelSize(popup_window_mode_);
  const int old_size = textfield_->font().GetFontSize();
  if (font_size != old_size)
    textfield_->SetFont(textfield_->font().DeriveFont(font_size - old_size));

  // Create popup view using the same font as |textfield_|'s.
  popup_view_.reset(
      OmniboxPopupContentsView::Create(
          textfield_->font(), this, model(), location_bar_view_));

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
  return textfield_->font();
}

int OmniboxViewViews::WidthOfTextAfterCursor() {
  ui::Range sel;
  textfield_->GetSelectedRange(&sel);
  // See comments in LocationBarView::Layout as to why this uses -1.
  const int start = std::max(0, static_cast<int>(sel.end()) - 1);
  // TODO: add horizontal margin.
  return textfield_->font().GetStringWidth(GetText().substr(start));
}

bool OmniboxViewViews::HandleAfterKeyEvent(const ui::KeyEvent& event,
                                           bool handled) {
  if (event.key_code() == ui::VKEY_RETURN) {
    bool alt_held = event.IsAltDown();
    model()->AcceptInput(alt_held ? NEW_FOREGROUND_TAB : CURRENT_TAB, false);
    handled = true;
  } else if (!handled && event.key_code() == ui::VKEY_ESCAPE) {
    // We can handle the Escape key if textfield did not handle it.
    // If it's not handled by us, then we need to propagate it up to the parent
    // widgets, so that Escape accelerator can still work.
    handled = model()->OnEscapeKeyPressed();
  } else if (event.key_code() == ui::VKEY_CONTROL) {
    // Omnibox2 can switch its contents while pressing a control key. To switch
    // the contents of omnibox2, we notify the OmniboxEditModel class when the
    // control-key state is changed.
    model()->OnControlKeyChanged(true);
  } else if (!handled && event.key_code() == ui::VKEY_DELETE &&
             event.IsShiftDown()) {
    // If shift+del didn't change the text, we let this delete an entry from
    // the popup.  We can't check to see if the IME handled it because even if
    // nothing is selected, the IME or the TextView still report handling it.
    if (model()->popup_model()->IsOpen())
      model()->popup_model()->TryDeletingCurrentItem();
  } else if (!handled && event.key_code() == ui::VKEY_UP) {
    model()->OnUpOrDownKeyPressed(-1);
    handled = true;
  } else if (!handled && event.key_code() == ui::VKEY_DOWN) {
    model()->OnUpOrDownKeyPressed(1);
    handled = true;
  } else if (!handled &&
             event.key_code() == ui::VKEY_TAB &&
             !event.IsControlDown()) {
    if (model()->is_keyword_hint() && !event.IsShiftDown()) {
      handled = model()->AcceptKeyword();
    } else if (model()->popup_model()->IsOpen()) {
      if (event.IsShiftDown() &&
          model()->popup_model()->selected_line_state() ==
              OmniboxPopupModel::KEYWORD) {
        model()->ClearKeyword(GetText());
      } else {
        model()->OnUpOrDownKeyPressed(event.IsShiftDown() ? -1 : 1);
      }
      handled = true;
    } else {
      string16::size_type start = 0;
      string16::size_type end = 0;
      size_t length = GetTextLength();
      GetSelectionBounds(&start, &end);
      if (start != end || start < length) {
        OnBeforePossibleChange();
        textfield_->SelectRange(ui::Range(length, length));
        OnAfterPossibleChange();
        handled = true;
      }

      // TODO(Oshima): handle instant
    }
  }
  // TODO(oshima): page up & down

  return handled;
}

bool OmniboxViewViews::HandleKeyReleaseEvent(const ui::KeyEvent& event) {
  // Omnibox2 can switch its contents while pressing a control key. To switch
  // the contents of omnibox2, we notify the OmniboxEditModel class when the
  // control-key state is changed.
  if (event.key_code() == ui::VKEY_CONTROL) {
    // TODO(oshima): investigate if we need to support keyboard with two
    // controls.
    model()->OnControlKeyChanged(false);
    return true;
  }
  return false;
}

void OmniboxViewViews::HandleMousePressEvent(const ui::MouseEvent& event) {
  select_all_on_mouse_release_ =
      (event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      (!textfield_->HasFocus() ||
       (model()->focus_state() == OMNIBOX_FOCUS_INVISIBLE));
  // Restore caret visibility whenever the user clicks in the omnibox in a way
  // that would give it focus.  We must handle this case separately here because
  // if the omnibox currently has invisible focus, the mouse event won't trigger
  // either SetFocus() or OmniboxEditModel::OnSetFocus().
  if (select_all_on_mouse_release_)
    model()->SetCaretVisibility(true);
}

void OmniboxViewViews::HandleMouseDragEvent(const ui::MouseEvent& event) {
  select_all_on_mouse_release_ = false;
}

void OmniboxViewViews::HandleMouseReleaseEvent(const ui::MouseEvent& event) {
  if ((event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      select_all_on_mouse_release_) {
    // Select all in the reverse direction so as not to scroll the caret
    // into view and shift the contents jarringly.
    SelectAll(true);
  }
  select_all_on_mouse_release_ = false;
}

void OmniboxViewViews::HandleFocusIn() {
  // TODO(oshima): Get control key state.
  model()->OnSetFocus(false);
  // Don't call controller()->OnSetFocus as this view has already
  // acquired the focus.
}

void OmniboxViewViews::HandleFocusOut() {
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

void OmniboxViewViews::SetLocationEntryFocusable(bool focusable) {
  textfield_->set_focusable(focusable);
}

bool OmniboxViewViews::IsLocationEntryFocusableInRootView() const {
  return textfield_->IsFocusable();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxViewViews, views::View implementation:
void OmniboxViewViews::Layout() {
  gfx::Insets insets = GetInsets();
  textfield_->SetBounds(insets.left(), insets.top(),
                        width() - insets.width(),
                        height() - insets.height());
}

void OmniboxViewViews::GetAccessibleState(ui::AccessibleViewState* state) {
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_LOCATION);
}

std::string OmniboxViewViews::GetClassName() const {
  return kViewClassName;
}

void OmniboxViewViews::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  if (popup_view_->IsOpen())
    popup_view_->UpdatePopupAppearance();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxViewViews, OmniboxView implementation:

void OmniboxViewViews::SaveStateToTab(WebContents* tab) {
  DCHECK(tab);

  // We don't want to keep the IME status, so force quit the current
  // session here.  It may affect the selection status, so order is
  // also important.
  if (textfield_->IsIMEComposing()) {
    textfield_->GetTextInputClient()->ConfirmCompositionText();
    textfield_->GetInputMethod()->CancelComposition(textfield_);
  }

  // NOTE: GetStateForTabSwitch may affect GetSelection, so order is important.
  OmniboxEditModel::State model_state = model()->GetStateForTabSwitch();
  gfx::SelectionModel selection;
  textfield_->GetSelectionModel(&selection);
  tab->SetUserData(
      kAutocompleteEditStateKey,
      new AutocompleteEditState(model_state, ViewState(selection)));
}

void OmniboxViewViews::Update(const WebContents* contents) {
  // NOTE: We're getting the URL text here from the ToolbarModel.
  bool visibly_changed_permanent_text =
      model()->UpdatePermanentText(toolbar_model()->GetText(true));
  ToolbarModel::SecurityLevel security_level =
        toolbar_model()->GetSecurityLevel();
  bool changed_security_level = (security_level != security_level_);
  security_level_ = security_level;

  // TODO(oshima): Copied from gtk implementation which is
  // slightly different from WIN impl. Find out the correct implementation
  // for views-implementation.
  if (contents) {
    RevertAll();
    const AutocompleteEditState* state = static_cast<AutocompleteEditState*>(
        contents->GetUserData(&kAutocompleteEditStateKey));
    if (state) {
      model()->RestoreState(state->model_state);

      // Move the marks for the cursor and the other end of the selection to
      // the previously-saved offsets (but preserve PRIMARY).
      textfield_->SelectSelectionModel(state->view_state.selection_model);
      // We do not carry over the current edit history to another tab.
      // TODO(oshima): consider saving/restoring edit history.
      textfield_->ClearEditHistory();
    }
  } else if (visibly_changed_permanent_text) {
    RevertAll();
  } else if (changed_security_level) {
    EmphasizeURLComponents();
  }
}

string16 OmniboxViewViews::GetText() const {
  // TODO(oshima): IME support
  return textfield_->text();
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
  const string16 current_text(GetText());
  const size_t start = current_text.find_first_not_of(kWhitespaceUTF16);
  if (start == string16::npos || (current_text[start] != '?'))
    SetUserText(ASCIIToUTF16("?"));
  else
    textfield_->SelectRange(ui::Range(current_text.size(), start + 1));
}

bool OmniboxViewViews::IsSelectAll() const {
  // TODO(oshima): IME support.
  return textfield_->text() == textfield_->GetSelectedText();
}

bool OmniboxViewViews::DeleteAtEndPressed() {
  return delete_at_end_pressed_;
}

void OmniboxViewViews::GetSelectionBounds(string16::size_type* start,
                                          string16::size_type* end) const {
  ui::Range range;
  textfield_->GetSelectedRange(&range);
  if (range.is_empty()) {
    // Omnibox API expects that selection bounds is at cursor position
    // if there is no selection.
    *start = textfield_->GetCursorPosition();
    *end = textfield_->GetCursorPosition();
  } else {
    *start = static_cast<size_t>(range.end());
    *end = static_cast<size_t>(range.start());
  }
}

void OmniboxViewViews::SelectAll(bool reversed) {
  textfield_->SelectAll(reversed);
}

void OmniboxViewViews::UpdatePopup() {
  model()->SetInputInProgress(true);
  if (ime_candidate_window_open_)
    return;
  if (!model()->has_focus())
    return;

  // Don't inline autocomplete when the caret/selection isn't at the end of
  // the text, or in the middle of composition.
  ui::Range sel;
  textfield_->GetSelectedRange(&sel);
  bool no_inline_autocomplete =
      sel.GetMax() < GetTextLength() || textfield_->IsIMEComposing();

  model()->StartAutocomplete(!sel.is_empty(), no_inline_autocomplete);
}

void OmniboxViewViews::SetFocus() {
  // In views-implementation, the focus is on textfield rather than OmniboxView.
  textfield_->RequestFocus();
  // Restore caret visibility if focus is explicitly requested. This is
  // necessary because if we already have invisible focus, the RequestFocus()
  // call above will short-circuit, preventing us from reaching
  // OmniboxEditModel::OnSetFocus(), which handles restoring visibility when the
  // omnibox regains focus after losing focus.
  model()->SetCaretVisibility(true);
}

void OmniboxViewViews::ApplyCaretVisibility() {
  textfield_->SetCursorEnabled(model()->is_caret_visible());
}

void OmniboxViewViews::OnTemporaryTextMaybeChanged(
    const string16& display_text,
    bool save_original_selection) {
  if (save_original_selection)
    textfield_->GetSelectedRange(&saved_temporary_selection_);

  SetWindowTextAndCaretPos(display_text, display_text.length(), false, true);
}

bool OmniboxViewViews::OnInlineAutocompleteTextMaybeChanged(
    const string16& display_text,
    size_t user_text_length) {
  if (display_text == GetText())
    return false;
  ui::Range range(display_text.size(), user_text_length);
  SetTextAndSelectedRange(display_text, range);
  TextChanged();
  return true;
}

void OmniboxViewViews::OnRevertTemporaryText() {
  textfield_->SelectRange(saved_temporary_selection_);
  TextChanged();
}

void OmniboxViewViews::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = GetText();
  textfield_->GetSelectedRange(&sel_before_change_);
  ime_composing_before_change_ = textfield_->IsIMEComposing();
}

bool OmniboxViewViews::OnAfterPossibleChange() {
  ui::Range new_sel;
  textfield_->GetSelectedRange(&new_sel);

  // See if the text or selection have changed since OnBeforePossibleChange().
  const string16 new_text = GetText();
  const bool text_changed = (new_text != text_before_change_) ||
      (ime_composing_before_change_ != textfield_->IsIMEComposing());
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
      selection_differs, text_changed, just_deleted_text,
      !textfield_->IsIMEComposing());

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
  return textfield_->font().GetStringWidth(textfield_->text());
}

bool OmniboxViewViews::IsImeComposing() const {
  return false;
}

int OmniboxViewViews::GetMaxEditWidth(int entry_width) const {
  return entry_width;
}

views::View* OmniboxViewViews::AddToView(views::View* parent) {
  parent->AddChildView(this);
  AddChildView(textfield_);
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
    // Checks if it's currently in keyword search mode.
    if (model()->is_keyword_hint() || model()->keyword().empty())
      return false;
    // If there is selection, let textfield handle the backspace.
    if (textfield_->HasSelection())
      return false;
    // If not at the begining of the text, let textfield handle the backspace.
    if (textfield_->GetCursorPosition())
      return false;
    model()->ClearKeyword(GetText());
    return true;
  }

  if (event.key_code() == ui::VKEY_DELETE && !event.IsAltDown()) {
    delete_at_end_pressed_ =
        (!textfield_->HasSelection() &&
         textfield_->GetCursorPosition() == textfield_->text().length());
  }

  // Though the Textfield usually handles the right-arrow key for LTR text or
  // left-arrow key for RTL text, we need to handle it if we have gray text
  // (Instant suggestion) that needs to be committed.
  if (textfield_->GetCursorPosition() == textfield_->text().length()) {
    base::i18n::TextDirection direction = textfield_->GetTextDirection();
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
  ui::Range selection_range;
  textfield_->GetSelectedRange(&selection_range);
  ui::Clipboard* cb = ui::Clipboard::GetForCurrentThread();
  string16 selected_text;
  cb->ReadText(ui::Clipboard::BUFFER_STANDARD, &selected_text);
  const string16 text = textfield_->text();
  GURL url;
  bool write_url;
  model()->AdjustTextForCopy(selection_range.GetMin(), selected_text == text,
      &selected_text, &url, &write_url);
  if (write_url)
    DoCopyURL(url, selected_text);
  else
    DoCopyText(selected_text);
}

void OmniboxViewViews::OnWriteDragData(ui::OSExchangeData* data) {
  ui::Range selection_range;
  textfield_->GetSelectedRange(&selection_range);
  string16 selected_text = textfield_->GetSelectedText();
  const string16 text = textfield_->text();
  GURL url;
  bool write_url;
  model()->AdjustTextForCopy(selection_range.start(), selected_text == text,
      &selected_text, &url, &write_url);
  data->SetString(selected_text);
  if (write_url)
    data->SetURL(url, selected_text);
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

  if (chrome::search::IsQueryExtractionEnabled(location_bar_view_->profile())) {
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

void OmniboxViewViews::ExecuteCommand(int command_id) {
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

size_t OmniboxViewViews::GetTextLength() const {
  // TODO(oshima): Support instant, IME.
  return textfield_->text().length();
}

int OmniboxViewViews::GetOmniboxTextLength() const {
  return static_cast<int>(GetTextLength());
}

void OmniboxViewViews::EmphasizeURLComponents() {
  // See whether the contents are a URL with a non-empty host portion, which we
  // should emphasize.  To check for a URL, rather than using the type returned
  // by Parse(), ask the model, which will check the desired page transition for
  // this input.  This can tell us whether an UNKNOWN input string is going to
  // be treated as a search or a navigation, and is the same method the Paste
  // And Go system uses.
  string16 text = GetText();
  url_parse::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(text, model()->GetDesiredTLD(),
                                                 &scheme, &host);
  const bool emphasize = model()->CurrentTextIsURL() && (host.len > 0);

  SkColor base_color = location_bar_view_->GetColor(
      security_level_,
      emphasize ? LocationBarView::DEEMPHASIZED_TEXT : LocationBarView::TEXT);
  ApplyURLStyle(textfield_, 0, text.length(), base_color, false);

  if (emphasize) {
    SkColor normal_color = location_bar_view_->GetColor(
        security_level_, LocationBarView::TEXT);
    ApplyURLStyle(textfield_, host.begin, host.end(), normal_color, false);
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  // Note that we check CurrentTextIsURL() because if we're replacing search
  // URLs with search terms, we may have a non-URL even when the user is not
  // editing; and in some cases, e.g. for "site:foo.com" searches, the parser
  // may have incorrectly identified a qualifier as a scheme.
  if (!model()->user_input_in_progress() && model()->CurrentTextIsURL() &&
      scheme.is_nonempty() && (security_level_ != ToolbarModel::NONE)) {
    SkColor security_color = location_bar_view_->GetColor(
        security_level_, LocationBarView::SECURITY_TEXT);
    bool use_strikethrough = (security_level_ == ToolbarModel::SECURITY_ERROR);
    ApplyURLStyle(textfield_, scheme.begin, scheme.end(),
                  security_color, use_strikethrough);
  }
}

void OmniboxViewViews::SetTextAndSelectedRange(const string16& text,
                                               const ui::Range& range) {
  if (text != GetText())
    textfield_->SetText(text);
  textfield_->SelectRange(range);
}

string16 OmniboxViewViews::GetSelectedText() const {
  // TODO(oshima): Support instant, IME.
  return textfield_->GetSelectedText();
}

void OmniboxViewViews::CopyURL() {
  DoCopyURL(toolbar_model()->GetURL(), toolbar_model()->GetText(false));
}

void OmniboxViewViews::OnPaste() {
  // Replace the selection if we have something to paste.
  const string16 text(GetClipboardText());
  if (!text.empty()) {
    // Record this paste, so we can do different behavior.
    model()->on_paste();
    // Force a Paste operation to trigger the text_changed code in
    // OnAfterPossibleChange(), even if identical contents are pasted into the
    // text box.
    text_before_change_.clear();
    textfield_->ReplaceSelection(text);
  }
}
