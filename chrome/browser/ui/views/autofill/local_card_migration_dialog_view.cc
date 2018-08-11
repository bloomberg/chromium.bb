// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/local_card_migration_dialog_view.h"

#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_factory.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_state.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/view_util.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/ui/local_card_migration_dialog_controller.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace autofill {

LocalCardMigrationDialogView::LocalCardMigrationDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Set up main contents container.
  views::View* main_container = new views::View();
  main_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  AddChildView(main_container);

  views::ImageView* image = new views::ImageView();
  image->SetImage(rb.GetImageSkiaNamed(GetHeaderImageId()));
  main_container->AddChildView(image);

  views::View* content_container = new views::View();
  content_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  content_container->SetBorder(views::CreateEmptyBorder(
      provider->GetDialogInsetsForContentType(views::TEXT, views::CONTROL)));
  main_container->AddChildView(content_container);

  title_ =
      new views::Label(GetDialogTitle(), views::style::CONTEXT_DIALOG_TITLE);
  content_container->AddChildView(title_);

  explanation_text_ =
      new views::Label(GetDialogInstruction(), CONTEXT_BODY_TEXT_LARGE,
                       ChromeTextStyle::STYLE_SECONDARY);
  explanation_text_->SetMultiLine(true);
  explanation_text_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  content_container->AddChildView(explanation_text_);
}

LocalCardMigrationDialogView::~LocalCardMigrationDialogView() {}

void LocalCardMigrationDialogView::ShowDialog(
    base::OnceClosure user_accepted_migration_closure) {
  user_accepted_migration_closure_ = std::move(user_accepted_migration_closure);
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void LocalCardMigrationDialogView::CloseDialog() {
  GetWidget()->Close();
}

gfx::Size LocalCardMigrationDialogView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

ui::ModalType LocalCardMigrationDialogView::GetModalType() const {
  // This should be a modal dialog since we don't want users to lose progress
  // in the migration workflow until they are done.
  return ui::MODAL_TYPE_CHILD;
}

void LocalCardMigrationDialogView::AddedToWidget() {
  GetWidget()->AddObserver(this);
}

bool LocalCardMigrationDialogView::ShouldShowCloseButton() const {
  return false;
}

base::string16 LocalCardMigrationDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? GetOkButtonLabel()
                                        : GetCancelButtonLabel();
}

// TODO(crbug.com/867194): Add button logic for the kFinished dialog and
// the kActionRequired dialog.
bool LocalCardMigrationDialogView::Accept() {
  OnSaveButtonClicked();
  // Dialog will not be closed if it is the migration offer dialog.
  return false;
}

// TODO(crbug.com/867194): Add button logic for the kFinished dialog and
// the kActionRequired dialog.
bool LocalCardMigrationDialogView::Cancel() {
  OnCancelButtonClicked();
  // Dialog will be closed.
  return true;
}

void LocalCardMigrationDialogView::OnWidgetClosing(views::Widget* widget) {
  controller_->OnDialogClosed();
  widget->RemoveObserver(this);
}

base::string16 LocalCardMigrationDialogView::GetDialogTitle() const {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TITLE_OFFER);
    case LocalCardMigrationDialogState::kFinished:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TITLE_DONE);
    case LocalCardMigrationDialogState::kActionRequired:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TITLE_FIX);
  }
}

base::string16 LocalCardMigrationDialogView::GetDialogInstruction() const {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      return l10n_util::GetPluralStringFUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_OFFER,
          controller_->GetCardList().size());
    case LocalCardMigrationDialogState::kFinished:
      return l10n_util::GetPluralStringFUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_DONE,
          controller_->GetCardList().size());
    case LocalCardMigrationDialogState::kActionRequired:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_FIX);
  }
}

int LocalCardMigrationDialogView::GetHeaderImageId() const {
  return IDR_AUTOFILL_MIGRATION_DIALOG_HEADER;
}

base::string16 LocalCardMigrationDialogView::GetOkButtonLabel() const {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_SAVE);
    case LocalCardMigrationDialogState::kFinished:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_DONE);
    case LocalCardMigrationDialogState::kActionRequired:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_DONE);
  }
}

base::string16 LocalCardMigrationDialogView::GetCancelButtonLabel() const {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_CANCEL);
    case LocalCardMigrationDialogState::kFinished:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_VIEW_CARDS);
    case LocalCardMigrationDialogState::kActionRequired:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_VIEW_CARDS);
  }
}

void LocalCardMigrationDialogView::OnSaveButtonClicked() {
  std::move(user_accepted_migration_closure_).Run();
}

void LocalCardMigrationDialogView::OnCancelButtonClicked() {
  user_accepted_migration_closure_.Reset();
}

LocalCardMigrationDialog* CreateLocalCardMigrationDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents) {
  return new LocalCardMigrationDialogView(controller, web_contents);
}

}  // namespace autofill