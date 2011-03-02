// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_edit_view_views.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_contents_view.h"
#include "chrome/browser/ui/views/autocomplete/touch_autocomplete_popup_contents_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/notification_service.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "views/border.h"
#include "views/layout/fill_layout.h"

namespace {

// Textfield for autocomplete that intercepts events that are necessary
// for AutocompleteEditViewViews.
class AutocompleteTextfield : public views::Textfield {
 public:
  explicit AutocompleteTextfield(
      AutocompleteEditViewViews* autocomplete_edit_view)
      : views::Textfield(views::Textfield::STYLE_DEFAULT),
        autocomplete_edit_view_(autocomplete_edit_view) {
    DCHECK(autocomplete_edit_view_);
    RemoveBorder();
  }

  // views::View implementation
  virtual void OnFocus() OVERRIDE {
    views::Textfield::OnFocus();
    autocomplete_edit_view_->HandleFocusIn();
  }

  virtual void OnBlur() OVERRIDE {
    views::Textfield::OnBlur();
    autocomplete_edit_view_->HandleFocusOut();
  }

  virtual bool OnKeyPressed(const views::KeyEvent& e) OVERRIDE {
    bool handled = views::Textfield::OnKeyPressed(e);
    return autocomplete_edit_view_->HandleAfterKeyEvent(e, handled) || handled;
  }

  virtual bool OnKeyReleased(const views::KeyEvent& e) OVERRIDE {
    return autocomplete_edit_view_->HandleKeyReleaseEvent(e);
  }

  virtual bool IsFocusable() const OVERRIDE {
    // Bypass Textfield::IsFocusable. The omnibox in popup window requires
    // focus in order for text selection to work.
    return views::View::IsFocusable();
  }

 private:
  AutocompleteEditViewViews* autocomplete_edit_view_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteTextfield);
};

// Stores omnibox state for each tab.
struct ViewState {
  explicit ViewState(const views::TextRange& selection_range)
      : selection_range(selection_range) {
  }

  // Range of selected text.
  views::TextRange selection_range;
};

struct AutocompleteEditState {
  AutocompleteEditState(const AutocompleteEditModel::State& model_state,
                        const ViewState& view_state)
      : model_state(model_state),
        view_state(view_state) {
  }

  const AutocompleteEditModel::State model_state;
  const ViewState view_state;
};

// Returns a lazily initialized property bag accessor for saving our state in a
// TabContents.
PropertyAccessor<AutocompleteEditState>* GetStateAccessor() {
  static PropertyAccessor<AutocompleteEditState> state;
  return &state;
}

const int kAutocompleteVerticalMargin = 4;

}  // namespace

AutocompleteEditViewViews::AutocompleteEditViewViews(
    AutocompleteEditController* controller,
    ToolbarModel* toolbar_model,
    Profile* profile,
    CommandUpdater* command_updater,
    bool popup_window_mode,
    const views::View* location_bar)
    : model_(new AutocompleteEditModel(this, controller, profile)),
      popup_view_(CreatePopupView(profile, location_bar)),
      controller_(controller),
      toolbar_model_(toolbar_model),
      command_updater_(command_updater),
      popup_window_mode_(popup_window_mode),
      security_level_(ToolbarModel::NONE),
      delete_was_pressed_(false),
      delete_at_end_pressed_(false) {
  set_border(views::Border::CreateEmptyBorder(kAutocompleteVerticalMargin, 0,
                                              kAutocompleteVerticalMargin, 0));
}

