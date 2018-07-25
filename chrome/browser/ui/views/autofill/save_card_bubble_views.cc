// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"

#include <stddef.h>
#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/autofill/dialog_view_ids.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "components/autofill/core/browser/autofill_experiments.h"
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

const int kTooltipIconSize = 12;

const SkColor kTitleSeparatorColor = SkColorSetRGB(0x9E, 0x9E, 0x9E);

std::unique_ptr<views::StyledLabel> CreateLegalMessageLineLabel(
    const LegalMessageLine& line,
    views::StyledLabelListener* listener) {
  std::unique_ptr<views::StyledLabel> label(
      new views::StyledLabel(line.text(), listener));
  label->SetTextContext(CONTEXT_BODY_TEXT_LARGE);
  label->SetDefaultTextStyle(ChromeTextStyle::STYLE_SECONDARY);
  for (const LegalMessageLine::Link& link : line.links()) {
    label->AddStyleRange(link.range,
                         views::StyledLabel::RangeStyleInfo::CreateForLink());
  }
  return label;
}

}  // namespace

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
  if (controller_->GetLegalMessageLines().empty())
    return nullptr;

  // Use BoxLayout to provide insets around the label.
  footnote_view_ = new View();
  footnote_view_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));
  footnote_view_->set_id(DialogViewId::FOOTNOTE_VIEW);

  // Add a StyledLabel for each line of the legal message.
  for (const LegalMessageLine& line : controller_->GetLegalMessageLines()) {
    footnote_view_->AddChildView(
        CreateLegalMessageLineLabel(line, this).release());
  }

  return footnote_view_;
}

bool SaveCardBubbleViews::Accept() {
  if (controller_)
    controller_->OnSaveButton(cardholder_name_textfield_
                                  ? cardholder_name_textfield_->text()
                                  : base::string16());
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

base::string16 SaveCardBubbleViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                       ? IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT
                                       : IDS_NO_THANKS);
}

bool SaveCardBubbleViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return true;

  DCHECK_EQ(ui::DIALOG_BUTTON_OK, button);
  if (cardholder_name_textfield_) {
    // If requesting the user confirm the name, it cannot be blank.
    base::string16 trimmed_text;
    base::TrimWhitespace(cardholder_name_textfield_->text(), base::TRIM_ALL,
                         &trimmed_text);
    return !trimmed_text.empty();
  }
  return true;
}

gfx::Size SaveCardBubbleViews::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

