// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_popup_model.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/render_text.h"
#include "ui/views/events/event.h"
#include "ui/views/layout/fill_layout.h"
#include "views/border.h"
#include "views/controls/textfield/textfield.h"

#if defined(TOUCH_UI)
#include "chrome/browser/ui/views/autocomplete/touch_autocomplete_popup_contents_view.h"
#else
#include "chrome/browser/ui/views/autocomplete/autocomplete_popup_contents_view.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/views/omnibox/omnibox_view_win.h"
#endif

namespace {

// Textfield for autocomplete that intercepts events that are necessary
// for OmniboxViewViews.
class AutocompleteTextfield : public views::Textfield {
 public:
  explicit AutocompleteTextfield(OmniboxViewViews* omnibox_view)
      : views::Textfield(views::Textfield::STYLE_DEFAULT),
        omnibox_view_(omnibox_view) {
    DCHECK(omnibox_view_);
    RemoveBorder();
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

  virtual bool OnKeyPressed(const views::KeyEvent& event) OVERRIDE {
    bool handled = views::Textfield::OnKeyPressed(event);
    return omnibox_view_->HandleAfterKeyEvent(event, handled) || handled;
  }

  virtual bool OnKeyReleased(const views::KeyEvent& event) OVERRIDE {
    return omnibox_view_->HandleKeyReleaseEvent(event);
  }

  virtual bool IsFocusable() const OVERRIDE {
    // Bypass Textfield::IsFocusable. The omnibox in popup window requires
    // focus in order for text selection to work.
    return views::View::IsFocusable();
  }

  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    return omnibox_view_->HandleMousePressEvent(event);
  }

 private:
  OmniboxViewViews* omnibox_view_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteTextfield);
};

// Stores omnibox state for each tab.
struct ViewState {
  explicit ViewState(const gfx::SelectionModel& selection_model)
      : selection_model(selection_model) {
  }

  // SelectionModel of selected text.
  gfx::SelectionModel selection_model;
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
  CR_DEFINE_STATIC_LOCAL(PropertyAccessor<AutocompleteEditState>, state, ());
  return &state;
}

// A convenience method for applying URL styles.
void ApplyURLStyle(views::Textfield* textfield,
                   size_t start,
                   size_t end,
                   SkColor color,
                   bool strike) {
  gfx::StyleRange style;
  style.font = textfield->font();
  style.foreground = color;
  style.range = ui::Range(start, end);
  style.strike = strike;
  textfield->ApplyStyleRange(style);
}

const int kAutocompleteVerticalMargin = 4;

// TODO(oshima): I'm currently using slightly different color than
// gtk/win omnibox so that I can tell which one is used from its color.
// Fix this once we finish all features.
const SkColor kFadedTextColor = SK_ColorGRAY;
const SkColor kNormalTextColor = SK_ColorBLACK;
const SkColor kSecureSchemeColor = SK_ColorGREEN;
const SkColor kSecurityErrorSchemeColor = SK_ColorRED;

}  // namespace

// static
const char OmniboxViewViews::kViewClassName[] =
    "browser/ui/views/omnibox/OmniboxViewViews";

OmniboxViewViews::OmniboxViewViews(AutocompleteEditController* controller,
                                   ToolbarModel* toolbar_model,
                                   Profile* profile,
                                   CommandUpdater* command_updater,
                                   bool popup_window_mode,
                                   LocationBarView* location_bar)
    : model_(new AutocompleteEditModel(this, controller, profile)),
      popup_view_(CreatePopupView(location_bar)),
      controller_(controller),
      toolbar_model_(toolbar_model),
      command_updater_(command_updater),
      popup_window_mode_(popup_window_mode),
      security_level_(ToolbarModel::NONE),
      ime_composing_before_change_(false),
      delete_at_end_pressed_(false),
      location_bar_view_(location_bar) {
  set_border(views::Border::CreateEmptyBorder(kAutocompleteVerticalMargin, 0,
                                              kAutocompleteVerticalMargin, 0));
}

