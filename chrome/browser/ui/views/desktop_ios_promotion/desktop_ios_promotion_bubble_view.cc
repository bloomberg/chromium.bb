// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_ios_promotion/desktop_ios_promotion_bubble_view.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/desktop_ios_promotion/desktop_ios_promotion_controller.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/grid_layout.h"

namespace {
// Returns the appropriate size for the promotion text label on the bubble.
int GetPromoBubbleTextLabelWidth(
    desktop_ios_promotion::PromotionEntryPoint entry_point) {
  if (entry_point ==
      desktop_ios_promotion::PromotionEntryPoint::SAVE_PASSWORD_BUBBLE) {
    return views::Widget::GetLocalizedContentsWidth(
        IDS_DESKTOP_IOS_PROMOTION_SAVE_PASSWORDS_BUBBLE_TEXT_WIDTH_CHARS);
  }
  return views::Widget::GetLocalizedContentsWidth(
      IDS_DESKTOP_IOS_PROMOTION_TEXT_WIDTH_CHARS);
}
}  // namespace

DesktopIOSPromotionBubbleView::DesktopIOSPromotionBubbleView(
    Profile* profile,
    desktop_ios_promotion::PromotionEntryPoint entry_point)
    : promotion_text_label_(
          new views::Label(desktop_ios_promotion::GetPromoText(entry_point))),
      promotion_controller_(
          base::MakeUnique<DesktopIOSPromotionController>(profile,
                                                          this,
                                                          entry_point)) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetBorder(views::CreateEmptyBorder(
      0,
      provider->GetDistanceMetric(DISTANCE_PANEL_CONTENT_MARGIN) +
          desktop_ios_promotion::GetPromoImage(
              GetNativeTheme()->GetSystemColor(
                  ui::NativeTheme::kColorId_TextfieldDefaultColor))
              .width(),
      0, 0));
  send_sms_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this, l10n_util::GetStringUTF16(IDS_DESKTOP_TO_IOS_PROMO_SEND_TO_PHONE));
  no_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_DESKTOP_TO_IOS_PROMO_NO_THANKS));
  constexpr int kLabelColumnSet = 1;
  views::ColumnSet* column_set = layout->AddColumnSet(kLabelColumnSet);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  constexpr int kDoubleButtonColumnSet = 2;
  column_set = layout->AddColumnSet(kDoubleButtonColumnSet);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                        1, views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(
      0,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                        0, views::GridLayout::USE_PREF, 0, 0);
  promotion_text_label_->SetEnabledColor(SK_ColorGRAY);
  promotion_text_label_->SetMultiLine(true);
  promotion_text_label_->SizeToFit(GetPromoBubbleTextLabelWidth(entry_point));
  promotion_text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRow(0, kLabelColumnSet);
  layout->AddView(promotion_text_label_);
  layout->AddPaddingRow(
      0, provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_VERTICAL));
  layout->StartRow(0, kDoubleButtonColumnSet);
  layout->AddView(send_sms_button_);
  layout->AddView(no_button_);
  promotion_controller_->OnPromotionShown();
}

DesktopIOSPromotionBubbleView::~DesktopIOSPromotionBubbleView() = default;

void DesktopIOSPromotionBubbleView::ButtonPressed(views::Button* sender,
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

void DesktopIOSPromotionBubbleView::UpdateBubbleHeight() {
  gfx::Rect old_bounds = GetWidget()->GetWindowBoundsInScreen();
  old_bounds.set_height(
      GetWidget()->GetRootView()->GetHeightForWidth(old_bounds.width()));
  GetWidget()->SetBounds(old_bounds);
}

void DesktopIOSPromotionBubbleView::UpdateRecoveryPhoneLabel() {
  std::string number = promotion_controller_->GetUsersRecoveryPhoneNumber();
  if (!number.empty()) {
    promotion_text_label_->SetText(desktop_ios_promotion::GetPromoText(
        promotion_controller_->entry_point(), number));
    Layout();
    UpdateBubbleHeight();
  }
}
