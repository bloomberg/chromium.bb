// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include <set>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/settings_api_bubble_helper_views.h"
#include "chrome/browser/ui/views/website_settings/website_settings_popup_view.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/omnibox/autocomplete_input.h"
#include "components/omnibox/autocomplete_match.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "grit/app_locale_settings.h"
#include "grit/generated_resources.h"
#include "grit/ui_strings.h"
#include "net/base/escape.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/selection_model.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/browser/browser_process.h"
#endif

using bookmarks::BookmarkNodeData;

namespace {

// OmniboxState ---------------------------------------------------------------

// Stores omnibox state for each tab.
struct OmniboxState : public base::SupportsUserData::Data {
  static const char kKey[];

  OmniboxState(const OmniboxEditModel::State& model_state,
               const gfx::Range& selection,
               const gfx::Range& saved_selection_for_focus_change);
  virtual ~OmniboxState();

  const OmniboxEditModel::State model_state;

  // We store both the actual selection and any saved selection (for when the
  // omnibox is not focused).  This allows us to properly handle cases like
  // selecting text, tabbing out of the omnibox, switching tabs away and back,
  // and tabbing back into the omnibox.
  const gfx::Range selection;
  const gfx::Range saved_selection_for_focus_change;
};

// static
const char OmniboxState::kKey[] = "OmniboxState";

OmniboxState::OmniboxState(const OmniboxEditModel::State& model_state,
                           const gfx::Range& selection,
                           const gfx::Range& saved_selection_for_focus_change)
    : model_state(model_state),
      selection(selection),
      saved_selection_for_focus_change(saved_selection_for_focus_change) {
}

OmniboxState::~OmniboxState() {
}


// Helpers --------------------------------------------------------------------

// We'd like to set the text input type to TEXT_INPUT_TYPE_URL, because this
// triggers URL-specific layout in software keyboards, e.g. adding top-level "/"
// and ".com" keys for English.  However, this also causes IMEs to default to
// Latin character mode, which makes entering search queries difficult for IME
// users.  Therefore, we try to guess whether an IME will be used based on the
// application language, and set the input type accordingly.
ui::TextInputType DetermineTextInputType() {
#if defined(OS_WIN)
  DCHECK(g_browser_process);
  const std::string& locale = g_browser_process->GetApplicationLocale();
  const std::string& language = locale.substr(0, 2);
  // Assume CJK + Thai users are using an IME.
  if (language == "ja" ||
      language == "ko" ||
      language == "th" ||
      language == "zh")
    return ui::TEXT_INPUT_TYPE_SEARCH;
#endif
  return ui::TEXT_INPUT_TYPE_URL;
}

}  // namespace


// OmniboxViewViews -----------------------------------------------------------

// static
const char OmniboxViewViews::kViewClassName[] = "OmniboxViewViews";

OmniboxViewViews::OmniboxViewViews(OmniboxEditController* controller,
                                   Profile* profile,
                                   CommandUpdater* command_updater,
                                   bool popup_window_mode,
                                   LocationBarView* location_bar,
                                   const gfx::FontList& font_list)
    : OmniboxView(profile, controller, command_updater),
      popup_window_mode_(popup_window_mode),
      security_level_(ToolbarModel::NONE),
      saved_selection_for_focus_change_(gfx::Range::InvalidRange()),
      ime_composing_before_change_(false),
      delete_at_end_pressed_(false),
      location_bar_view_(location_bar),
      ime_candidate_window_open_(false),
      select_all_on_mouse_release_(false),
      select_all_on_gesture_tap_(false),
      weak_ptr_factory_(this) {
  SetBorder(views::Border::NullBorder());
  set_id(VIEW_ID_OMNIBOX);
  SetFontList(font_list);
}

OmniboxViewViews::~OmniboxViewViews() {
#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager::Get()->
      RemoveCandidateWindowObserver(this);
#endif

  // Explicitly teardown members which have a reference to us.  Just to be safe
  // we want them to be destroyed before destroying any other internal state.
  popup_view_.reset();
}

