// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/migratable_card_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_state.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace autofill {

constexpr char MigratableCardView::kViewClassName[] = "MigratableCardView";

MigratableCardView::MigratableCardView(
    const MigratableCreditCard& migratable_credit_card,
    views::ButtonListener* listener,
    bool should_show_checkbox)
    : migratable_credit_card_(migratable_credit_card) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL)));
  constexpr int kMigrationResultImageSize = 16;

  std::unique_ptr<views::Label> card_description =
      std::make_unique<views::Label>(
          migratable_credit_card.credit_card().NetworkAndLastFourDigits(),
          views::style::CONTEXT_LABEL);

  switch (migratable_credit_card.migration_status()) {
    case MigratableCreditCard::MigrationStatus::UNKNOWN: {
      if (should_show_checkbox) {
        DCHECK(listener);
        checkbox_ = new views::Checkbox(base::string16(), listener);
        checkbox_->SetChecked(true);
        // TODO(crbug/867194): Currently the ink drop animation circle is
        // cropped by the border of scroll bar view. Find a way to adjust the
        // format.
        checkbox_->SetInkDropMode(views::InkDropHostView::InkDropMode::OFF);
        checkbox_->SetAssociatedLabel(card_description.get());
        AddChildView(checkbox_);
      }
      break;
    }
    case MigratableCreditCard::MigrationStatus::SUCCESS_ON_UPLOAD: {
      auto* migration_succeeded_image = new views::ImageView();
      migration_succeeded_image->SetImage(gfx::CreateVectorIcon(
          vector_icons::kCheckCircleIcon, kMigrationResultImageSize,
          gfx::kGoogleGreen700));
      AddChildView(migration_succeeded_image);
      break;
    }
    case MigratableCreditCard::MigrationStatus::FAILURE_ON_UPLOAD: {
      auto* migration_failed_image = new views::ImageView();
      migration_failed_image->SetImage(
          gfx::CreateVectorIcon(kBrowserToolsErrorIcon,
                                kMigrationResultImageSize, gfx::kGoogleRed700));
      AddChildView(migration_failed_image);
      break;
    }
  }

  std::unique_ptr<views::View> card_network_and_last_four_digits =
      std::make_unique<views::View>();
  card_network_and_last_four_digits->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, gfx::Insets(),
          provider->GetDistanceMetric(DISTANCE_RELATED_LABEL_HORIZONTAL_LIST)));

  std::unique_ptr<views::ImageView> card_image =
      std::make_unique<views::ImageView>();
  card_image->SetImage(
      rb.GetImageNamed(CreditCard::IconResourceId(
                           migratable_credit_card.credit_card().network()))
          .AsImageSkia());
  card_image->SetAccessibleName(
      migratable_credit_card.credit_card().NetworkForDisplay());
  card_network_and_last_four_digits->AddChildView(card_image.release());
  card_network_and_last_four_digits->AddChildView(card_description.release());
  AddChildView(card_network_and_last_four_digits.release());

  std::unique_ptr<views::Label> card_expiration =
      std::make_unique<views::Label>(
          migratable_credit_card.credit_card()
              .AbbreviatedExpirationDateForDisplay(/*with_prefix=*/true),
          views::style::CONTEXT_LABEL, ChromeTextStyle::STYLE_SECONDARY);
  AddChildView(card_expiration.release());

  // If card is not successfully uploaded we create the trash can icon.
  if (migratable_credit_card.migration_status() ==
      MigratableCreditCard::MigrationStatus::FAILURE_ON_UPLOAD) {
    // TODO(crbug.com/867194): Add the invalid string for failed card, and
    // update the above comment.

    DCHECK(listener);
    delete_card_from_local_button_ = views::CreateVectorImageButton(listener);
    views::SetImageFromVectorIcon(delete_card_from_local_button_,
                                  kTrashCanIcon);
    // TODO(crbug.com/867194): Add tooltip and tag for the
    // delete_card_from_local_button_, and then set it visible by default.
    delete_card_from_local_button_->SetVisible(false);
    AddChildView(delete_card_from_local_button_);
  }
}

MigratableCardView::~MigratableCardView() = default;

bool MigratableCardView::IsSelected() {
  return !checkbox_ || checkbox_->checked();
}

std::string MigratableCardView::GetGuid() {
  return migratable_credit_card_.credit_card().guid();
}

const char* MigratableCardView::GetClassName() const {
  return kViewClassName;
}

}  // namespace autofill
