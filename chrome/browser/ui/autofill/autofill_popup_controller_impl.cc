// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_delegate.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "grit/webkit_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"
#include "ui/base/events/event.h"

using WebKit::WebAutofillClient;

namespace {

// Used to indicate that no line is currently selected by the user.
const int kNoSelection = -1;

// Size difference between name and subtext in pixels.
const int kLabelFontSizeDelta = -2;

// The vertical height of each row in pixels.
const size_t kRowHeight = 24;

// The vertical height of a separator in pixels.
const size_t kSeparatorHeight = 1;

// The amount of minimum padding between the Autofill name and subtext in
// pixels.
const size_t kNamePadding = 15;

// The maximum amount of characters to display from either the name or subtext.
const size_t kMaxTextLength = 15;

#if !defined(OS_ANDROID)
const size_t kIconPadding = AutofillPopupView::kIconPadding;
const size_t kEndPadding = AutofillPopupView::kEndPadding;
const size_t kDeleteIconHeight = AutofillPopupView::kDeleteIconHeight;
const size_t kDeleteIconWidth = AutofillPopupView::kDeleteIconWidth;
const size_t kAutofillIconWidth = AutofillPopupView::kAutofillIconWidth;
#endif

struct DataResource {
  const char* name;
  int id;
};

const DataResource kDataResources[] = {
  { "americanExpressCC", IDR_AUTOFILL_CC_AMEX },
  { "dinersCC", IDR_AUTOFILL_CC_DINERS },
  { "discoverCC", IDR_AUTOFILL_CC_DISCOVER },
  { "genericCC", IDR_AUTOFILL_CC_GENERIC },
  { "jcbCC", IDR_AUTOFILL_CC_JCB },
  { "masterCardCC", IDR_AUTOFILL_CC_MASTERCARD },
  { "soloCC", IDR_AUTOFILL_CC_SOLO },
  { "visaCC", IDR_AUTOFILL_CC_VISA },
};

}  // end namespace

// static
AutofillPopupControllerImpl* AutofillPopupControllerImpl::GetOrCreate(
    AutofillPopupControllerImpl* previous,
    AutofillPopupDelegate* delegate,
    gfx::NativeView container_view,
    const gfx::Rect& element_bounds) {
  DCHECK(!previous || previous->delegate_ == delegate);

  if (previous &&
      previous->container_view() == container_view &&
      previous->element_bounds() == element_bounds) {
    return previous;
  }

  if (previous)
    previous->Hide();

  return new AutofillPopupControllerImpl(
      delegate, container_view, element_bounds);
}

AutofillPopupControllerImpl::AutofillPopupControllerImpl(
    AutofillPopupDelegate* delegate,
    gfx::NativeView container_view,
    const gfx::Rect& element_bounds)
    : view_(NULL),
      delegate_(delegate),
      container_view_(container_view),
      element_bounds_(element_bounds),
      selected_line_(kNoSelection),
      delete_icon_hovered_(false),
      is_hiding_(false),
      inform_delegate_of_destruction_(true) {
#if !defined(OS_ANDROID)
  subtext_font_ = name_font_.DeriveFont(kLabelFontSizeDelta);
#endif
}

AutofillPopupControllerImpl::~AutofillPopupControllerImpl() {
  if (inform_delegate_of_destruction_)
    delegate_->ControllerDestroyed();
}

void AutofillPopupControllerImpl::Show(
    const std::vector<string16>& names,
    const std::vector<string16>& subtexts,
    const std::vector<string16>& icons,
    const std::vector<int>& identifiers) {
  names_ = names;
  subtexts_ = subtexts;
  icons_ = icons;
  identifiers_ = identifiers;

#if !defined(OS_ANDROID)
  // Android displays the long text with ellipsis using the view attributes.

  // TODO(csharp): Fix crbug.com/156163 and use better logic when clipping.
  for (size_t i = 0; i < names_.size(); ++i) {
    if (names_[i].length() > 15)
      names_[i].erase(15);
    if (subtexts[i].length() > 15)
      subtexts_[i].erase(15);
  }
#endif

  if (!view_) {
    view_ = AutofillPopupView::Create(this);
    ShowView();
  } else {
    UpdateBoundsAndRedrawPopup();
  }
}

