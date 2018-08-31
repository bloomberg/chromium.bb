// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/local_card_migration_dialog_view.h"

#include "base/location.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_factory.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_state.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/migratable_card_view.h"
#include "chrome/browser/ui/views/autofill/view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/legal_message_line.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/ui/local_card_migration_dialog_controller.h"
#include "components/autofill/core/common/autofill_features.h"
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
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

namespace {

MigratableCardView* AsMigratableCardView(views::View* view) {
  DCHECK_EQ(MigratableCardView::kViewClassName, view->GetClassName());
  return static_cast<MigratableCardView*>(view);
}

}  // namespace

LocalCardMigrationDialogView::LocalCardMigrationDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {}

LocalCardMigrationDialogView::~LocalCardMigrationDialogView() {}

void LocalCardMigrationDialogView::ShowDialog(
    base::OnceClosure user_accepted_migration_closure) {
  user_accepted_migration_closure_ = std::move(user_accepted_migration_closure);
  Init();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void LocalCardMigrationDialogView::CloseDialog() {
  GetWidget()->Close();
}

void LocalCardMigrationDialogView::OnMigrationFinished() {
  show_close_button_timer_.Stop();
  title_->SetText(GetDialogTitle());
  explanation_text_->SetText(GetDialogInstruction());
  for (int index = 0; index < card_list_view_->child_count(); index++) {
    AsMigratableCardView(card_list_view_->child_at(index))
        ->UpdateCardView(controller_->GetViewState());
  }
  separator_->SetVisible(false);
  // TODO(crbug/867194): Add tip value prompt.
  SetMigrationIsPending(false);
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

views::View* LocalCardMigrationDialogView::CreateExtraView() {
  close_migration_dialog_button_.reset(
      views::MdTextButton::CreateSecondaryUiButton(
          this, l10n_util::GetStringUTF16(IDS_CLOSE)));
  close_migration_dialog_button_->SetVisible(false);
  return close_migration_dialog_button_.get();
}

bool LocalCardMigrationDialogView::ShouldShowCloseButton() const {
  return false;
}

base::string16 LocalCardMigrationDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? GetOkButtonLabel()
                                        : GetCancelButtonLabel();
}

bool LocalCardMigrationDialogView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  // The buttons will be disabled when card uploading is in progress.
  return !migration_pending_;
}

bool LocalCardMigrationDialogView::Accept() {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      OnSaveButtonClicked();
      return !base::FeatureList::IsEnabled(
          features::kAutofillLocalCardMigrationShowFeedback);
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      return true;
  }
}

bool LocalCardMigrationDialogView::Cancel() {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      OnCancelButtonClicked();
      return true;
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      OnViewCardsButtonClicked();
      return true;
  }
}

void LocalCardMigrationDialogView::OnWidgetClosing(views::Widget* widget) {
  controller_->OnDialogClosed();
  widget->RemoveObserver(this);
}

// TODO(crbug/867194): Add button pressed logic for kDeleteCardButtonTag.
void LocalCardMigrationDialogView::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  // If button clicked is the |close_migration_dialog_button_|, close the
  // dialog.
  if (sender == close_migration_dialog_button_.get()) {
    CloseDialog();
  } else {
    // Checkbox is clicked.
    controller_->OnCardSelected(sender->tag());
  }
}

// TODO(crbug/867194): Add metrics for legal message link clicking.
void LocalCardMigrationDialogView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  if (!controller_)
    return;

  legal_message_container_->OnLinkClicked(label, range, web_contents_);
}