void OmniboxViewViews::Init() {
  set_controller(this);
  SetTextInputType(DetermineTextInputType());

  if (popup_window_mode_)
    SetReadOnly(true);

  // Initialize the popup view using the same font.
  popup_view_.reset(OmniboxPopupContentsView::Create(
      GetFontList(), this, model(), location_bar_view_));

#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager::Get()->
      AddCandidateWindowObserver(this);
#endif
}

void OmniboxViewViews::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);

  // We don't want to keep the IME status, so force quit the current
  // session here.  It may affect the selection status, so order is
  // also important.
  if (IsIMEComposing()) {
    GetTextInputClient()->ConfirmCompositionText();
    GetInputMethod()->CancelComposition(this);
  }

  // NOTE: GetStateForTabSwitch() may affect GetSelectedRange(), so order is
  // important.
  OmniboxEditModel::State state = model()->GetStateForTabSwitch();
  tab->SetUserData(OmniboxState::kKey, new OmniboxState(
      state, GetSelectedRange(), saved_selection_for_focus_change_));
}

void OmniboxViewViews::OnTabChanged(const content::WebContents* web_contents) {
  security_level_ = controller()->GetToolbarModel()->GetSecurityLevel(false);

  const OmniboxState* state = static_cast<OmniboxState*>(
      web_contents->GetUserData(&OmniboxState::kKey));
  model()->RestoreState(state ? &state->model_state : NULL);
  if (state) {
    // This assumes that the omnibox has already been focused or blurred as
    // appropriate; otherwise, a subsequent OnFocus() or OnBlur() call could
    // goof up the selection.  See comments at the end of
    // BrowserView::ActiveTabChanged().
    SelectRange(state->selection);
    saved_selection_for_focus_change_ = state->saved_selection_for_focus_change;
  }

  // TODO(msw|oshima): Consider saving/restoring edit history.
  ClearEditHistory();
}

void OmniboxViewViews::Update() {
  UpdatePlaceholderText();

  const ToolbarModel::SecurityLevel old_security_level = security_level_;
  security_level_ = controller()->GetToolbarModel()->GetSecurityLevel(false);
  if (model()->UpdatePermanentText()) {
    // Something visibly changed.  Re-enable URL replacement.
    controller()->GetToolbarModel()->set_url_replacement_enabled(true);
    controller()->GetToolbarModel()->set_origin_chip_enabled(true);
    model()->UpdatePermanentText();

    // Select all the new text if the user had all the old text selected, or if
    // there was no previous text (for new tab page URL replacement extensions).
    // This makes one particular case better: the user clicks in the box to
    // change it right before the permanent URL is changed.  Since the new URL
    // is still fully selected, the user's typing will replace the edit contents
    // as they'd intended.
    const bool was_select_all = IsSelectAll();
    const bool was_reversed = GetSelectedRange().is_reversed();

    RevertAll();

    // Only select all when we have focus.  If we don't have focus, selecting
    // all is unnecessary since the selection will change on regaining focus,
    // and can in fact cause artifacts, e.g. if the user is on the NTP and
    // clicks a link to navigate, causing |was_select_all| to be vacuously true
    // for the empty omnibox, and we then select all here, leading to the
    // trailing portion of a long URL being scrolled into view.  We could try
    // and address cases like this, but it seems better to just not muck with
    // things when the omnibox isn't focused to begin with.
    if (was_select_all && model()->has_focus())
      SelectAll(was_reversed);
  } else if (old_security_level != security_level_) {
    EmphasizeURLComponents();
  }
}

void OmniboxViewViews::UpdatePlaceholderText() {
  if (chrome::ShouldDisplayOriginChip() ||
      OmniboxFieldTrial::DisplayHintTextWhenPossible())
    set_placeholder_text(GetHintText());
}

base::string16 OmniboxViewViews::GetText() const {
  // TODO(oshima): IME support
  return text();
}

void OmniboxViewViews::SetUserText(const base::string16& text,
                                   const base::string16& display_text,
                                   bool update_popup) {
  saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  OmniboxView::SetUserText(text, display_text, update_popup);
}

void OmniboxViewViews::SetForcedQuery() {
  const base::string16 current_text(text());
  const size_t start = current_text.find_first_not_of(base::kWhitespaceUTF16);
  if (start == base::string16::npos || (current_text[start] != '?'))
    OmniboxView::SetUserText(base::ASCIIToUTF16("?"));
  else
    SelectRange(gfx::Range(current_text.size(), start + 1));
}