void AutofillPopupControllerImpl::Hide() {
  inform_delegate_of_destruction_ = false;
  HideInternal();
}

void AutofillPopupControllerImpl::ViewDestroyed() {
  delete this;
}

void AutofillPopupControllerImpl::UpdateBoundsAndRedrawPopup() {
#if !defined(OS_ANDROID)
  popup_bounds_.set_width(GetPopupRequiredWidth());
  popup_bounds_.set_height(GetPopupRequiredHeight());
#endif

  view_->UpdateBoundsAndRedrawPopup();
}

void AutofillPopupControllerImpl::MouseHovered(int x, int y) {
  SetSelectedLine(LineFromY(y));

  bool delete_icon_hovered = DeleteIconIsUnder(x, y);
  if (delete_icon_hovered != delete_icon_hovered_) {
    delete_icon_hovered_ = delete_icon_hovered;
    InvalidateRow(selected_line());
  }
}

void AutofillPopupControllerImpl::MouseClicked(int x, int y) {
  MouseHovered(x, y);

  if (delete_icon_hovered_)
    RemoveSelectedLine();
  else
    AcceptSelectedLine();
}

void AutofillPopupControllerImpl::MouseExitedPopup() {
  SetSelectedLine(kNoSelection);
}

void AutofillPopupControllerImpl::AcceptSuggestion(size_t index) {
  delegate_->DidAcceptSuggestion(names_[index], identifiers_[index]);
}

int AutofillPopupControllerImpl::GetIconResourceID(
    const string16& resource_name) {
  for (size_t i = 0; i < arraysize(kDataResources); ++i) {
    if (resource_name == ASCIIToUTF16(kDataResources[i].name))
      return kDataResources[i].id;
  }

  return -1;
}

bool AutofillPopupControllerImpl::CanDelete(size_t index) {
  // TODO(isherman): AddressBook suggestions on Mac should not be drawn as
  // deleteable.
  int id = identifiers_[index];
  return id > 0 ||
      id == WebAutofillClient::MenuItemIDAutocompleteEntry ||
      id == WebAutofillClient::MenuItemIDPasswordEntry;
}

#if !defined(OS_ANDROID)
int AutofillPopupControllerImpl::GetPopupRequiredWidth() {
  if (name_font_.platform_font() == NULL ||
      subtext_font_.platform_font() == NULL) {
    // We can't calculate the size of the popup if the fonts
    // aren't present.
    return 0;
  }

  int popup_width = element_bounds().width();
  DCHECK_EQ(names().size(), subtexts().size());
  for (size_t i = 0; i < names().size(); ++i) {
    int row_size = kEndPadding +
        name_font_.GetStringWidth(names()[i]) +
        kNamePadding +
        subtext_font_.GetStringWidth(subtexts()[i]);

    // Add the Autofill icon size, if required.
    if (!icons()[i].empty())
      row_size += kAutofillIconWidth + kIconPadding;

    // Add delete icon, if required.
    if (CanDelete(i))
      row_size += kDeleteIconWidth + kIconPadding;

    // Add the padding at the end
    row_size += kEndPadding;

    popup_width = std::max(popup_width, row_size);
  }

  return popup_width;
}

int AutofillPopupControllerImpl::GetPopupRequiredHeight() {
  int popup_height = 0;

  for (size_t i = 0; i < identifiers().size(); ++i) {
    popup_height += GetRowHeightFromId(identifiers()[i]);
  }

  return popup_height;
}
#endif  // !defined(OS_ANDROID)