void SaveCardBubbleViews::AddedToWidget() {
  // Use a custom title container if this is a server card. Done when this view
  // is added to the widget, so the bubble frame view is guaranteed to exist.
  if (GetCurrentFlowStep() != UPLOAD_SAVE_ONLY_STEP)
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

void SaveCardBubbleViews::StyledLabelLinkClicked(views::StyledLabel* label,
                                                 const gfx::Range& range,
                                                 int event_flags) {
  if (!controller_)
    return;

  // Index of |label| within its parent's view hierarchy is the same as the
  // legal message line index. DCHECK this assumption to guard against future
  // layout changes.
  DCHECK_EQ(static_cast<size_t>(label->parent()->child_count()),
            controller_->GetLegalMessageLines().size());

  const auto& links =
      controller_->GetLegalMessageLines()[label->parent()->GetIndexOf(label)]
          .links();
  for (const LegalMessageLine::Link& link : links) {
    if (link.range == range) {
      controller_->OnLegalMessageLinkClicked(link.url);
      return;
    }
  }

  // |range| was not found.
  NOTREACHED();
}

void SaveCardBubbleViews::ContentsChanged(views::Textfield* sender,
                                          const base::string16& new_contents) {
  DCHECK_EQ(cardholder_name_textfield_, sender);
  DialogModelChanged();
}

views::View* SaveCardBubbleViews::GetFootnoteViewForTesting() {
  return footnote_view_;
}

SaveCardBubbleViews::~SaveCardBubbleViews() {}

SaveCardBubbleViews::CurrentFlowStep SaveCardBubbleViews::GetCurrentFlowStep()
    const {
  // Local save if no legal messages, upload save otherwise.
  return controller_->GetLegalMessageLines().empty() ? LOCAL_SAVE_ONLY_STEP
                                                     : UPLOAD_SAVE_ONLY_STEP;
}

std::unique_ptr<views::View> SaveCardBubbleViews::CreateMainContentView() {
  std::unique_ptr<views::View> view = std::make_unique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  view->set_id(GetCurrentFlowStep() == LOCAL_SAVE_ONLY_STEP
                   ? DialogViewId::MAIN_CONTENT_VIEW_LOCAL
                   : DialogViewId::MAIN_CONTENT_VIEW_UPLOAD);

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

  // If necessary, add the cardholder name label and textfield to the upload
  // save dialog.
  if (controller_->ShouldRequestNameFromUser()) {
    std::unique_ptr<views::View> cardholder_name_label_row =
        std::make_unique<views::View>();

    // Set up cardholder name label.
    // TODO(jsaul): DISTANCE_RELATED_BUTTON_HORIZONTAL isn't the right choice
    //              here, but DISTANCE_RELATED_CONTROL_HORIZONTAL gives too much
    //              padding. Make a new Harmony DistanceMetric?
    cardholder_name_label_row->SetLayoutManager(
        std::make_unique<views::BoxLayout>(
            views::BoxLayout::kHorizontal, gfx::Insets(),
            provider->GetDistanceMetric(
                views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
    std::unique_ptr<views::Label> cardholder_name_label =
        std::make_unique<views::Label>(
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME),
            CONTEXT_BODY_TEXT_LARGE, ChromeTextStyle::STYLE_SECONDARY);
    cardholder_name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    cardholder_name_label_row->AddChildView(cardholder_name_label.release());

    // Prepare the prefilled cardholder name.
    base::string16 prefilled_name;
    if (!IsAutofillUpstreamBlankCardholderNameFieldExperimentEnabled()) {
      prefilled_name =
          base::UTF8ToUTF16(controller_->GetAccountInfo().full_name);
    }

    // Set up cardholder name label tooltip ONLY if the cardholder name
    // textfield will be prefilled.
    if (!prefilled_name.empty()) {
      std::unique_ptr<views::TooltipIcon> cardholder_name_tooltip =
          std::make_unique<views::TooltipIcon>(
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME_TOOLTIP),
              kTooltipIconSize);
      cardholder_name_tooltip->set_anchor_point_arrow(
          views::BubbleBorder::Arrow::TOP_LEFT);
      cardholder_name_tooltip->set_id(DialogViewId::CARDHOLDER_NAME_TOOLTIP);
      cardholder_name_label_row->AddChildView(
          cardholder_name_tooltip.release());
    }

    // Set up cardholder name textfield.
    DCHECK(!cardholder_name_textfield_);
    cardholder_name_textfield_ = new views::Textfield();
    cardholder_name_textfield_->set_controller(this);
    cardholder_name_textfield_->set_id(DialogViewId::CARDHOLDER_NAME_TEXTFIELD);
    cardholder_name_textfield_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_AUTOFILL_SAVE_CARD_PROMPT_CARDHOLDER_NAME));
    cardholder_name_textfield_->SetTextInputType(
        ui::TextInputType::TEXT_INPUT_TYPE_TEXT);
    cardholder_name_textfield_->SetText(prefilled_name);
    AutofillMetrics::LogSaveCardCardholderNamePrefilled(
        !prefilled_name.empty());

    // Add cardholder name elements to a single view, then to the final dialog.
    std::unique_ptr<views::View> cardholder_name_view =
        std::make_unique<views::View>();
    cardholder_name_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical, gfx::Insets(),
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));
    cardholder_name_view->AddChildView(cardholder_name_label_row.release());
    cardholder_name_view->AddChildView(cardholder_name_textfield_);
    view->AddChildView(cardholder_name_view.release());
  }

  return view;
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
      GetCurrentFlowStep() == UPLOAD_SAVE_ONLY_STEP ? views::TEXT
                                                    : views::CONTROL,
      views::CONTROL));
  AddChildView(CreateMainContentView().release());
}

}  // namespace autofill