void LocalCardMigrationDialogView::Init() {
  if (has_children())
    return;
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Set up main contents container.
  std::unique_ptr<views::View> main_container = std::make_unique<views::View>();
  constexpr int kMigrationDialogUnrelatedControlVerticalDistance = 24;
  main_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      kMigrationDialogUnrelatedControlVerticalDistance));

  std::unique_ptr<views::View> image_container =
      std::make_unique<views::View>();
  image_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  constexpr int kMigrationDialogImageBorderBottom = 16;
  image_container->SetBorder(
      views::CreateEmptyBorder(0, 0, kMigrationDialogImageBorderBottom, 0));
  std::unique_ptr<views::ImageView> image =
      std::make_unique<views::ImageView>();
  image->SetImage(rb.GetImageSkiaNamed(GetHeaderImageId()));
  image_container->AddChildView(image.release());
  main_container->AddChildView(image_container.release());

  title_ = std::make_unique<views::Label>(GetDialogTitle(),
                                          views::style::CONTEXT_DIALOG_TITLE);
  main_container->AddChildView(title_.get());

  std::unique_ptr<views::View> contents_container =
      std::make_unique<views::View>();
  contents_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  constexpr int kMigrationDialogInsets = 24;
  gfx::Insets migration_dialog_insets = gfx::Insets(kMigrationDialogInsets);
  contents_container->SetBorder(
      views::CreateEmptyBorder(migration_dialog_insets));

  explanation_text_ = std::make_unique<views::Label>(
      GetDialogInstruction(), CONTEXT_BODY_TEXT_LARGE,
      ChromeTextStyle::STYLE_SECONDARY);
  explanation_text_->SetMultiLine(true);
  explanation_text_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  contents_container->AddChildView(explanation_text_.get());

  card_list_view_ = std::make_unique<views::View>();
  const std::vector<MigratableCreditCard>& migratable_credit_cards =
      controller_->GetCardList();
  views::BoxLayout* card_list_view_layout_ =
      card_list_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical, gfx::Insets(),
          provider->GetDistanceMetric(
              views::DISTANCE_RELATED_CONTROL_VERTICAL)));
  card_list_view_layout_->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  for (size_t index = 0; index < migratable_credit_cards.size(); index++) {
    card_list_view_->AddChildView(
        std::make_unique<MigratableCardView>(migratable_credit_cards[index],
                                             this, static_cast<int>(index))
            .release());
  }

  std::unique_ptr<views::ScrollView> card_list_scroll_bar =
      std::make_unique<views::ScrollView>();
  card_list_scroll_bar->set_hide_horizontal_scrollbar(true);
  card_list_scroll_bar->SetContents(card_list_view_.get());
  card_list_scroll_bar->set_draw_overflow_indicator(false);
  constexpr int kCardListScrollViewHeight = 140;
  card_list_scroll_bar->ClipHeightTo(0, kCardListScrollViewHeight);
  contents_container->AddChildView(card_list_scroll_bar.release());
  main_container->AddChildView(contents_container.release());

  separator_ = std::make_unique<views::Separator>();
  main_container->AddChildView(separator_.get());

  legal_message_container_ = std::make_unique<LegalMessageView>(
      controller_->GetLegalMessageLines(), this);
  constexpr int kMigrationDialogContentMarginBottomText = 48;
  legal_message_container_->SetBorder(
      views::CreateEmptyBorder(0, migration_dialog_insets.left(),
                               kMigrationDialogContentMarginBottomText,
                               migration_dialog_insets.right()));
  main_container->AddChildView(legal_message_container_.get());

  AddChildView(main_container.release());
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
    case LocalCardMigrationDialogState::kActionRequired:
      return l10n_util::GetStringUTF16(
          IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_BUTTON_LABEL_VIEW_CARDS);
  }
}

void LocalCardMigrationDialogView::OnSaveButtonClicked() {
  for (int index = 0; index < card_list_view_->child_count(); index++) {
    // Checkboxes will be disabled when card uploading is in progress.
    AsMigratableCardView(card_list_view_->child_at(index))
        ->SetCheckboxEnabled(false);
  }
  SetMigrationIsPending(true);
  std::move(user_accepted_migration_closure_).Run();
  show_close_button_timer_.Start(
      FROM_HERE, features::GetTimeoutForMigrationPromptFeedbackCloseButton(),
      this, &LocalCardMigrationDialogView::ShowCloseButton);
}

void LocalCardMigrationDialogView::OnCancelButtonClicked() {
  user_accepted_migration_closure_.Reset();
}

void LocalCardMigrationDialogView::OnViewCardsButtonClicked() {
  constexpr int kPaymentsProfileUserIndex = 0;
  web_contents_->OpenURL(content::OpenURLParams(
      payments::GetManageInstrumentsUrl(kPaymentsProfileUserIndex),
      content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false));
}

void LocalCardMigrationDialogView::SetMigrationIsPending(
    bool migration_pending) {
  migration_pending_ = migration_pending;
  DialogModelChanged();
  GetDialogClientView()->Layout();
}

void LocalCardMigrationDialogView::ShowCloseButton() {
  close_migration_dialog_button_->SetVisible(true);
}

LocalCardMigrationDialog* CreateLocalCardMigrationDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents) {
  return new LocalCardMigrationDialogView(controller, web_contents);
}

}  // namespace autofill