void OmniboxViewViews::GetSelectionBounds(
    base::string16::size_type* start,
    base::string16::size_type* end) const {
  const gfx::Range range = GetSelectedRange();
  *start = static_cast<size_t>(range.start());
  *end = static_cast<size_t>(range.end());
}

void OmniboxViewViews::SelectAll(bool reversed) {
  views::Textfield::SelectAll(reversed);
}

void OmniboxViewViews::RevertAll() {
  saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  OmniboxView::RevertAll();
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

int OmniboxViewViews::GetTextWidth() const {
  // Returns the width necessary to display the current text, including any
  // necessary space for the cursor or border/margin.
  return GetRenderText()->GetContentWidth() + GetInsets().width();
}

bool OmniboxViewViews::IsImeComposing() const {
  return IsIMEComposing();
}

gfx::Size OmniboxViewViews::GetMinimumSize() const {
  const int kMinCharacters = 10;
  return gfx::Size(
      GetFontList().GetExpectedTextWidth(kMinCharacters) + GetInsets().width(),
      GetPreferredSize().height());
}

void OmniboxViewViews::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  views::Textfield::OnNativeThemeChanged(theme);
  SetBackgroundColor(location_bar_view_->GetColor(
      ToolbarModel::NONE, LocationBarView::BACKGROUND));
  EmphasizeURLComponents();
}

void OmniboxViewViews::ExecuteCommand(int command_id, int event_flags) {
  // In the base class, touch text selection is deactivated when a command is
  // executed. Since we are not always calling the base class implementation
  // here, we need to deactivate touch text selection here, too.
  DestroyTouchSelection();
  switch (command_id) {
    // These commands don't invoke the popup via OnBefore/AfterPossibleChange().
    case IDS_PASTE_AND_GO:
      model()->PasteAndGo(GetClipboardText());
      break;
    case IDS_SHOW_URL:
      controller()->ShowURL();
      break;
    case IDC_EDIT_SEARCH_ENGINES:
      command_updater()->ExecuteCommand(command_id);
      break;
    case IDS_MOVE_DOWN:
    case IDS_MOVE_UP:
      model()->OnUpOrDownKeyPressed(command_id == IDS_MOVE_DOWN ? 1 : -1);
      break;

    default:
      OnBeforePossibleChange();
      if (command_id == IDS_APP_PASTE)
        OnPaste();
      else if (Textfield::IsCommandIdEnabled(command_id))
        Textfield::ExecuteCommand(command_id, event_flags);
      else
        command_updater()->ExecuteCommand(command_id);
      OnAfterPossibleChange();
      break;
  }
}

void OmniboxViewViews::SetTextAndSelectedRange(const base::string16& text,
                                               const gfx::Range& range) {
  SetText(text);
  SelectRange(range);
}

base::string16 OmniboxViewViews::GetSelectedText() const {
  // TODO(oshima): Support IME.
  return views::Textfield::GetSelectedText();
}

void OmniboxViewViews::OnPaste() {
  const base::string16 text(GetClipboardText());
  if (!text.empty()) {
    // Record this paste, so we can do different behavior.
    model()->OnPaste();
    // Force a Paste operation to trigger the text_changed code in
    // OnAfterPossibleChange(), even if identical contents are pasted.
    text_before_change_.clear();
    InsertOrReplaceText(text);
  }
}

bool OmniboxViewViews::HandleEarlyTabActions(const ui::KeyEvent& event) {
  // This must run before acclerator handling invokes a focus change on tab.
  // Note the parallel with SkipDefaultKeyEventProcessing above.
  if (!views::FocusManager::IsTabTraversalKeyEvent(event))
    return false;

  if (model()->is_keyword_hint() && !event.IsShiftDown()) {
    model()->AcceptKeyword(ENTERED_KEYWORD_MODE_VIA_TAB);
    return true;
  }

  if (!model()->popup_model()->IsOpen())
    return false;

  if (event.IsShiftDown() &&
      (model()->popup_model()->selected_line_state() ==
          OmniboxPopupModel::KEYWORD))
    model()->ClearKeyword(text());
  else
    model()->OnUpOrDownKeyPressed(event.IsShiftDown() ? -1 : 1);

  return true;
}

