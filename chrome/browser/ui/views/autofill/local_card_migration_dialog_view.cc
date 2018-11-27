// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/local_card_migration_dialog_view.h"

#include "base/location.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_factory.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_state.h"
#include "chrome/browser/ui/autofill/popup_constants.h"
#include "chrome/browser/ui/views/autofill/migratable_card_view.h"
#include "chrome/browser/ui/views/autofill/view_util.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

namespace {

// Create the title label container for the migration dialogs. The title
// text depends on the |view_state| of the dialog.
std::unique_ptr<views::Label> CreateTitle(
    LocalCardMigrationDialogState view_state) {
  int message_id;
  switch (view_state) {
    case LocalCardMigrationDialogState::kOffered:
      message_id = IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TITLE_OFFER;
      break;
    case LocalCardMigrationDialogState::kFinished:
      message_id = IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TITLE_DONE;
      break;
    case LocalCardMigrationDialogState::kActionRequired:
      message_id = IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_TITLE_FIX;
      break;
  }
  auto title =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(message_id));
  constexpr int kMigrationDialogTitleFontSize = 8;
  title->SetFontList(gfx::FontList().Derive(kMigrationDialogTitleFontSize,
                                            gfx::Font::NORMAL,
                                            gfx::Font::Weight::MEDIUM));
  title->SetEnabledColor(gfx::kGoogleGrey900);
  constexpr int kMigrationDialogTitleLineHeight = 20;
  title->SetLineHeight(kMigrationDialogTitleLineHeight);
  return title;
}

// Create the explanation text label container for the migration dialogs. The
// text depends on the |view_state| of the dialog and the |card_list_size|.
std::unique_ptr<views::Label> CreateExplanationText(
    LocalCardMigrationDialogState view_state,
    int card_list_size) {
  int message_id;
  switch (view_state) {
    case LocalCardMigrationDialogState::kOffered:
      message_id = IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_OFFER;
      break;
    case LocalCardMigrationDialogState::kFinished:
      message_id = IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_DONE;
      break;
    case LocalCardMigrationDialogState::kActionRequired:
      message_id = IDS_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_MESSAGE_FIX;
      break;
  }
  auto explanation_text = std::make_unique<views::Label>(
      l10n_util::GetPluralStringFUTF16(message_id, card_list_size),
      CONTEXT_BODY_TEXT_LARGE, ChromeTextStyle::STYLE_SECONDARY);
  explanation_text->SetMultiLine(true);
  explanation_text->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return explanation_text;
}

// Create the scroll view of cards in |migratable_credit_cards|, and each
// row in the scroll view is a MigratableCardView. |card_list_listener|
// will be notified whenever the checkbox or the trash can button
// (if any) in any row is clicked. The content and the layout of the
// scroll view depends on |should_show_checkbox|.
std::unique_ptr<views::ScrollView> CreateCardList(
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    views::ButtonListener* card_list_listener,
    bool should_show_checkbox) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  auto* card_list_view = new views::View();
  constexpr int kCardListSmallVerticalDistance = 8;
  auto* card_list_view_layout =
      card_list_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical, gfx::Insets(),
          should_show_checkbox
              ? kCardListSmallVerticalDistance
              : provider->GetDistanceMetric(
                    views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  card_list_view_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  for (size_t index = 0; index < migratable_credit_cards.size(); ++index) {
    card_list_view->AddChildView(
        new MigratableCardView(migratable_credit_cards[index],
                               card_list_listener, should_show_checkbox));
  }

  auto card_list_scroll_view = std::make_unique<views::ScrollView>();
  card_list_scroll_view->set_hide_horizontal_scrollbar(true);
  card_list_scroll_view->SetContents(card_list_view);
  card_list_scroll_view->set_draw_overflow_indicator(false);
  constexpr int kCardListScrollViewHeight = 140;
  card_list_scroll_view->ClipHeightTo(0, kCardListScrollViewHeight);
  return card_list_scroll_view;
}