gfx::Rect AutofillPopupControllerImpl::GetRowBounds(size_t index) {
  int top = 0;
  for (size_t i = 0; i < index; ++i) {
    top += GetRowHeightFromId(identifiers()[i]);
  }

  return gfx::Rect(
      0,
      top,
      popup_bounds_.width(),
      GetRowHeightFromId(identifiers()[index]));
}

void AutofillPopupControllerImpl::SetPopupBounds(const gfx::Rect& bounds) {
  popup_bounds_ = bounds;
  UpdateBoundsAndRedrawPopup();
}

const gfx::Rect& AutofillPopupControllerImpl::popup_bounds() const {
  return popup_bounds_;
}

gfx::NativeView AutofillPopupControllerImpl::container_view() const {
  return container_view_;
}

const gfx::Rect& AutofillPopupControllerImpl::element_bounds() const {
  return element_bounds_;
}

const std::vector<string16>& AutofillPopupControllerImpl::names() const {
  return names_;
}

const std::vector<string16>& AutofillPopupControllerImpl::subtexts() const {
  return subtexts_;
}

const std::vector<string16>& AutofillPopupControllerImpl::icons() const {
  return icons_;
}

const std::vector<int>& AutofillPopupControllerImpl::identifiers() const {
  return identifiers_;
}

#if !defined(OS_ANDROID)
const gfx::Font& AutofillPopupControllerImpl::name_font() const {
  return name_font_;
}

const gfx::Font& AutofillPopupControllerImpl::subtext_font() const {
  return subtext_font_;
}
#endif

int AutofillPopupControllerImpl::selected_line() const {
  return selected_line_;
}

bool AutofillPopupControllerImpl::delete_icon_hovered() const {
  return delete_icon_hovered_;
}

bool AutofillPopupControllerImpl::HandleKeyPressEvent(
    const content::NativeWebKeyboardEvent& event) {
  switch (event.windowsKeyCode) {
    case ui::VKEY_UP:
      SelectPreviousLine();
      return true;
    case ui::VKEY_DOWN:
      SelectNextLine();
      return true;
    case ui::VKEY_PRIOR:  // Page up.
      SetSelectedLine(0);
      return true;
    case ui::VKEY_NEXT:  // Page down.
      SetSelectedLine(names().size() - 1);
      return true;
    case ui::VKEY_ESCAPE:
      HideInternal();
      return true;
    case ui::VKEY_DELETE:
      return (event.modifiers & content::NativeWebKeyboardEvent::ShiftKey) &&
             RemoveSelectedLine();
    case ui::VKEY_RETURN:
      return AcceptSelectedLine();
    default:
      return false;
  }
}

void AutofillPopupControllerImpl::HideInternal() {
  if (is_hiding_)
    return;
  is_hiding_ = true;

  SetSelectedLine(kNoSelection);

  if (view_)
    view_->Hide();
  else
    delete this;
}

void AutofillPopupControllerImpl::SetSelectedLine(int selected_line) {
  if (selected_line_ == selected_line)
    return;

  if (selected_line_ != kNoSelection)
    InvalidateRow(selected_line_);

  if (selected_line != kNoSelection)
    InvalidateRow(selected_line);

  selected_line_ = selected_line;

  if (selected_line_ != kNoSelection)
    delegate_->DidSelectSuggestion(identifiers_[selected_line_]);
  else
    delegate_->ClearPreviewedForm();
}

void AutofillPopupControllerImpl::SelectNextLine() {
  int new_selected_line = selected_line_ + 1;

  // Skip over any lines that can't be selected.
  while (static_cast<size_t>(new_selected_line) < names_.size() &&
         !CanAccept(identifiers()[new_selected_line])) {
    ++new_selected_line;
  }

  if (new_selected_line == static_cast<int>(names_.size()))
    new_selected_line = 0;

  SetSelectedLine(new_selected_line);
}