void OmniboxViewViews::AccessibilitySetValue(const base::string16& new_value) {
  SetUserText(new_value, new_value, true);
}

void OmniboxViewViews::SetWindowTextAndCaretPos(const base::string16& text,
                                                size_t caret_pos,
                                                bool update_popup,
                                                bool notify_text_changed) {
  const gfx::Range range(caret_pos, caret_pos);
  SetTextAndSelectedRange(text, range);

  if (update_popup)
    UpdatePopup();

  if (notify_text_changed)
    TextChanged();
}

bool OmniboxViewViews::IsSelectAll() const {
  // TODO(oshima): IME support.
  return text() == GetSelectedText();
}

bool OmniboxViewViews::DeleteAtEndPressed() {
  return delete_at_end_pressed_;
}

void OmniboxViewViews::UpdatePopup() {
  model()->SetInputInProgress(true);
  if (!model()->has_focus())
    return;

  // Prevent inline autocomplete when the caret isn't at the end of the text.
  const gfx::Range sel = GetSelectedRange();
  model()->StartAutocomplete(!sel.is_empty(), sel.GetMax() < text().length());
}

void OmniboxViewViews::ApplyCaretVisibility() {
  SetCursorEnabled(model()->is_caret_visible());
}

void OmniboxViewViews::OnTemporaryTextMaybeChanged(
    const base::string16& display_text,
    bool save_original_selection,
    bool notify_text_changed) {
  if (save_original_selection)
    saved_temporary_selection_ = GetSelectedRange();

  SetWindowTextAndCaretPos(display_text, display_text.length(), false,
                           notify_text_changed);
}

bool OmniboxViewViews::OnInlineAutocompleteTextMaybeChanged(
    const base::string16& display_text,
    size_t user_text_length) {
  if (display_text == text())
    return false;

  if (IsIMEComposing()) {
    location_bar_view_->SetImeInlineAutocompletion(
        display_text.substr(user_text_length));
  } else {
    gfx::Range range(display_text.size(), user_text_length);
    SetTextAndSelectedRange(display_text, range);
  }
  TextChanged();
  return true;
}

void OmniboxViewViews::OnInlineAutocompleteTextCleared() {
  // Hide the inline autocompletion for IME users.
  location_bar_view_->SetImeInlineAutocompletion(base::string16());
}

void OmniboxViewViews::OnRevertTemporaryText() {
  SelectRange(saved_temporary_selection_);
  // We got here because the user hit the Escape key. We explicitly don't call
  // TextChanged(), since OmniboxPopupModel::ResetToDefaultMatch() has already
  // been called by now, and it would've called TextChanged() if it was
  // warranted.
}

void OmniboxViewViews::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = text();
  sel_before_change_ = GetSelectedRange();
  ime_composing_before_change_ = IsIMEComposing();
}

