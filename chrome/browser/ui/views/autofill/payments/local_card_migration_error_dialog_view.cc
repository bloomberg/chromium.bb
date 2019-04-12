// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/local_card_migration_error_dialog_view.h"

#include "base/macros.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/local_card_migration_dialog_factory.h"
#include "chrome/browser/ui/autofill/payments/payments_ui_constants.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

LocalCardMigrationErrorDialogView::LocalCardMigrationErrorDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  set_close_on_deactivate(false);
  set_margins(gfx::Insets());
}

LocalCardMigrationErrorDialogView::~LocalCardMigrationErrorDialogView() {}

void LocalCardMigrationErrorDialogView::ShowDialog() {
  Init();
  constrained_window::CreateBrowserModalDialogViews(
      this, web_contents_->GetTopLevelNativeWindow())
      ->Show();
}

void LocalCardMigrationErrorDialogView::CloseDialog() {
  controller_ = nullptr;
  GetWidget()->Close();
}

gfx::Size LocalCardMigrationErrorDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType LocalCardMigrationErrorDialogView::GetModalType() const {
  // The error dialog should be a modal dialog blocking the whole browser
  // which is consistent with other dialogs. It should make sure that the
  // user can see the error message.
  return ui::MODAL_TYPE_WINDOW;
}

bool LocalCardMigrationErrorDialogView::ShouldShowCloseButton() const {
  return false;
}

int LocalCardMigrationErrorDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

void LocalCardMigrationErrorDialogView::WindowClosing() {
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

bool LocalCardMigrationErrorDialogView::Cancel() {
  controller_->OnDoneButtonClicked();
  return true;
}

bool LocalCardMigrationErrorDialogView::Close() {
  // Close the dialog if the user exits the browser when dialog is visible.
  return true;
}

void LocalCardMigrationErrorDialogView::Init() {
  if (!children().empty())
    return;

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      kMigrationDialogMainContainerChildSpacing));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  auto* image = new views::ImageView();
  image->SetImage(
      rb.GetImageSkiaNamed(GetNativeTheme()->SystemDarkModeEnabled()
                               ? IDR_AUTOFILL_MIGRATION_DIALOG_HEADER_DARK
                               : IDR_AUTOFILL_MIGRATION_DIALOG_HEADER));
  image->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME));
  AddChildView(image);

  auto* error_view = new views::View();
  auto* horizontal_layout =
      error_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, gfx::Insets(),
          provider->GetDistanceMetric(
              views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  horizontal_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  error_view->SetBorder(views::CreateEmptyBorder(kMigrationDialogInsets));
  auto* error_image = new views::ImageView();
  error_image->SetImage(
      gfx::CreateVectorIcon(kBrowserToolsErrorIcon, gfx::kGoogleRed700));
  error_view->AddChildView(error_image);

  auto* error_message = new views::Label(
      l10n_util::GetPluralStringFUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_ERROR,
          controller_->GetCardList().size()),
      CONTEXT_BODY_TEXT_LARGE, ChromeTextStyle::STYLE_RED);
  error_view->AddChildView(error_message);

  AddChildView(error_view);
}

LocalCardMigrationDialog* CreateLocalCardMigrationErrorDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents) {
  return new LocalCardMigrationErrorDialogView(controller, web_contents);
}

}  // namespace autofill