// Create the view containing the |tip_message| shown to the user.
std::unique_ptr<views::View> CreateTip(const base::string16& tip_message) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  // Set up the tip text container with inset, background and a solid border.
  auto tip_text_container = std::make_unique<views::View>();
  tip_text_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal,
      gfx::Insets(provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION)),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_LABEL_HORIZONTAL)));
  tip_text_container->SetBackground(
      views::CreateSolidBackground(gfx::kGoogleGrey050));
  constexpr int kTipValuePromptBorderThickness = 1;
  tip_text_container->SetBorder(views::CreateSolidBorder(
      kTipValuePromptBorderThickness, gfx::kGoogleGrey100));

  auto* lightbulb_outline_image = new views::ImageView();
  constexpr int kTipImageSize = 16;
  lightbulb_outline_image->SetImage(
      gfx::CreateVectorIcon(vector_icons::kLightbulbOutlineIcon, kTipImageSize,
                            gfx::kGoogleYellow700));
  lightbulb_outline_image->SetVerticalAlignment(views::ImageView::LEADING);
  tip_text_container->AddChildView(lightbulb_outline_image);

  auto* tip = new views::Label(tip_message, CONTEXT_BODY_TEXT_LARGE,
                               ChromeTextStyle::STYLE_SECONDARY);
  tip->SetMultiLine(true);
  tip->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  tip_text_container->AddChildView(tip);
  return tip_text_container;
}

// Create the feedback main content view composed of
// title, explanation text, card list, and the tip (if present).
std::unique_ptr<views::View> CreateFeedbackContentView(
    LocalCardMigrationDialogController* controller,
    views::ButtonListener* card_list_listener) {
  DCHECK(controller->GetViewState() != LocalCardMigrationDialogState::kOffered);

  auto feedback_view = std::make_unique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  feedback_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  feedback_view->SetBorder(views::CreateEmptyBorder(kMigrationDialogInsets));

  LocalCardMigrationDialogState view_state = controller->GetViewState();
  const std::vector<MigratableCreditCard>& card_list =
      controller->GetCardList();
  const int card_list_size = card_list.size();

  feedback_view->AddChildView(
      CreateExplanationText(view_state, card_list_size).release());
  feedback_view->AddChildView(
      CreateCardList(card_list, card_list_listener, false).release());
  // If there are no more than two cards in the finished dialog, show the tip.
  constexpr int kShowTipMessageCardNumberLimit = 2;
  if (view_state == LocalCardMigrationDialogState::kFinished &&
      card_list_size <= kShowTipMessageCardNumberLimit) {
    feedback_view->AddChildView(
        CreateTip(controller->GetTipMessage()).release());
  }

  return feedback_view;
}

}  // namespace

// A view composed of the main contents for local card migration dialog
// including title, explanatory message, migratable credit card list,
// horizontal separator, and legal message. It is used by
// LocalCardMigrationDialogView class when it offers the user the
// option to upload all browser-saved credit cards.
class LocalCardMigrationOfferView : public views::View,
                                    public views::StyledLabelListener {
 public:
  LocalCardMigrationOfferView(LocalCardMigrationDialogController* controller,
                              views::ButtonListener* card_list_listener)
      : controller_(controller) {
    ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical, gfx::Insets(),
        kMigrationDialogMainContainerChildSpacing));

    auto* contents_container = new views::View();
    contents_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical, gfx::Insets(),
        provider->GetDistanceMetric(
            views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
    // Don't set bottom since there is a legal message view in the offer dialog.
    contents_container->SetBorder(views::CreateEmptyBorder(
        0, kMigrationDialogInsets.left(), 0, kMigrationDialogInsets.right()));

    const std::vector<MigratableCreditCard>& card_list =
        controller->GetCardList();
    int card_list_size = card_list.size();

    contents_container->AddChildView(
        CreateExplanationText(controller_->GetViewState(), card_list_size)
            .release());

    std::unique_ptr<views::ScrollView> scroll_view =
        CreateCardList(card_list, card_list_listener, card_list_size != 1);
    card_list_view_ = scroll_view->contents();
    contents_container->AddChildView(scroll_view.release());

    AddChildView(contents_container);

    AddChildView(new views::Separator());

    legal_message_container_ =
        new LegalMessageView(controller->GetLegalMessageLines(), this);
    legal_message_container_->SetBorder(
        views::CreateEmptyBorder(kMigrationDialogInsets));
    AddChildView(legal_message_container_);
  }

  ~LocalCardMigrationOfferView() override {}

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override {
    controller_->OnLegalMessageLinkClicked(
        legal_message_container_->GetUrlForLink(label, range));
  }

  const std::vector<std::string> GetSelectedCardGuids() const {
    std::vector<std::string> selected_cards;
    for (int index = 0; index < card_list_view_->child_count(); ++index) {
      DCHECK_EQ(MigratableCardView::kViewClassName,
                card_list_view_->child_at(index)->GetClassName());
      MigratableCardView* card =
          static_cast<MigratableCardView*>(card_list_view_->child_at(index));
      if (card->IsSelected())
        selected_cards.push_back(card->GetGuid());
    }
    return selected_cards;
  }

 private:
  LocalCardMigrationDialogController* controller_;

  views::View* card_list_view_ = nullptr;

  // The view that contains legal message and handles legal message links
  // clicking.
  LegalMessageView* legal_message_container_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationOfferView);
};

