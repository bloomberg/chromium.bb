// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/legal_message_line.h"
#include "components/autofill/core/browser/ui/save_card_bubble_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/bubble/tooltip_icon.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

namespace {

// Dimensions of the Google Pay logo.
const int kGooglePayLogoWidth = 40;
const int kGooglePayLogoHeight = 16;

const int kGooglePayLogoSeparatorHeight = 12;

const SkColor kTitleSeparatorColor = SkColorSetRGB(0x9E, 0x9E, 0x9E);

}  // namespace

SaveCardBubbleViews::SyncPromoDelegate::SyncPromoDelegate(
    SaveCardBubbleController* controller)
    : controller_(controller) {
  DCHECK(controller_);
}

void SaveCardBubbleViews::SyncPromoDelegate::OnEnableSync(
    const AccountInfo& account,
    bool is_default_promo_account) {
  controller_->OnSyncPromoAccepted(account, is_default_promo_account);
}

SaveCardBubbleViews::SaveCardBubbleViews(views::View* anchor_view,
                                         const gfx::Point& anchor_point,
                                         content::WebContents* web_contents,
                                         SaveCardBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, anchor_point, web_contents),
      controller_(controller) {
  DCHECK(controller);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SAVE_CARD);
}

void SaveCardBubbleViews::Show(DisplayReason reason) {
  ShowForReason(reason);
  AssignIdsToDialogClientView();
}

void SaveCardBubbleViews::Hide() {
  // If |controller_| is null, WindowClosing() won't invoke OnBubbleClosed(), so
  // do that here. This will clear out |controller_|'s reference to |this|. Note
  // that WindowClosing() happens only after the _asynchronous_ Close() task
  // posted in CloseBubble() completes, but we need to fix references sooner.
  if (controller_)
    controller_->OnBubbleClosed();
  controller_ = nullptr;
  CloseBubble();
}

views::View* SaveCardBubbleViews::CreateFootnoteView() {
  return nullptr;
}

bool SaveCardBubbleViews::Accept() {
  if (controller_)
    controller_->OnSaveButton(base::string16());
  return true;
}

bool SaveCardBubbleViews::Cancel() {
  if (controller_)
    controller_->OnCancelButton();
  return true;
}

bool SaveCardBubbleViews::Close() {
  // If there is a cancel button (non-Material UI), Cancel is logged as a
  // different user action than closing, so override Close() to prevent the
  // superclass' implementation from calling Cancel().
  //
  // Clicking the top-right [X] close button and/or focusing then unfocusing the
  // bubble count as a close action only (without calling Cancel), which means
  // we can't tell the controller to permanently hide the bubble on close,
  // because the user simply dismissed/ignored the bubble; they might want to
  // access the bubble again from the location bar icon. Return true to indicate
  // that the bubble can be closed.
  return true;
}

int SaveCardBubbleViews::GetDialogButtons() const {
  // Material UI has no "No thanks" button in favor of an [X].
  return ui::MaterialDesignController::IsSecondaryUiMaterial()
             ? ui::DIALOG_BUTTON_OK
             : ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

gfx::Size SaveCardBubbleViews::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void SaveCardBubbleViews::AddedToWidget() {
  // Use a custom title container if offering to upload a server card.
  // Done when this view is added to the widget, so the bubble frame
  // view is guaranteed to exist.
  if (!controller_->IsUploadSave())
    return;
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  auto title_container = std::make_unique<views::View>();
  // TODO(ftirelo): DISTANCE_RELATED_BUTTON_HORIZONTAL isn't the right choice
  //                here, but INSETS_DIALOG_TITLE gives too much padding.
  //                Make a new Harmony DistanceMetric?
  title_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));

  // kGooglePayLogoIcon is square, and CreateTiledImage() will clip it whereas
  // setting the icon size would rescale it incorrectly.
  gfx::ImageSkia image = gfx::ImageSkiaOperations::CreateTiledImage(
      gfx::CreateVectorIcon(kGooglePayLogoIcon, gfx::kPlaceholderColor),
      /*x=*/0, /*y=*/0, kGooglePayLogoWidth, kGooglePayLogoHeight);
  auto icon_view = std::make_unique<views::ImageView>();
  icon_view->SetImage(&image);
  title_container->AddChildView(icon_view.release());

  auto* separator = new views::Separator();
  separator->SetColor(kTitleSeparatorColor);
  title_container->AddChildView(separator);

  auto title_label = std::make_unique<views::Label>(
      GetWindowTitle(), views::style::CONTEXT_DIALOG_TITLE);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  title_container->AddChildView(title_label.release());

  GetBubbleFrameView()->SetTitleView(std::move(title_container));

  // Add vertical padding to the separator doesn't expand to use all the
  // available vertical space. This needs to be done after the title container
  // is added to the bubble frame view, in order to use its preferred size.
  const int separator_vertical_padding =
      (GetBubbleFrameView()->title()->GetPreferredSize().height() -
       kGooglePayLogoSeparatorHeight) /
      2;
  separator->SetBorder(views::CreateEmptyBorder(gfx::Insets(
      /*vertical=*/separator_vertical_padding,
      /*horizontal=*/0)));
}