bool OmniboxViewViews::OnAfterPossibleChange() {
  // See if the text or selection have changed since OnBeforePossibleChange().
  const base::string16 new_text = text();
  const gfx::Range new_sel = GetSelectedRange();
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

void OmniboxViewViews::SetGrayTextAutocompletion(const base::string16& input) {
  location_bar_view_->SetGrayTextAutocompletion(input);
}

base::string16 OmniboxViewViews::GetGrayTextAutocompletion() const {
  return location_bar_view_->GetGrayTextAutocompletion();
}

int OmniboxViewViews::GetWidth() const {
  return location_bar_view_->width();
}

bool OmniboxViewViews::IsImeShowingPopup() const {
#if defined(OS_CHROMEOS)
  return ime_candidate_window_open_;
#else
  const views::InputMethod* input_method = this->GetInputMethod();
  return input_method && input_method->IsCandidatePopupOpen();
#endif
}

void OmniboxViewViews::ShowImeIfNeeded() {
  GetInputMethod()->ShowImeIfNeeded();
}

void OmniboxViewViews::OnMatchOpened(const AutocompleteMatch& match,
                                     content::WebContents* web_contents) {
  extensions::MaybeShowExtensionControlledSearchNotification(
      profile(), web_contents, match);
}

int OmniboxViewViews::GetOmniboxTextLength() const {
  // TODO(oshima): Support IME.
  return static_cast<int>(text().length());
}

void OmniboxViewViews::EmphasizeURLComponents() {
  // See whether the contents are a URL with a non-empty host portion, which we
  // should emphasize.  To check for a URL, rather than using the type returned
  // by Parse(), ask the model, which will check the desired page transition for
  // this input.  This can tell us whether an UNKNOWN input string is going to
  // be treated as a search or a navigation, and is the same method the Paste
  // And Go system uses.
  url::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      text(), ChromeAutocompleteSchemeClassifier(profile()), &scheme, &host);
  bool grey_out_url = text().substr(scheme.begin, scheme.len) ==
      base::UTF8ToUTF16(extensions::kExtensionScheme);
  bool grey_base = model()->CurrentTextIsURL() &&
      (host.is_nonempty() || grey_out_url);
  SetColor(location_bar_view_->GetColor(
      security_level_,
      grey_base ? LocationBarView::DEEMPHASIZED_TEXT : LocationBarView::TEXT));
  if (grey_base && !grey_out_url) {
    ApplyColor(
        location_bar_view_->GetColor(security_level_, LocationBarView::TEXT),
        gfx::Range(host.begin, host.end()));
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
    const gfx::Range scheme_range(scheme.begin, scheme.end());
    ApplyColor(security_color, scheme_range);
    ApplyStyle(gfx::DIAGONAL_STRIKE, strike, scheme_range);
  }
}

bool OmniboxViewViews::OnKeyReleased(const ui::KeyEvent& event) {
  // The omnibox contents may change while the control key is pressed.
  if (event.key_code() == ui::VKEY_CONTROL)
    model()->OnControlKeyChanged(false);
  return views::Textfield::OnKeyReleased(event);
}

bool OmniboxViewViews::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDS_PASTE_AND_GO;
}

base::string16 OmniboxViewViews::GetLabelForCommandId(int command_id) const {
  DCHECK_EQ(IDS_PASTE_AND_GO, command_id);
  return l10n_util::GetStringUTF16(
      model()->IsPasteAndSearch(GetClipboardText()) ?
          IDS_PASTE_AND_SEARCH : IDS_PASTE_AND_GO);
}

const char* OmniboxViewViews::GetClassName() const {
  return kViewClassName;
}