AutocompleteEditViewViews::~AutocompleteEditViewViews() {
  NotificationService::current()->Notify(
      NotificationType::AUTOCOMPLETE_EDIT_DESTROYED,
      Source<AutocompleteEditViewViews>(this),
      NotificationService::NoDetails());
  // Explicitly teardown members which have a reference to us.  Just to be safe
  // we want them to be destroyed before destroying any other internal state.
  popup_view_.reset();
  model_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteEditViewViews public:

void AutocompleteEditViewViews::Init() {
  // The height of the text view is going to change based on the font used.  We
  // don't want to stretch the height, and we want it vertically centered.
  // TODO(oshima): make sure the above happens with views.
  textfield_ = new AutocompleteTextfield(this);
  textfield_->SetController(this);

  if (popup_window_mode_)
    textfield_->SetReadOnly(true);

  // Manually invoke SetBaseColor() because TOOLKIT_VIEWS doesn't observe
  // themes.
  SetBaseColor();
}

void AutocompleteEditViewViews::SetBaseColor() {
  // TODO(oshima): Implment style change.
  NOTIMPLEMENTED();
}

bool AutocompleteEditViewViews::HandleAfterKeyEvent(
    const views::KeyEvent& event,
    bool handled) {
  handling_key_press_ = false;
  if (content_maybe_changed_by_key_press_)
    OnAfterPossibleChange();

  if (event.key_code() == ui::VKEY_RETURN) {
    bool alt_held = event.IsAltDown();
    model_->AcceptInput(alt_held ? NEW_FOREGROUND_TAB : CURRENT_TAB, false);
    handled = true;
  } else if (!handled && event.key_code() == ui::VKEY_ESCAPE) {
    // We can handle the Escape key if textfield did not handle it.
    // If it's not handled by us, then we need to propagate it up to the parent
    // widgets, so that Escape accelerator can still work.
    handled = model_->OnEscapeKeyPressed();
  } else if (event.key_code() == ui::VKEY_CONTROL) {
    // Omnibox2 can switch its contents while pressing a control key. To switch
    // the contents of omnibox2, we notify the AutocompleteEditModel class when
    // the control-key state is changed.
    model_->OnControlKeyChanged(true);
  } else if (!text_changed_ && event.key_code() == ui::VKEY_DELETE &&
             event.IsShiftDown()) {
    // If shift+del didn't change the text, we let this delete an entry from
    // the popup.  We can't check to see if the IME handled it because even if
    // nothing is selected, the IME or the TextView still report handling it.
    if (model_->popup_model()->IsOpen())
      model_->popup_model()->TryDeletingCurrentItem();
  } else if (!handled && event.key_code() == ui::VKEY_UP) {
    model_->OnUpOrDownKeyPressed(-1);
    handled = true;
  } else if (!handled && event.key_code() == ui::VKEY_DOWN) {
    model_->OnUpOrDownKeyPressed(1);
    handled = true;
  } else if (!handled &&
             event.key_code() == ui::VKEY_TAB &&
             !event.IsShiftDown() &&
             !event.IsControlDown()) {
    if (model_->is_keyword_hint()) {
      handled = model_->AcceptKeyword();
    } else {
      string16::size_type start = 0;
      string16::size_type end = 0;
      size_t length = GetTextLength();
      GetSelectionBounds(&start, &end);
      if (start != end || start < length) {
        OnBeforePossibleChange();
        SelectRange(length, length);
        OnAfterPossibleChange();
        handled = true;
      }

      // TODO(Oshima): handle instant
    }
  }
  // TODO(oshima): page up & down

  return handled;
}

bool AutocompleteEditViewViews::HandleKeyReleaseEvent(
    const views::KeyEvent& event) {
  // Omnibox2 can switch its contents while pressing a control key. To switch
  // the contents of omnibox2, we notify the AutocompleteEditModel class when
  // the control-key state is changed.
  if (event.key_code() == ui::VKEY_CONTROL) {
    // TODO(oshima): investigate if we need to support keyboard with two
    // controls. See autocomplete_edit_view_gtk.cc.
    model_->OnControlKeyChanged(false);
    return true;
  }
  return false;
}

void AutocompleteEditViewViews::HandleFocusIn() {
  // TODO(oshima): Get control key state.
  model_->OnSetFocus(false);
  // Don't call controller_->OnSetFocus as this view has already
  // acquired the focus.
}

void AutocompleteEditViewViews::HandleFocusOut() {
  // TODO(oshima): we don't have native view. This requires
  // further refactoring.
  controller_->OnAutocompleteLosingFocus(NULL);
  // Close the popup.
  ClosePopup();
  // Tell the model to reset itself.
  model_->OnKillFocus();
  controller_->OnKillFocus();
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteEditViewViews, views::View implementation:

bool AutocompleteEditViewViews::OnMousePressed(
    const views::MouseEvent& event) {
  if (event.IsLeftMouseButton()) {
    // Button press event may change the selection, we need to record the change
    // and report it to |model_| later when button is released.
    OnBeforePossibleChange();
  }
  // Pass the event through to TextfieldViews.
  return false;
}

void AutocompleteEditViewViews::Layout() {
  gfx::Insets insets = GetInsets();
  textfield_->SetBounds(insets.left(), insets.top(),
                        width() - insets.width(),
                        height() - insets.height());
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteEditViewViews, AutocopmleteEditView implementation:

AutocompleteEditModel* AutocompleteEditViewViews::model() {
  return model_.get();
}

const AutocompleteEditModel* AutocompleteEditViewViews::model() const {
  return model_.get();
}

void AutocompleteEditViewViews::SaveStateToTab(TabContents* tab) {
  DCHECK(tab);

  // NOTE: GetStateForTabSwitch may affect GetSelection, so order is important.
  AutocompleteEditModel::State model_state = model_->GetStateForTabSwitch();
  views::TextRange selection;
  textfield_->GetSelectedRange(&selection);
  GetStateAccessor()->SetProperty(
      tab->property_bag(),
      AutocompleteEditState(model_state, ViewState(selection)));
}

void AutocompleteEditViewViews::Update(const TabContents* contents) {
  // NOTE: We're getting the URL text here from the ToolbarModel.
  bool visibly_changed_permanent_text =
      model_->UpdatePermanentText(WideToUTF16Hack(toolbar_model_->GetText()));

  ToolbarModel::SecurityLevel security_level =
        toolbar_model_->GetSecurityLevel();
  bool changed_security_level = (security_level != security_level_);
  security_level_ = security_level;

  // TODO(oshima): Copied from gtk implementation which is
  // slightly different from WIN impl. Find out the correct implementation
  // for views-implementation.
  if (contents) {
    RevertAll();
    const AutocompleteEditState* state =
        GetStateAccessor()->GetProperty(contents->property_bag());
    if (state) {
      model_->RestoreState(state->model_state);

      // Move the marks for the cursor and the other end of the selection to
      // the previously-saved offsets (but preserve PRIMARY).
      textfield_->SelectRange(state->view_state.selection_range);
    }
  } else if (visibly_changed_permanent_text) {
    RevertAll();
  } else if (changed_security_level) {
    EmphasizeURLComponents();
  }
}

void AutocompleteEditViewViews::OpenURL(const GURL& url,
                                        WindowOpenDisposition disposition,
                                        PageTransition::Type transition,
                                        const GURL& alternate_nav_url,
                                        size_t selected_line,
                                        const string16& keyword) {
  if (!url.is_valid())
    return;

  model_->OpenURL(url, disposition, transition, alternate_nav_url,
                  selected_line, keyword);
}

string16 AutocompleteEditViewViews::GetText() const {
  // TODO(oshima): IME support
  return textfield_->text();
}

bool AutocompleteEditViewViews::IsEditingOrEmpty() const {
  return model_->user_input_in_progress() || (GetTextLength() == 0);
}

int AutocompleteEditViewViews::GetIcon() const {
  return IsEditingOrEmpty() ?
      AutocompleteMatch::TypeToIcon(model_->CurrentTextType()) :
      toolbar_model_->GetIcon();
}

void AutocompleteEditViewViews::SetUserText(const string16& text) {
  SetUserText(text, text, true);
}

void AutocompleteEditViewViews::SetUserText(const string16& text,
                                            const string16& display_text,
                                            bool update_popup) {
  model_->SetUserText(text);
  SetWindowTextAndCaretPos(display_text, display_text.length());
  if (update_popup)
    UpdatePopup();
  TextChanged();
}

void AutocompleteEditViewViews::SetWindowTextAndCaretPos(
    const string16& text,
    size_t caret_pos) {
  const views::TextRange range(caret_pos, caret_pos);
  SetTextAndSelectedRange(text, range);
}

void AutocompleteEditViewViews::SetForcedQuery() {
  const string16 current_text(GetText());
  const size_t start = current_text.find_first_not_of(kWhitespaceUTF16);
  if (start == string16::npos || (current_text[start] != '?')) {
    SetUserText(ASCIIToUTF16("?"));
  } else {
    SelectRange(current_text.size(), start + 1);
  }
}

bool AutocompleteEditViewViews::IsSelectAll() {
  // TODO(oshima): IME support.
  return textfield_->text() == textfield_->GetSelectedText();
}

bool AutocompleteEditViewViews::DeleteAtEndPressed() {
  return delete_at_end_pressed_;
}

void AutocompleteEditViewViews::GetSelectionBounds(
    string16::size_type* start,
    string16::size_type* end) {
  views::TextRange range;
  textfield_->GetSelectedRange(&range);
  *start = static_cast<size_t>(range.end());
  *end = static_cast<size_t>(range.start());
}

void AutocompleteEditViewViews::SelectAll(bool reversed) {
  if (reversed)
    SelectRange(GetTextLength(), 0);
  else
    SelectRange(0, GetTextLength());
}

void AutocompleteEditViewViews::RevertAll() {
  ClosePopup();
  model_->Revert();
  TextChanged();
}

void AutocompleteEditViewViews::UpdatePopup() {
  model_->SetInputInProgress(true);
  if (!model_->has_focus())
    return;

  // Don't inline autocomplete when the caret/selection isn't at the end of
  // the text, or in the middle of composition.
  views::TextRange sel;
  textfield_->GetSelectedRange(&sel);
  bool no_inline_autocomplete = sel.GetMax() < GetTextLength();

  // TODO(oshima): Support IME. Don't show autocomplete if IME has some text.
  model_->StartAutocomplete(!sel.is_empty(), no_inline_autocomplete);
}

void AutocompleteEditViewViews::ClosePopup() {
  if (model_->popup_model()->IsOpen())
    controller_->OnAutocompleteWillClosePopup();

  model_->StopAutocomplete();
}

void AutocompleteEditViewViews::SetFocus() {
  // In views-implementation, the focus is on textfield rather than
  // AutocompleteEditView.
  textfield_->RequestFocus();
}

void AutocompleteEditViewViews::OnTemporaryTextMaybeChanged(
    const string16& display_text,
    bool save_original_selection) {
  if (save_original_selection)
    textfield_->GetSelectedRange(&saved_temporary_selection_);

  SetWindowTextAndCaretPos(display_text, display_text.length());
  TextChanged();
}

bool AutocompleteEditViewViews::OnInlineAutocompleteTextMaybeChanged(
    const string16& display_text,
    size_t user_text_length) {
  if (display_text == GetText())
    return false;
  views::TextRange range(display_text.size(), user_text_length);
  SetTextAndSelectedRange(display_text, range);
  TextChanged();
  return true;
}

void AutocompleteEditViewViews::OnRevertTemporaryText() {
  textfield_->SelectRange(saved_temporary_selection_);
  TextChanged();
}

void AutocompleteEditViewViews::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = GetText();
  textfield_->GetSelectedRange(&sel_before_change_);
}

bool AutocompleteEditViewViews::OnAfterPossibleChange() {
  // OnAfterPossibleChange should be called once per modification,
  // and we should ignore if this is called while a key event is being handled
  // because OnAfterPossibleChagne will be called after the key event is
  // actually handled.
  if (handling_key_press_) {
    content_maybe_changed_by_key_press_ = true;
    return false;
  }
  views::TextRange new_sel;
  textfield_->GetSelectedRange(&new_sel);

  size_t length = GetTextLength();
  bool at_end_of_edit = (new_sel.start() == length && new_sel.end() == length);

  // See if the text or selection have changed since OnBeforePossibleChange().
  string16 new_text = GetText();
  text_changed_ = (new_text != text_before_change_);
  bool selection_differs =
      !((sel_before_change_.is_empty() && new_sel.is_empty()) ||
        sel_before_change_.EqualsIgnoringDirection(new_sel));

  // When the user has deleted text, we don't allow inline autocomplete.  Make
  // sure to not flag cases like selecting part of the text and then pasting
  // (or typing) the prefix of that selection.  (We detect these by making
  // sure the caret, which should be after any insertion, hasn't moved
  // forward of the old selection start.)
  bool just_deleted_text =
      (text_before_change_.length() > new_text.length()) &&
      (new_sel.start() <= sel_before_change_.GetMin());

  delete_at_end_pressed_ = false;

  bool something_changed = model_->OnAfterPossibleChange(new_text,
      selection_differs, text_changed_, just_deleted_text, at_end_of_edit);

  // If only selection was changed, we don't need to call |controller_|'s
  // OnChanged() method, which is called in TextChanged().
  // But we still need to call EmphasizeURLComponents() to make sure the text
  // attributes are updated correctly.
  if (something_changed && text_changed_) {
    TextChanged();
  } else if (selection_differs) {
    EmphasizeURLComponents();
  } else if (delete_was_pressed_ && at_end_of_edit) {
    delete_at_end_pressed_ = true;
    controller_->OnChanged();
  }
  delete_was_pressed_ = false;

  return something_changed;
}

gfx::NativeView AutocompleteEditViewViews::GetNativeView() const {
  return GetWidget()->GetNativeView();
}

CommandUpdater* AutocompleteEditViewViews::GetCommandUpdater() {
  return command_updater_;
}

void AutocompleteEditViewViews::SetInstantSuggestion(const string16& input) {
  NOTIMPLEMENTED();
}

string16 AutocompleteEditViewViews::GetInstantSuggestion() const {
  NOTIMPLEMENTED();
  return string16();
}

int AutocompleteEditViewViews::TextWidth() const {
  // TODO(oshima): add horizontal margin.
  return textfield_->font().GetStringWidth(textfield_->text());
}

bool AutocompleteEditViewViews::IsImeComposing() const {
  return false;
}

views::View* AutocompleteEditViewViews::AddToView(views::View* parent) {
  parent->AddChildView(this);
  AddChildView(textfield_);
  return this;
}

int AutocompleteEditViewViews::OnPerformDrop(
    const views::DropTargetEvent& event) {
  NOTIMPLEMENTED();
  return ui::DragDropTypes::DRAG_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteEditViewViews, NotificationObserver implementation:

void AutocompleteEditViewViews::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_THEME_CHANGED);
  SetBaseColor();
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteEditViewViews, Textfield::Controller implementation:

void AutocompleteEditViewViews::ContentsChanged(views::Textfield* sender,
                                                const string16& new_contents) {
  if (handling_key_press_)
    content_maybe_changed_by_key_press_ = true;
}

bool AutocompleteEditViewViews::HandleKeyEvent(
    views::Textfield* textfield,
    const views::KeyEvent& event) {
  delete_was_pressed_ = event.key_code() == ui::VKEY_DELETE;

  // Reset |text_changed_| before passing the key event on to the text view.
  text_changed_ = false;
  OnBeforePossibleChange();
  handling_key_press_ = true;
  content_maybe_changed_by_key_press_ = false;

  if (event.key_code() == ui::VKEY_BACK) {
    // Checks if it's currently in keyword search mode.
    if (model_->is_keyword_hint() || model_->keyword().empty())
      return false;
    // If there is selection, let textfield handle the backspace.
    if (!textfield_->GetSelectedText().empty())
      return false;
    // If not at the begining of the text, let textfield handle the backspace.
    if (textfield_->GetCursorPosition())
      return false;
    model_->ClearKeyword(GetText());
    return true;
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// AutocompleteEditViewViews, private:

size_t AutocompleteEditViewViews::GetTextLength() const {
  // TODO(oshima): Support instant, IME.
  return textfield_->text().length();
}

void AutocompleteEditViewViews::EmphasizeURLComponents() {
  // TODO(oshima): Update URL visual style
  NOTIMPLEMENTED();
}

void AutocompleteEditViewViews::TextChanged() {
  EmphasizeURLComponents();
  controller_->OnChanged();
}

void AutocompleteEditViewViews::SetTextAndSelectedRange(
    const string16& text,
    const views::TextRange& range) {
  if (text != GetText())
    textfield_->SetText(text);
  textfield_->SelectRange(range);
}

string16 AutocompleteEditViewViews::GetSelectedText() const {
  // TODO(oshima): Support instant, IME.
  return textfield_->GetSelectedText();
}

void AutocompleteEditViewViews::SelectRange(size_t caret, size_t end) {
  const views::TextRange range(caret, end);
  textfield_->SelectRange(range);
}

AutocompletePopupView* AutocompleteEditViewViews::CreatePopupView(
    Profile* profile,
    const View* location_bar) {
#if defined(TOUCH_UI)
  return new TouchAutocompletePopupContentsView(
#else
  return new AutocompletePopupContentsView(
#endif
      gfx::Font(), this, model_.get(), profile, location_bar);
}
