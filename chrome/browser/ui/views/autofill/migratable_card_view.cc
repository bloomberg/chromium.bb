// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/migratable_card_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_state.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/grit/components_scaled_resources.h"
#include "components/vector_icons/vector_icons.h"
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
    int card_index)
    : migratable_credit_card_(migratable_credit_card) {
  Init(migratable_credit_card, listener, card_index);
}

MigratableCardView::~MigratableCardView() = default;

bool MigratableCardView::IsSelected() {
  DCHECK(checkbox_);
  return checkbox_->checked();
}

std::string MigratableCardView::GetGuid() {
  return migratable_credit_card_.credit_card().guid();
}

void MigratableCardView::SetCheckboxEnabled(bool checkbox_enabled) {
  checkbox_->SetEnabled(checkbox_enabled);
}

void MigratableCardView::UpdateCardView(
    LocalCardMigrationDialogState dialog_state) {
  checkbox_->SetVisible(false);
  switch (dialog_state) {
    case LocalCardMigrationDialogState::kFinished:
      migration_succeeded_image_->SetVisible(true);
      break;
    case LocalCardMigrationDialogState::kActionRequired:
      migration_failed_image_->SetVisible(true);
      delete_card_from_local_button_->SetVisible(true);
      break;
    case LocalCardMigrationDialogState::kOffered:
      NOTREACHED();
      break;
  }
}

const char* MigratableCardView::GetClassName() const {
  return kViewClassName;
}

void MigratableCardView::Init(
    const MigratableCreditCard& migratable_credit_card,
    views::ButtonListener* listener,
    int card_index) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(),
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_HORIZONTAL_SMALL)));

  checkbox_ = new views::Checkbox(base::string16(), listener);
  checkbox_->SetChecked(true);
  checkbox_->set_tag(card_index);
  checkbox_->SetVisible(true);
  // TODO(crbug/867194): Currently the ink drop animation circle is cut by the
  // border of scroll bar view. Find a way to adjust the format.
  checkbox_->SetInkDropMode(views::InkDropHostView::InkDropMode::OFF);
  AddChildView(checkbox_);

  constexpr int kMigrationResultImageSize = 16;
  migration_succeeded_image_ = new views::ImageView();
  migration_succeeded_image_->SetImage(
      gfx::CreateVectorIcon(vector_icons::kCheckCircleIcon,
                            kMigrationResultImageSize, gfx::kGoogleGreen700));
  migration_succeeded_image_->SetVisible(false);
  AddChildView(migration_succeeded_image_);
  migration_failed_image_ = new views::ImageView();
  migration_failed_image_->SetImage(gfx::CreateVectorIcon(
      kBrowserToolsErrorIcon, kMigrationResultImageSize, gfx::kGoogleRed700));
  migration_failed_image_->SetVisible(false);
  AddChildView(migration_failed_image_);

  std::unique_ptr<views::ImageView> card_image =
      std::make_unique<views::ImageView>();
  card_image->SetImage(
      rb.GetImageNamed(CreditCard::IconResourceId(
                           migratable_credit_card.credit_card().network()))
          .AsImageSkia());
  AddChildView(card_image.release());

  std::unique_ptr<views::Label> card_description =
      std::make_unique<views::Label>(
          migratable_credit_card.credit_card().NetworkAndLastFourDigits(),
          views::style::CONTEXT_LABEL);
  AddChildView(card_description.release());

  std::unique_ptr<views::Label> card_expiration =
      std::make_unique<views::Label>(migratable_credit_card.credit_card()
                                         .AbbreviatedExpirationDateForDisplay(),
                                     views::style::CONTEXT_LABEL,
                                     ChromeTextStyle::STYLE_SECONDARY);
  AddChildView(card_expiration.release());

  delete_card_from_local_button_ = views::CreateVectorImageButton(listener);
  views::SetImageFromVectorIcon(delete_card_from_local_button_, kTrashCanIcon);
  // TODO(crbug.com/867194): Add tooltip and tag for the
  // delete_card_from_local_button_.
  delete_card_from_local_button_->SetVisible(false);
  AddChildView(delete_card_from_local_button_);
}

}  // namespace autofill