bool SaveCardBubbleViews::ShouldShowCloseButton() const {
  // The [X] is shown for Material UI.
  return ui::MaterialDesignController::IsSecondaryUiMaterial();
}

base::string16 SaveCardBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : base::string16();
}

void SaveCardBubbleViews::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
}

views::View* SaveCardBubbleViews::GetFootnoteViewForTesting() {
  return footnote_view_;
}

SaveCardBubbleViews::~SaveCardBubbleViews() {}

// Overridden
std::unique_ptr<views::View> SaveCardBubbleViews::CreateMainContentView() {
  std::unique_ptr<views::View> view = std::make_unique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  // If applicable, add the upload explanation label.  Appears above the card
  // info.
  base::string16 explanation = controller_->GetExplanatoryMessage();
  if (!explanation.empty()) {
    auto* explanation_label = new views::Label(
        explanation, CONTEXT_BODY_TEXT_LARGE, ChromeTextStyle::STYLE_SECONDARY);
    explanation_label->SetMultiLine(true);
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view->AddChildView(explanation_label);
  }

  // Add the card type icon, last four digits and expiration date.
  auto* description_view = new views::View();
  views::BoxLayout* box_layout =
      description_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, gfx::Insets(),
          provider->GetDistanceMetric(
              views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  view->AddChildView(description_view);

  const CreditCard& card = controller_->GetCard();
  auto* card_type_icon = new views::ImageView();
  card_type_icon->SetImage(
      ui::ResourceBundle::GetSharedInstance()
          .GetImageNamed(CreditCard::IconResourceId(card.network()))
          .AsImageSkia());
  card_type_icon->SetTooltipText(card.NetworkForDisplay());
  description_view->AddChildView(card_type_icon);

  description_view->AddChildView(
      new views::Label(card.NetworkAndLastFourDigits(), CONTEXT_BODY_TEXT_LARGE,
                       views::style::STYLE_PRIMARY));

  // The spacer will stretch to use the available horizontal space in the
  // dialog, which will end-align the expiration date label.
  auto* spacer = new views::View();
  description_view->AddChildView(spacer);
  box_layout->SetFlexForView(spacer, /*flex=*/1);

  description_view->AddChildView(new views::Label(
      card.AbbreviatedExpirationDateForDisplay(), CONTEXT_BODY_TEXT_LARGE,
      ChromeTextStyle::STYLE_SECONDARY));

  return view;
}

void SaveCardBubbleViews::SetFootnoteViewForTesting(
    views::View* footnote_view) {
  footnote_view_ = footnote_view;
}

void SaveCardBubbleViews::AssignIdsToDialogClientView() {
  auto* ok_button = GetDialogClientView()->ok_button();
  if (ok_button)
    ok_button->set_id(DialogViewId::OK_BUTTON);
  auto* cancel_button = GetDialogClientView()->cancel_button();
  if (cancel_button)
    cancel_button->set_id(DialogViewId::CANCEL_BUTTON);
}

void SaveCardBubbleViews::Init() {
  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  // For server cards, there is an explanation between the title and the
  // controls; use views::TEXT. For local cards, since there is no explanation,
  // use views::CONTROL instead.
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      controller_->GetExplanatoryMessage().empty() ? views::CONTROL
                                                   : views::TEXT,
      GetDialogButtons() == ui::DIALOG_BUTTON_NONE ? views::TEXT
                                                   : views::CONTROL));
  AddChildView(CreateMainContentView().release());
}

}  // namespace autofill
