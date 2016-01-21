// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"

#include <algorithm>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/common/autofill_util.h"
#include "grit/components_scaled_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace autofill {

namespace {

// The vertical height of each row in pixels.
const size_t kRowHeight = 24;

// The vertical height of a separator in pixels.
const size_t kSeparatorHeight = 1;

const struct {
  const char* name;
  int id;
} kDataResources[] = {
    {"americanExpressCC", IDR_AUTOFILL_CC_AMEX},
    {"dinersCC", IDR_AUTOFILL_CC_GENERIC},
    {"discoverCC", IDR_AUTOFILL_CC_DISCOVER},
    {"genericCC", IDR_AUTOFILL_CC_GENERIC},
    {"jcbCC", IDR_AUTOFILL_CC_GENERIC},
    {"masterCardCC", IDR_AUTOFILL_CC_MASTERCARD},
    {"visaCC", IDR_AUTOFILL_CC_VISA},
#if defined(OS_ANDROID)
    {"scanCreditCardIcon", IDR_AUTOFILL_CC_SCAN_NEW},
    {"settings", IDR_AUTOFILL_SETTINGS},
#endif
};

int GetRowHeightFromId(int identifier) {
  if (identifier == POPUP_ITEM_ID_SEPARATOR)
    return kSeparatorHeight;

  return kRowHeight;
}

}  // namespace

AutofillPopupLayoutModel::AutofillPopupLayoutModel(
    AutofillPopupViewDelegate* delegate)
    : delegate_(delegate) {}

#if !defined(OS_ANDROID)
int AutofillPopupLayoutModel::GetDesiredPopupHeight() const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();
  int popup_height = 2 * kPopupBorderThickness;

  for (size_t i = 0; i < suggestions.size(); ++i) {
    popup_height += GetRowHeightFromId(suggestions[i].frontend_id);
  }

  return popup_height;
}

int AutofillPopupLayoutModel::GetDesiredPopupWidth() const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();

  int popup_width = RoundedElementBounds().width();

  for (size_t i = 0; i < suggestions.size(); ++i) {
    int label_size = delegate_->GetElidedLabelWidthForRow(i);
    int row_size = delegate_->GetElidedValueWidthForRow(i) + label_size +
                   RowWidthWithoutText(i, /* with_label= */ label_size > 0);

    popup_width = std::max(popup_width, row_size);
  }

  return popup_width;
}

int AutofillPopupLayoutModel::RowWidthWithoutText(int row,
                                                  bool with_label) const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();

  int row_size = kEndPadding;

  if (with_label)
    row_size += kNamePadding;

  // Add the Autofill icon size, if required.
  const base::string16& icon = suggestions[row].icon;
  if (!icon.empty()) {
    int icon_width = ui::ResourceBundle::GetSharedInstance()
                         .GetImageNamed(GetIconResourceID(icon))
                         .Width();
    row_size += icon_width + kIconPadding;
  }

  // Add the padding at the end.
  row_size += kEndPadding;

  // Add room for the popup border.
  row_size += 2 * kPopupBorderThickness;

  return row_size;
}

int AutofillPopupLayoutModel::GetAvailableWidthForRow(int row,
                                                      bool with_label) const {
  return popup_bounds_.width() - RowWidthWithoutText(row, with_label);
}

void AutofillPopupLayoutModel::UpdatePopupBounds() {
  int popup_width = GetDesiredPopupWidth();
  int popup_height = GetDesiredPopupHeight();

  popup_bounds_ = view_common_.CalculatePopupBounds(
      popup_width, popup_height, RoundedElementBounds(),
      delegate_->container_view(), delegate_->IsRTL());
}
#endif

int AutofillPopupLayoutModel::LineFromY(int y) const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();
  int current_height = kPopupBorderThickness;

  for (size_t i = 0; i < suggestions.size(); ++i) {
    current_height += GetRowHeightFromId(suggestions[i].frontend_id);

    if (y <= current_height)
      return i;
  }

  // The y value goes beyond the popup so stop the selection at the last line.
  return suggestions.size() - 1;
}

gfx::Rect AutofillPopupLayoutModel::GetRowBounds(size_t index) const {
  std::vector<autofill::Suggestion> suggestions = delegate_->GetSuggestions();

  int top = kPopupBorderThickness;
  for (size_t i = 0; i < index; ++i) {
    top += GetRowHeightFromId(suggestions[i].frontend_id);
  }

  return gfx::Rect(kPopupBorderThickness, top,
                   popup_bounds_.width() - 2 * kPopupBorderThickness,
                   GetRowHeightFromId(suggestions[index].frontend_id));
}

int AutofillPopupLayoutModel::GetIconResourceID(
    const base::string16& resource_name) const {
  int result = -1;
  for (size_t i = 0; i < arraysize(kDataResources); ++i) {
    if (resource_name == base::ASCIIToUTF16(kDataResources[i].name)) {
      result = kDataResources[i].id;
      break;
    }
  }

#if defined(OS_ANDROID) && !defined(USE_AURA)
  if (result == IDR_AUTOFILL_CC_SCAN_NEW && IsKeyboardAccessoryEnabled())
    result = IDR_AUTOFILL_CC_SCAN_NEW_KEYBOARD_ACCESSORY;
#endif

  return result;
}

const gfx::Rect AutofillPopupLayoutModel::RoundedElementBounds() const {
  return gfx::ToEnclosingRect(delegate_->element_bounds());
}

}  // namespace autofill
