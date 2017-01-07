// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_ios_promotion/desktop_ios_promotion_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_controller.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

int GetDesiredBubbleMaxWidth(
    desktop_ios_promotion::PromotionEntryPoint entry_point) {
  if (entry_point ==
      desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE) {
    return ManagePasswordsBubbleView::kDesiredBubbleWidth;
  }
  return 0;
}

}  // namespace

DesktopIOSPromotionView::DesktopIOSPromotionView(
    desktop_ios_promotion::PromotionEntryPoint entry_point) {
  promotion_controller_ = new DesktopIOSPromotionController();
  int bubbleWidth = ::GetDesiredBubbleMaxWidth(entry_point);
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(bubbleWidth, 0));
  SetLayoutManager(layout);
  send_sms_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this, l10n_util::GetStringUTF16(IDS_DESKTOP_TO_IOS_PROMO_SEND_TO_PHONE));
  no_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_DESKTOP_TO_IOS_PROMO_NO_THANKS));

  constexpr int kLabelColumnSet = 1;
  views::ColumnSet* column_set = layout->AddColumnSet(kLabelColumnSet);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::FIXED, bubbleWidth, 0);

  constexpr int kDoubleButtonColumnSet = 2;
  column_set = layout->AddColumnSet(kDoubleButtonColumnSet);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                        1, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                        0, views::GridLayout::USE_PREF, 0, 0);

  views::Label* label =
      new views::Label(desktop_ios_promotion::GetPromoBubbleText(entry_point));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRow(0, kLabelColumnSet);
  layout->AddView(label);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
  layout->StartRow(0, kDoubleButtonColumnSet);
  layout->AddView(send_sms_button_);
  layout->AddView(no_button_);

  // TODO(crbug.com/676655): Log impression.
}

void DesktopIOSPromotionView::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  if (sender == send_sms_button_) {
    promotion_controller_->OnSendSMSClicked();
  } else if (sender == no_button_) {
    promotion_controller_->OnNoThanksClicked();
  } else {
    NOTREACHED();
  }
  GetWidget()->Close();
}