bool OmniboxViewViews::OnMousePressed(const ui::MouseEvent& event) {
  select_all_on_mouse_release_ =
      (event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      (!HasFocus() || (model()->focus_state() == OMNIBOX_FOCUS_INVISIBLE));
  if (select_all_on_mouse_release_) {
    // Restore caret visibility whenever the user clicks in the omnibox in a way
    // that would give it focus.  We must handle this case separately here
    // because if the omnibox currently has invisible focus, the mouse event
    // won't trigger either SetFocus() or OmniboxEditModel::OnSetFocus().
    model()->SetCaretVisibility(true);

    // When we're going to select all on mouse release, invalidate any saved
    // selection lest restoring it fights with the "select all" action.  It's
    // possible to later set select_all_on_mouse_release_ back to false, but
    // that happens for things like dragging, which are cases where having
    // invalidated this saved selection is still OK.
    saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  }
  return views::Textfield::OnMousePressed(event);
}

bool OmniboxViewViews::OnMouseDragged(const ui::MouseEvent& event) {
  if (ExceededDragThreshold(event.location() - last_click_location()))
    select_all_on_mouse_release_ = false;

  if (HasTextBeingDragged())
    CloseOmniboxPopup();

  return views::Textfield::OnMouseDragged(event);
}

void OmniboxViewViews::OnMouseReleased(const ui::MouseEvent& event) {
  views::Textfield::OnMouseReleased(event);
  if (event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) {
    // When the user has clicked and released to give us focus, select all
    // unless we're omitting the URL (in which case refining an existing query
    // is common enough that we do click-to-place-cursor).
    if (select_all_on_mouse_release_ &&
        !controller()->GetToolbarModel()->WouldReplaceURL()) {
      // Select all in the reverse direction so as not to scroll the caret
      // into view and shift the contents jarringly.
      SelectAll(true);
    }

    HandleOriginChipMouseRelease();
  }
  select_all_on_mouse_release_ = false;
}

bool OmniboxViewViews::OnKeyPressed(const ui::KeyEvent& event) {
  // Skip processing of [Alt]+<num-pad digit> Unicode alt key codes.
  // Otherwise, if num-lock is off, the events are handled as [Up], [Down], etc.
  if (event.IsUnicodeKeyCode())
    return views::Textfield::OnKeyPressed(event);

  const bool shift = event.IsShiftDown();
  const bool control = event.IsControlDown();
  const bool alt = event.IsAltDown() || event.IsAltGrDown();
  switch (event.key_code()) {
    case ui::VKEY_RETURN:
      model()->AcceptInput(alt ? NEW_FOREGROUND_TAB : CURRENT_TAB, false);
      return true;
    case ui::VKEY_ESCAPE:
      return model()->OnEscapeKeyPressed();
    case ui::VKEY_CONTROL:
      model()->OnControlKeyChanged(true);
      break;
    case ui::VKEY_DELETE:
      if (shift && model()->popup_model()->IsOpen())
        model()->popup_model()->TryDeletingCurrentItem();
      break;
    case ui::VKEY_UP:
      if (!read_only()) {
        model()->OnUpOrDownKeyPressed(-1);
        return true;
      }
      break;
    case ui::VKEY_DOWN:
      if (!read_only()) {
        model()->OnUpOrDownKeyPressed(1);
        return true;
      }
      break;
    case ui::VKEY_PRIOR:
      if (control || alt || shift)
        return false;
      model()->OnUpOrDownKeyPressed(-1 * model()->result().size());
      return true;
    case ui::VKEY_NEXT:
      if (control || alt || shift)
        return false;
      model()->OnUpOrDownKeyPressed(model()->result().size());
      return true;
    case ui::VKEY_V:
      if (control && !alt && !read_only()) {
        ExecuteCommand(IDS_APP_PASTE, 0);
        return true;
      }
      break;
    case ui::VKEY_INSERT:
      if (shift && !control && !read_only()) {
        ExecuteCommand(IDS_APP_PASTE, 0);
        return true;
      }
      break;
    default:
      break;
  }

  return views::Textfield::OnKeyPressed(event) || HandleEarlyTabActions(event);
}

void OmniboxViewViews::OnGestureEvent(ui::GestureEvent* event) {
  if (!HasFocus() && event->type() == ui::ET_GESTURE_TAP_DOWN) {
    select_all_on_gesture_tap_ = true;

    // If we're trying to select all on tap, invalidate any saved selection lest
    // restoring it fights with the "select all" action.
    saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  }

  views::Textfield::OnGestureEvent(event);

  if (select_all_on_gesture_tap_ && event->type() == ui::ET_GESTURE_TAP)
    SelectAll(true);

  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_CANCEL ||
      event->type() == ui::ET_GESTURE_TWO_FINGER_TAP ||
      event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event->type() == ui::ET_GESTURE_PINCH_BEGIN ||
      event->type() == ui::ET_GESTURE_LONG_PRESS ||
      event->type() == ui::ET_GESTURE_LONG_TAP) {
    select_all_on_gesture_tap_ = false;
  }
}

void OmniboxViewViews::AboutToRequestFocusFromTabTraversal(bool reverse) {
  views::Textfield::AboutToRequestFocusFromTabTraversal(reverse);
  // Tabbing into the omnibox should affect the origin chip in the same way
  // clicking it should.
  HandleOriginChipMouseRelease();
}

bool OmniboxViewViews::SkipDefaultKeyEventProcessing(
    const ui::KeyEvent& event) {
  if (views::FocusManager::IsTabTraversalKeyEvent(event) &&
      ((model()->is_keyword_hint() && !event.IsShiftDown()) ||
       model()->popup_model()->IsOpen())) {
    return true;
  }
  return Textfield::SkipDefaultKeyEventProcessing(event);
}