void AutofillPopupControllerImpl::SelectPreviousLine() {
  int new_selected_line = selected_line_ - 1;

  // Skip over any lines that can't be selected.
  while (new_selected_line > kNoSelection &&
         !CanAccept(identifiers()[new_selected_line])) {
    --new_selected_line;
  }

  if (new_selected_line <= kNoSelection)
    new_selected_line = names_.size() - 1;

  SetSelectedLine(new_selected_line);
}

bool AutofillPopupControllerImpl::AcceptSelectedLine() {
  if (selected_line_ == kNoSelection)
    return false;

  DCHECK_GE(selected_line_, 0);
  DCHECK_LT(selected_line_, static_cast<int>(names_.size()));

  if (!CanAccept(identifiers_[selected_line_]))
    return false;

  AcceptSuggestion(selected_line_);
  return true;
}

bool AutofillPopupControllerImpl::RemoveSelectedLine() {
  if (selected_line_ == kNoSelection)
    return false;

  DCHECK_GE(selected_line_, 0);
  DCHECK_LT(selected_line_, static_cast<int>(names_.size()));

  if (!CanDelete(selected_line_))
    return false;

  delegate_->RemoveSuggestion(names_[selected_line_],
                              identifiers_[selected_line_]);

  // Remove the deleted element.
  names_.erase(names_.begin() + selected_line_);
  subtexts_.erase(subtexts_.begin() + selected_line_);
  icons_.erase(icons_.begin() + selected_line_);
  identifiers_.erase(identifiers_.begin() + selected_line_);

  SetSelectedLine(kNoSelection);

  if (HasSuggestions()) {
    delegate_->ClearPreviewedForm();
    UpdateBoundsAndRedrawPopup();
  } else {
    HideInternal();
  }

  return true;
}

int AutofillPopupControllerImpl::LineFromY(int y) {
  int current_height = 0;

  for (size_t i = 0; i < identifiers().size(); ++i) {
    current_height += GetRowHeightFromId(identifiers()[i]);

    if (y <= current_height)
      return i;
  }

  // The y value goes beyond the popup so stop the selection at the last line.
  return identifiers().size() - 1;
}

int AutofillPopupControllerImpl::GetRowHeightFromId(int identifier) {
  if (identifier == WebAutofillClient::MenuItemIDSeparator)
    return kSeparatorHeight;

  return kRowHeight;
}

bool AutofillPopupControllerImpl::DeleteIconIsUnder(int x, int y) {
#if defined(OS_ANDROID)
  return false;
#else
  if (!CanDelete(selected_line()))
    return false;

  int row_start_y = 0;
  for (int i = 0; i < selected_line(); ++i) {
    row_start_y += GetRowHeightFromId(identifiers()[i]);
  }

  gfx::Rect delete_icon_bounds = gfx::Rect(
      GetPopupRequiredWidth() - kDeleteIconWidth - kIconPadding,
      row_start_y + ((kRowHeight - kDeleteIconHeight) / 2),
      kDeleteIconWidth,
      kDeleteIconHeight);

  return delete_icon_bounds.Contains(x, y);
#endif
}

bool AutofillPopupControllerImpl::CanAccept(int id) {
  return id != WebAutofillClient::MenuItemIDSeparator &&
      id != WebAutofillClient::MenuItemIDWarningMessage;
}

bool AutofillPopupControllerImpl::HasSuggestions() {
  return identifiers_.size() != 0 &&
      (identifiers_[0] > 0 ||
       identifiers_[0] ==
           WebAutofillClient::MenuItemIDAutocompleteEntry ||
       identifiers_[0] == WebAutofillClient::MenuItemIDPasswordEntry ||
       identifiers_[0] == WebAutofillClient::MenuItemIDDataListEntry);
}

void AutofillPopupControllerImpl::ShowView() {
  view_->Show();
}

void AutofillPopupControllerImpl::InvalidateRow(size_t row) {
  view_->InvalidateRow(row);
}
