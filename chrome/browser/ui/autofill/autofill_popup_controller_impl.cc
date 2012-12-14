// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "grit/webkit_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"
#include "ui/base/events/event.h"

using WebKit::WebAutofillClient;

namespace {

// Used to indicate that no line is currently selected by the user.
const int kNoSelection = -1;

// Size difference between value text and label text in pixels.
const int kLabelFontSizeDelta = -2;

// The vertical height of each row in pixels.
const size_t kRowHeight = 24;

// The vertical height of a separator in pixels.
const size_t kSeparatorHeight = 1;

// The amount of minimum padding between the Autofill value and label in pixels.
const size_t kLabelPadding = 15;

// The maximum amount of characters to display from either the label or value.
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

AutofillPopupControllerImpl::AutofillPopupControllerImpl(
    AutofillPopupDelegate* delegate,
    gfx::NativeView container_view,
    const gfx::Rect& element_bounds)
    : view_(NULL),
      delegate_(delegate),
      container_view_(container_view),
      element_bounds_(element_bounds),
      selected_line_(kNoSelection),
      delete_icon_hovered_(false) {
#if !defined(OS_ANDROID)
  label_font_ = value_font_.DeriveFont(kLabelFontSizeDelta);
#endif
}

AutofillPopupControllerImpl::~AutofillPopupControllerImpl() {
  if (delegate_)
    delegate_->ControllerDestroyed();
}

void AutofillPopupControllerImpl::Show(
    const std::vector<string16>& autofill_values,
    const std::vector<string16>& autofill_labels,
    const std::vector<string16>& autofill_icons,
    const std::vector<int>& autofill_unique_ids) {
  autofill_values_ = autofill_values;
  autofill_labels_ = autofill_labels;
  autofill_icons_ = autofill_icons;
  autofill_unique_ids_ = autofill_unique_ids;

#if !defined(OS_ANDROID)
  // Android displays the long text with ellipsis using the view attributes.

  // TODO(csharp): Fix crbug.com/156163 and use better logic when clipping.
  for (size_t i = 0; i < autofill_values_.size(); ++i) {
    if (autofill_values_[i].length() > 15)
      autofill_values_[i].erase(15);
    if (autofill_labels[i].length() > 15)
      autofill_labels_[i].erase(15);
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
  if (view_)
    view_->Hide();
  else
    delete this;
}

void AutofillPopupControllerImpl::DelegateDestroyed() {
  delegate_ = NULL;
  Hide();
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

void AutofillPopupControllerImpl::SetSelectedPosition(int x, int y) {
  int line = LineFromY(y);

  SetSelectedLine(line);

  bool delete_icon_hovered = DeleteIconIsUnder(x, y);
  if (delete_icon_hovered != delete_icon_hovered_) {
    delete_icon_hovered_ = delete_icon_hovered;
    InvalidateRow(selected_line());
  }
}

bool AutofillPopupControllerImpl::AcceptAutofillSuggestion(
    const string16& value,
    int unique_id,
    unsigned index) {
  return delegate_->DidAcceptAutofillSuggestion(value, unique_id, index);
}

void AutofillPopupControllerImpl::AcceptSelectedPosition(int x, int y) {
  DCHECK_EQ(selected_line(), LineFromY(y));

  if (DeleteIconIsUnder(x, y))
    RemoveSelectedLine();
  else
    AcceptSelectedLine();
}

void AutofillPopupControllerImpl::ClearSelectedLine() {
  SetSelectedLine(kNoSelection);
}

int AutofillPopupControllerImpl::GetIconResourceID(
    const string16& resource_name) {
  for (size_t i = 0; i < arraysize(kDataResources); ++i) {
    if (resource_name == ASCIIToUTF16(kDataResources[i].name))
      return kDataResources[i].id;
  }

  return -1;
}

bool AutofillPopupControllerImpl::CanDelete(int id) {
  return id > 0 ||
      id == WebAutofillClient::MenuItemIDAutocompleteEntry ||
      id == WebAutofillClient::MenuItemIDPasswordEntry;
}

#if !defined(OS_ANDROID)
int AutofillPopupControllerImpl::GetPopupRequiredWidth() {
  if (value_font_.platform_font() == NULL ||
      label_font_.platform_font() == NULL) {
    // We can't calculate the size of the popup if the fonts
    // aren't present.
    return 0;
  }

  int popup_width = element_bounds().width();
  DCHECK_EQ(autofill_values().size(), autofill_labels().size());
  for (size_t i = 0; i < autofill_values().size(); ++i) {
    int row_size = kEndPadding +
        value_font_.GetStringWidth(autofill_values()[i]) +
        kLabelPadding +
        label_font_.GetStringWidth(autofill_labels()[i]);

    // Add the Autofill icon size, if required.
    if (!autofill_icons()[i].empty())
      row_size += kAutofillIconWidth + kIconPadding;

    // Add delete icon, if required.
    if (CanDelete(autofill_unique_ids()[i]))
      row_size += kDeleteIconWidth + kIconPadding;

    // Add the padding at the end
    row_size += kEndPadding;

    popup_width = std::max(popup_width, row_size);
  }

  return popup_width;
}

int AutofillPopupControllerImpl::GetPopupRequiredHeight() {
  int popup_height = 0;

  for (size_t i = 0; i < autofill_unique_ids().size(); ++i) {
    popup_height += GetRowHeightFromId(autofill_unique_ids()[i]);
  }

  return popup_height;
}
#endif  // !defined(OS_ANDROID)

int AutofillPopupControllerImpl::GetRowHeightFromId(int unique_id) {
  if (unique_id == WebAutofillClient::MenuItemIDSeparator)
    return kSeparatorHeight;

  return kRowHeight;
}

gfx::Rect AutofillPopupControllerImpl::GetRectForRow(size_t row, int width) {
  int top = 0;
  for (size_t i = 0; i < row; ++i) {
    top += GetRowHeightFromId(autofill_unique_ids()[i]);
  }

  return gfx::Rect(
      0, top, width, GetRowHeightFromId(autofill_unique_ids()[row]));
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

const std::vector<string16>& AutofillPopupControllerImpl::
    autofill_values() const {
  return autofill_values_;
}

const std::vector<string16>& AutofillPopupControllerImpl::
    autofill_labels() const {
  return autofill_labels_;
}

const std::vector<string16>& AutofillPopupControllerImpl::
    autofill_icons() const {
  return autofill_icons_;
}

const std::vector<int>& AutofillPopupControllerImpl::
    autofill_unique_ids() const {
  return autofill_unique_ids_;
}

#if !defined(OS_ANDROID)
const gfx::Font& AutofillPopupControllerImpl::label_font() const {
  return label_font_;
}

const gfx::Font& AutofillPopupControllerImpl::value_font() const {
  return value_font_;
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
      SetSelectedLine(autofill_values().size() - 1);
      return true;
    case ui::VKEY_ESCAPE:
      Hide();
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

void AutofillPopupControllerImpl::SetSelectedLine(int selected_line) {
  if (selected_line_ == selected_line)
    return;

  if (selected_line_ != kNoSelection)
    InvalidateRow(selected_line_);

  if (selected_line != kNoSelection)
    InvalidateRow(selected_line);

  selected_line_ = selected_line;

  if (selected_line_ != kNoSelection) {
    delegate_->SelectAutofillSuggestionAtIndex(
        autofill_unique_ids_[selected_line_]);
  }
}

void AutofillPopupControllerImpl::SelectNextLine() {
  int new_selected_line = selected_line_ + 1;

  // Skip over any lines that can't be selected.
  while (static_cast<size_t>(new_selected_line) < autofill_values_.size() &&
         !CanAccept(autofill_unique_ids()[new_selected_line])) {
    ++new_selected_line;
  }

  if (new_selected_line == static_cast<int>(autofill_values_.size()))
    new_selected_line = 0;

  SetSelectedLine(new_selected_line);
}

void AutofillPopupControllerImpl::SelectPreviousLine() {
  int new_selected_line = selected_line_ - 1;

  // Skip over any lines that can't be selected.
  while (new_selected_line > kNoSelection &&
         !CanAccept(autofill_unique_ids()[new_selected_line])) {
    --new_selected_line;
  }

  if (new_selected_line <= kNoSelection)
    new_selected_line = autofill_values_.size() - 1;

  SetSelectedLine(new_selected_line);
}

bool AutofillPopupControllerImpl::AcceptSelectedLine() {
  if (selected_line_ == kNoSelection)
    return false;

  DCHECK_GE(selected_line_, 0);
  DCHECK_LT(selected_line_, static_cast<int>(autofill_values_.size()));

  if (!CanAccept(autofill_unique_ids_[selected_line_]))
    return false;

  return AcceptAutofillSuggestion(
      autofill_values_[selected_line_],
      autofill_unique_ids_[selected_line_],
      selected_line_);
}

bool AutofillPopupControllerImpl::RemoveSelectedLine() {
  if (selected_line_ == kNoSelection)
    return false;

  DCHECK_GE(selected_line_, 0);
  DCHECK_LT(selected_line_, static_cast<int>(autofill_values_.size()));

  if (!CanDelete(autofill_unique_ids_[selected_line_]))
    return false;

  if (autofill_unique_ids_[selected_line_] > 0) {
    delegate_->RemoveAutofillProfileOrCreditCard(
        autofill_unique_ids_[selected_line_]);
  } else {
    delegate_->RemoveAutocompleteEntry(
        autofill_values_[selected_line_]);
  }

  // Remove the deleted element.
  autofill_values_.erase(autofill_values_.begin() + selected_line_);
  autofill_labels_.erase(autofill_labels_.begin() + selected_line_);
  autofill_icons_.erase(autofill_icons_.begin() + selected_line_);
  autofill_unique_ids_.erase(autofill_unique_ids_.begin() + selected_line_);

  SetSelectedLine(kNoSelection);

  if (HasAutofillEntries()) {
    delegate_->ClearPreviewedForm();
    UpdateBoundsAndRedrawPopup();
  } else {
    Hide();
  }

  return true;
}

int AutofillPopupControllerImpl::LineFromY(int y) {
  int current_height = 0;

  for (size_t i = 0; i < autofill_unique_ids().size(); ++i) {
    current_height += GetRowHeightFromId(autofill_unique_ids()[i]);

    if (y <= current_height)
      return i;
  }

  // The y value goes beyond the popup so stop the selection at the last line.
  return autofill_unique_ids().size() - 1;
}

bool AutofillPopupControllerImpl::DeleteIconIsUnder(int x, int y) {
#if defined(OS_ANDROID)
  return false;
#else
  if (!CanDelete(selected_line()))
    return false;

  int row_start_y = 0;
  for (int i = 0; i < selected_line(); ++i) {
    row_start_y += GetRowHeightFromId(autofill_unique_ids()[i]);
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
  return id != WebAutofillClient::MenuItemIDSeparator;
}

bool AutofillPopupControllerImpl::HasAutofillEntries() {
  return autofill_values_.size() != 0 &&
      (autofill_unique_ids_[0] > 0 ||
       autofill_unique_ids_[0] ==
           WebAutofillClient::MenuItemIDAutocompleteEntry ||
       autofill_unique_ids_[0] == WebAutofillClient::MenuItemIDPasswordEntry ||
       autofill_unique_ids_[0] == WebAutofillClient::MenuItemIDDataListEntry);
}

void AutofillPopupControllerImpl::ShowView() {
  view_->Show();
}

void AutofillPopupControllerImpl::InvalidateRow(size_t row) {
  view_->InvalidateRow(row);
}