void OmniboxViewViews::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_TEXT_FIELD;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_LOCATION);
  state->value = GetText();

  base::string16::size_type entry_start;
  base::string16::size_type entry_end;
  GetSelectionBounds(&entry_start, &entry_end);
  state->selection_start = entry_start;
  state->selection_end = entry_end;

  if (popup_window_mode_) {
    state->AddStateFlag(ui::AX_STATE_READ_ONLY);
  } else {
    state->set_value_callback =
        base::Bind(&OmniboxViewViews::AccessibilitySetValue,
                   weak_ptr_factory_.GetWeakPtr());
  }

}

void OmniboxViewViews::OnFocus() {
  views::Textfield::OnFocus();
  // TODO(oshima): Get control key state.
  model()->OnSetFocus(false);
  // Don't call controller()->OnSetFocus, this view has already acquired focus.

  // Restore the selection we saved in OnBlur() if it's still valid.
  if (saved_selection_for_focus_change_.IsValid()) {
    SelectRange(saved_selection_for_focus_change_);
    saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  }
}

void OmniboxViewViews::OnBlur() {
  // Save the user's existing selection to restore it later.
  saved_selection_for_focus_change_ = GetSelectedRange();

  views::Textfield::OnBlur();
  gfx::NativeView native_view = NULL;
  views::Widget* widget = GetWidget();
  if (widget) {
    aura::client::FocusClient* client =
        aura::client::GetFocusClient(widget->GetNativeView());
    if (client)
      native_view = client->GetFocusedWindow();
  }
  model()->OnWillKillFocus(native_view);
  // Close the popup.
  CloseOmniboxPopup();

  // Tell the model to reset itself.
  model()->OnKillFocus();

  // Ignore loss of focus if we lost focus because the website settings popup
  // is open. When the popup is destroyed, focus will return to the Omnibox.
  if (!WebsiteSettingsPopupView::IsPopupShowing())
    OnDidKillFocus();

  // Make sure the beginning of the text is visible.
  SelectRange(gfx::Range(0));
}

bool OmniboxViewViews::IsCommandIdEnabled(int command_id) const {
  if (command_id == IDS_APP_PASTE)
    return !read_only() && !GetClipboardText().empty();
  if (command_id == IDS_PASTE_AND_GO)
    return !read_only() && model()->CanPasteAndGo(GetClipboardText());
  if (command_id == IDS_SHOW_URL)
    return controller()->GetToolbarModel()->WouldReplaceURL();
  return command_id == IDS_MOVE_DOWN || command_id == IDS_MOVE_UP ||
         Textfield::IsCommandIdEnabled(command_id) ||
         command_updater()->IsCommandEnabled(command_id);
}

base::string16 OmniboxViewViews::GetSelectionClipboardText() const {
  return SanitizeTextForPaste(Textfield::GetSelectionClipboardText());
}

#if defined(OS_CHROMEOS)
void OmniboxViewViews::CandidateWindowOpened(
      chromeos::input_method::InputMethodManager* manager) {
  ime_candidate_window_open_ = true;
}

void OmniboxViewViews::CandidateWindowClosed(
      chromeos::input_method::InputMethodManager* manager) {
  ime_candidate_window_open_ = false;
}
#endif