OmniboxViewViews::~OmniboxViewViews() {
  // Explicitly teardown members which have a reference to us.  Just to be safe
  // we want them to be destroyed before destroying any other internal state.
  popup_view_.reset();
  model_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxViewViews public:

void OmniboxViewViews::Init() {
  // The height of the text view is going to change based on the font used.  We
  // don't want to stretch the height, and we want it vertically centered.
  // TODO(oshima): make sure the above happens with views.
  textfield_ = new AutocompleteTextfield(this);
  textfield_->SetController(this);
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_URL);

#if defined(TOUCH_UI)
  textfield_->SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
                      ResourceBundle::LargeFont));
#endif

  if (popup_window_mode_)
    textfield_->SetReadOnly(true);

  // Manually invoke SetBaseColor() because TOOLKIT_VIEWS doesn't observe
  // themes.
  SetBaseColor();
}

void OmniboxViewViews::SetBaseColor() {
  // TODO(oshima): Implement style change.
  NOTIMPLEMENTED();
}

bool OmniboxViewViews::HandleAfterKeyEvent(const views::KeyEvent& event,
                                           bool handled) {
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
  } else if (!handled && event.key_code() == ui::VKEY_DELETE &&
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

bool OmniboxViewViews::HandleKeyReleaseEvent(const views::KeyEvent& event) {
  // Omnibox2 can switch its contents while pressing a control key. To switch
  // the contents of omnibox2, we notify the AutocompleteEditModel class when
  // the control-key state is changed.
  if (event.key_code() == ui::VKEY_CONTROL) {
    // TODO(oshima): investigate if we need to support keyboard with two
    // controls. See omnibox_view_gtk.cc.
    model_->OnControlKeyChanged(false);
    return true;
  }
  return false;
}

bool OmniboxViewViews::HandleMousePressEvent(const views::MouseEvent& event) {
  if (!textfield_->HasFocus() && !textfield_->HasSelection()) {
    textfield_->SelectAll();
    textfield_->RequestFocus();
    return true;
  }

  return false;
}

void OmniboxViewViews::HandleFocusIn() {
  // TODO(oshima): Get control key state.
  model_->OnSetFocus(false);
  // Don't call controller_->OnSetFocus as this view has already
  // acquired the focus.
}

void OmniboxViewViews::HandleFocusOut() {
  // TODO(oshima): we don't have native view. This requires
  // further refactoring.
  model_->OnWillKillFocus(NULL);
  // Close the popup.
  ClosePopup();
  // Tell the model to reset itself.
  model_->OnKillFocus();
  controller_->OnKillFocus();
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
// OmniboxViewViews, AutocopmleteEditView implementation:

AutocompleteEditModel* OmniboxViewViews::model() {
  return model_.get();
}

const AutocompleteEditModel* OmniboxViewViews::model() const {
  return model_.get();
}

void OmniboxViewViews::SaveStateToTab(TabContents* tab) {
  DCHECK(tab);

  // NOTE: GetStateForTabSwitch may affect GetSelection, so order is important.
  AutocompleteEditModel::State model_state = model_->GetStateForTabSwitch();
  gfx::SelectionModel selection;
  textfield_->GetSelectionModel(&selection);
  GetStateAccessor()->SetProperty(
      tab->property_bag(),
      AutocompleteEditState(model_state, ViewState(selection)));
}

void OmniboxViewViews::Update(const TabContents* contents) {
  // NOTE: We're getting the URL text here from the ToolbarModel.
  bool visibly_changed_permanent_text =
      model_->UpdatePermanentText(toolbar_model_->GetText());
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

void OmniboxViewViews::OpenMatch(const AutocompleteMatch& match,
                                 WindowOpenDisposition disposition,
                                 const GURL& alternate_nav_url,
                                 size_t selected_line,
                                 const string16& keyword) {
  if (!match.destination_url.is_valid())
    return;

  model_->OpenMatch(match, disposition, alternate_nav_url,
                    selected_line, keyword);
}

string16 OmniboxViewViews::GetText() const {
  // TODO(oshima): IME support
  return textfield_->text();
}

bool OmniboxViewViews::IsEditingOrEmpty() const {
  return model_->user_input_in_progress() || (GetTextLength() == 0);
}

int OmniboxViewViews::GetIcon() const {
  return IsEditingOrEmpty() ?
      AutocompleteMatch::TypeToIcon(model_->CurrentTextType()) :
      toolbar_model_->GetIcon();
}

void OmniboxViewViews::SetUserText(const string16& text) {
  SetUserText(text, text, true);
}

void OmniboxViewViews::SetUserText(const string16& text,
                                   const string16& display_text,
                                   bool update_popup) {
  model_->SetUserText(text);
  SetWindowTextAndCaretPos(display_text, display_text.length());
  if (update_popup)
    UpdatePopup();
  TextChanged();
}

void OmniboxViewViews::SetWindowTextAndCaretPos(const string16& text,
                                                size_t caret_pos) {
  const ui::Range range(caret_pos, caret_pos);
  SetTextAndSelectedRange(text, range);
}

void OmniboxViewViews::SetForcedQuery() {
  const string16 current_text(GetText());
  const size_t start = current_text.find_first_not_of(kWhitespaceUTF16);
  if (start == string16::npos || (current_text[start] != '?'))
    SetUserText(ASCIIToUTF16("?"));
  else
    textfield_->SelectRange(ui::Range(current_text.size(), start + 1));
}

bool OmniboxViewViews::IsSelectAll() {
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
  *start = static_cast<size_t>(range.end());
  *end = static_cast<size_t>(range.start());
}

void OmniboxViewViews::SelectAll(bool reversed) {
  if (reversed)
    textfield_->SelectRange(ui::Range(GetTextLength(), 0));
  else
    textfield_->SelectRange(ui::Range(0, GetTextLength()));
}

void OmniboxViewViews::RevertAll() {
  ClosePopup();
  model_->Revert();
  TextChanged();
}

void OmniboxViewViews::UpdatePopup() {
  model_->SetInputInProgress(true);
  if (!model_->has_focus())
    return;

  // Don't inline autocomplete when the caret/selection isn't at the end of
  // the text, or in the middle of composition.
  ui::Range sel;
  textfield_->GetSelectedRange(&sel);
  bool no_inline_autocomplete =
      sel.GetMax() < GetTextLength() || textfield_->IsIMEComposing();

  model_->StartAutocomplete(!sel.is_empty(), no_inline_autocomplete);
}

void OmniboxViewViews::ClosePopup() {
  model_->StopAutocomplete();
}

void OmniboxViewViews::SetFocus() {
  // In views-implementation, the focus is on textfield rather than OmniboxView.
  textfield_->RequestFocus();
}

void OmniboxViewViews::OnTemporaryTextMaybeChanged(
    const string16& display_text,
    bool save_original_selection) {
  if (save_original_selection)
    textfield_->GetSelectedRange(&saved_temporary_selection_);

  SetWindowTextAndCaretPos(display_text, display_text.length());
  TextChanged();
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

  const bool something_changed = model_->OnAfterPossibleChange(
      new_text, new_sel.start(), new_sel.end(), selection_differs,
      text_changed, just_deleted_text, !textfield_->IsIMEComposing());

  // If only selection was changed, we don't need to call |model_|'s
  // OnChanged() method, which is called in TextChanged().
  // But we still need to call EmphasizeURLComponents() to make sure the text
  // attributes are updated correctly.
  if (something_changed && text_changed)
    TextChanged();
  else if (selection_differs)
    EmphasizeURLComponents();
  else if (delete_at_end_pressed_)
    model_->OnChanged();

  return something_changed;
}

gfx::NativeView OmniboxViewViews::GetNativeView() const {
  return GetWidget()->GetNativeView();
}

gfx::NativeView OmniboxViewViews::GetRelativeWindowForPopup() const {
#if defined(OS_WIN) && !defined(USE_AURA)
  return OmniboxViewWin::GetRelativeWindowForNativeView(GetNativeView());
#else
  return GetWidget()->GetTopLevelWidget()->GetNativeView();
#endif
}

CommandUpdater* OmniboxViewViews::GetCommandUpdater() {
  return command_updater_;
}

void OmniboxViewViews::SetInstantSuggestion(const string16& input,
                                            bool animate_to_complete) {
#if defined(OS_WIN) || defined(USE_AURA)
  location_bar_view_->SetInstantSuggestion(input, animate_to_complete);
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

int OmniboxViewViews::OnPerformDrop(const views::DropTargetEvent& event) {
  NOTIMPLEMENTED();
  return ui::DragDropTypes::DRAG_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxViewViews, content::NotificationObserver implementation:

void OmniboxViewViews::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_THEME_CHANGED);
  SetBaseColor();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxViewViews, views::TextfieldController implementation:

void OmniboxViewViews::ContentsChanged(views::Textfield* sender,
                                       const string16& new_contents) {
}

bool OmniboxViewViews::HandleKeyEvent(views::Textfield* textfield,
                                      const views::KeyEvent& event) {
  delete_at_end_pressed_ = false;

  if (event.key_code() == ui::VKEY_BACK) {
    // Checks if it's currently in keyword search mode.
    if (model_->is_keyword_hint() || model_->keyword().empty())
      return false;
    // If there is selection, let textfield handle the backspace.
    if (textfield_->HasSelection())
      return false;
    // If not at the begining of the text, let textfield handle the backspace.
    if (textfield_->GetCursorPosition())
      return false;
    model_->ClearKeyword(GetText());
    return true;
  }

  if (event.key_code() == ui::VKEY_DELETE && !event.IsAltDown()) {
    delete_at_end_pressed_ =
        (!textfield_->HasSelection() &&
         textfield_->GetCursorPosition() == textfield_->text().length());
  }

  return false;
}

void OmniboxViewViews::OnBeforeUserAction(views::Textfield* sender) {
  OnBeforePossibleChange();
}

void OmniboxViewViews::OnAfterUserAction(views::Textfield* sender) {
  OnAfterPossibleChange();
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxViewViews, private:

size_t OmniboxViewViews::GetTextLength() const {
  // TODO(oshima): Support instant, IME.
  return textfield_->text().length();
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
  AutocompleteInput::ParseForEmphasizeComponents(text, model_->GetDesiredTLD(),
                                                 &scheme, &host);
  const bool emphasize = model_->CurrentTextIsURL() && (host.len > 0);
  SkColor base_color = emphasize ? kFadedTextColor : kNormalTextColor;
  ApplyURLStyle(textfield_, 0, text.length(), base_color, false);
  if (emphasize)
    ApplyURLStyle(textfield_, host.begin, host.end(), kNormalTextColor, false);
  // Emphasize the scheme for security UI display purposes (if necessary).
  if (!model_->user_input_in_progress() && scheme.is_nonempty() &&
      (security_level_ != ToolbarModel::NONE)) {
    const size_t start = scheme.begin, end = scheme.end();
    switch (security_level_) {
      case ToolbarModel::SECURITY_ERROR:
        ApplyURLStyle(textfield_, start, end, kSecurityErrorSchemeColor, true);
        break;
      case ToolbarModel::SECURITY_WARNING:
        ApplyURLStyle(textfield_, start, end, kFadedTextColor, false);
        break;
      case ToolbarModel::EV_SECURE:
      case ToolbarModel::SECURE:
        ApplyURLStyle(textfield_, start, end, kSecureSchemeColor, false);
        break;
      default:
        NOTREACHED() << "Unknown SecurityLevel:" << security_level_;
    }
  }
}

void OmniboxViewViews::TextChanged() {
  EmphasizeURLComponents();
  model_->OnChanged();
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

AutocompletePopupView* OmniboxViewViews::CreatePopupView(
    View* location_bar) {
#if defined(TOUCH_UI)
  typedef TouchAutocompletePopupContentsView AutocompleteContentsView;
#else
  typedef AutocompletePopupContentsView AutocompleteContentsView;
#endif
  return new AutocompleteContentsView(gfx::Font(), this, model_.get(),
                                      location_bar);
}

#if defined(USE_AURA)
// static
OmniboxView* OmniboxView::CreateOmniboxView(
    AutocompleteEditController* controller,
    ToolbarModel* toolbar_model,
    Profile* profile,
    CommandUpdater* command_updater,
    bool popup_window_mode,
    LocationBarView* location_bar) {
  OmniboxViewViews* omnibox_view = new OmniboxViewViews(controller,
                                                        toolbar_model,
                                                        profile,
                                                        command_updater,
                                                        popup_window_mode,
                                                        location_bar);
  omnibox_view->Init();
  return omnibox_view;
}
#endif