LocalCardMigrationDialogView::LocalCardMigrationDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents)
    : controller_(controller), web_contents_(web_contents) {
  set_close_on_deactivate(false);
  set_margins(gfx::Insets());
}

LocalCardMigrationDialogView::~LocalCardMigrationDialogView() {}

void LocalCardMigrationDialogView::ShowDialog() {
  Init();
  constrained_window::ShowWebModalDialogViews(this, web_contents_);
}

void LocalCardMigrationDialogView::CloseDialog() {
  controller_ = nullptr;
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

bool LocalCardMigrationDialogView::ShouldShowCloseButton() const {
  return false;
}

base::string16 LocalCardMigrationDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? GetOkButtonLabel()
                                        : GetCancelButtonLabel();
}

// TODO(crbug.com/867194): Update this method when adding feedback.
bool LocalCardMigrationDialogView::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  // If the dialog is offer dialog and all checkboxes are unchecked, disable the
  // save button.
  if (controller_->GetViewState() == LocalCardMigrationDialogState::kOffered &&
      button == ui::DIALOG_BUTTON_OK) {
    DCHECK(offer_view_);
    return !offer_view_->GetSelectedCardGuids().empty();
  }
  return true;
}

bool LocalCardMigrationDialogView::Accept() {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      DCHECK(offer_view_);
      controller_->OnSaveButtonClicked(offer_view_->GetSelectedCardGuids());
      return true;
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      return true;
  }
}

bool LocalCardMigrationDialogView::Cancel() {
  switch (controller_->GetViewState()) {
    case LocalCardMigrationDialogState::kOffered:
      controller_->OnCancelButtonClicked();
      return true;
    case LocalCardMigrationDialogState::kFinished:
    case LocalCardMigrationDialogState::kActionRequired:
      controller_->OnViewCardsButtonClicked();
      return true;
  }
}

void LocalCardMigrationDialogView::WindowClosing() {
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

// TODO(crbug/867194): Add button pressed logic for kDeleteCardButtonTag.
void LocalCardMigrationDialogView::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  // The button clicked is a checkbox. Enable/disable the save
  // button if needed.
  DCHECK_EQ(sender->GetClassName(), views::Checkbox::kViewClassName);
  DialogModelChanged();
}

void LocalCardMigrationDialogView::Init() {
  if (has_children())
    return;

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      kMigrationDialogMainContainerChildSpacing));
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  auto* image = new views::ImageView();
  constexpr int kImageBorderBottom = 8;
  image->SetBorder(views::CreateEmptyBorder(0, 0, kImageBorderBottom, 0));
  image->SetImage(rb.GetImageSkiaNamed(IDR_AUTOFILL_MIGRATION_DIALOG_HEADER));
  image->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME));
  AddChildView(image);

  LocalCardMigrationDialogState view_state = controller_->GetViewState();
  AddChildView(CreateTitle(view_state).release());

  if (view_state == LocalCardMigrationDialogState::kOffered) {
    offer_view_ = new LocalCardMigrationOfferView(controller_, this);
    AddChildView(offer_view_);
  } else {
    AddChildView(CreateFeedbackContentView(controller_, this).release());
  }
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

LocalCardMigrationDialog* CreateLocalCardMigrationDialogView(
    LocalCardMigrationDialogController* controller,
    content::WebContents* web_contents) {
  return new LocalCardMigrationDialogView(controller, web_contents);
}

}  // namespace autofill