void OmniboxViewViews::ContentsChanged(views::Textfield* sender,
                                       const base::string16& new_contents) {
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
  // if there is gray text that needs to be committed.
  if (GetCursorPosition() == text().length()) {
    base::i18n::TextDirection direction = GetTextDirection();
    if ((direction == base::i18n::LEFT_TO_RIGHT &&
         event.key_code() == ui::VKEY_RIGHT) ||
        (direction == base::i18n::RIGHT_TO_LEFT &&
         event.key_code() == ui::VKEY_LEFT)) {
      return model()->CommitSuggestedText();
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

void OmniboxViewViews::OnAfterCutOrCopy(ui::ClipboardType clipboard_type) {
  ui::Clipboard* cb = ui::Clipboard::GetForCurrentThread();
  base::string16 selected_text;
  cb->ReadText(clipboard_type, &selected_text);
  GURL url;
  bool write_url;
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), IsSelectAll(),
                             &selected_text, &url, &write_url);
  if (IsSelectAll())
    UMA_HISTOGRAM_COUNTS(OmniboxEditModel::kCutOrCopyAllTextHistogram, 1);

  if (write_url) {
    BookmarkNodeData data;
    data.ReadFromTuple(url, selected_text);
    data.WriteToClipboard(clipboard_type);
  } else {
    ui::ScopedClipboardWriter scoped_clipboard_writer(
        ui::Clipboard::GetForCurrentThread(), clipboard_type);
    scoped_clipboard_writer.WriteText(selected_text);
  }
}

void OmniboxViewViews::OnWriteDragData(ui::OSExchangeData* data) {
  GURL url;
  bool write_url;
  bool is_all_selected = IsSelectAll();
  base::string16 selected_text = GetSelectedText();
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), is_all_selected,
                             &selected_text, &url, &write_url);
  data->SetString(selected_text);
  if (write_url) {
    gfx::Image favicon;
    base::string16 title = selected_text;
    if (is_all_selected)
      model()->GetDataForURLExport(&url, &title, &favicon);
    button_drag_utils::SetURLAndDragImage(url, title, favicon.AsImageSkia(),
                                          NULL, data, GetWidget());
    data->SetURL(url, title);
  }
}

void OmniboxViewViews::OnGetDragOperationsForTextfield(int* drag_operations) {
  base::string16 selected_text = GetSelectedText();
  GURL url;
  bool write_url;
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), IsSelectAll(),
                             &selected_text, &url, &write_url);
  if (write_url)
    *drag_operations |= ui::DragDropTypes::DRAG_LINK;
}

void OmniboxViewViews::AppendDropFormats(
    int* formats,
    std::set<ui::OSExchangeData::CustomFormat>* custom_formats) {
  *formats = *formats | ui::OSExchangeData::URL;
}

int OmniboxViewViews::OnDrop(const ui::OSExchangeData& data) {
  if (HasTextBeingDragged())
    return ui::DragDropTypes::DRAG_NONE;

  if (data.HasURL(ui::OSExchangeData::CONVERT_FILENAMES)) {
    GURL url;
    base::string16 title;
    if (data.GetURLAndTitle(
            ui::OSExchangeData::CONVERT_FILENAMES, &url, &title)) {
      base::string16 text(
          StripJavascriptSchemas(base::UTF8ToUTF16(url.spec())));
      if (model()->CanPasteAndGo(text)) {
        model()->PasteAndGo(text);
        return ui::DragDropTypes::DRAG_COPY;
      }
    }
  } else if (data.HasString()) {
    base::string16 text;
    if (data.GetString(&text)) {
      base::string16 collapsed_text(base::CollapseWhitespace(text, true));
      if (model()->CanPasteAndGo(collapsed_text))
        model()->PasteAndGo(collapsed_text);
      return ui::DragDropTypes::DRAG_COPY;
    }
  }

  return ui::DragDropTypes::DRAG_NONE;
}

void OmniboxViewViews::UpdateContextMenu(ui::SimpleMenuModel* menu_contents) {
  int paste_position = menu_contents->GetIndexOfCommandId(IDS_APP_PASTE);
  DCHECK_GE(paste_position, 0);
  menu_contents->InsertItemWithStringIdAt(
      paste_position + 1, IDS_PASTE_AND_GO, IDS_PASTE_AND_GO);

  menu_contents->AddSeparator(ui::NORMAL_SEPARATOR);

  if (chrome::IsQueryExtractionEnabled() || chrome::ShouldDisplayOriginChip()) {
    int select_all_position = menu_contents->GetIndexOfCommandId(
        IDS_APP_SELECT_ALL);
    DCHECK_GE(select_all_position, 0);
    menu_contents->InsertItemWithStringIdAt(
        select_all_position + 1, IDS_SHOW_URL, IDS_SHOW_URL);
  }

  // Minor note: We use IDC_ for command id here while the underlying textfield
  // is using IDS_ for all its command ids. This is because views cannot depend
  // on IDC_ for now.
  menu_contents->AddItemWithStringId(IDC_EDIT_SEARCH_ENGINES,
      IDS_EDIT_SEARCH_ENGINES);
}
