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
#include "chrome/browser/ui/views/autofill/view_util.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/legal_message_line.h"
#include "components/autofill/core/browser/ui/save_card_bubble_controller.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

namespace {

// Fixed width of the bubble, in dip.
const int kBubbleWidth = 395;

std::unique_ptr<views::StyledLabel> CreateLegalMessageLineLabel(
    const LegalMessageLine& line,
    views::StyledLabelListener* listener) {
  std::unique_ptr<views::StyledLabel> label(
      new views::StyledLabel(line.text(), listener));
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
  views::BubbleDialogDelegateView::CreateBubble(this);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SAVE_CARD);
}

void SaveCardBubbleViews::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void SaveCardBubbleViews::Hide() {
  controller_ = nullptr;
  CloseBubble();
}

views::View* SaveCardBubbleViews::CreateExtraView() {
  if (GetCurrentFlowStep() != LOCAL_SAVE_ONLY_STEP &&
      IsAutofillUpstreamShowNewUiExperimentEnabled())
    return nullptr;
  // Learn More link is only shown on local save bubble or when the new UI
  // experiment is disabled.
  DCHECK(!learn_more_link_);
  learn_more_link_ = new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_link_->SetUnderline(false);
  learn_more_link_->set_listener(this);
  return learn_more_link_;
}

views::View* SaveCardBubbleViews::CreateFootnoteView() {
  if (controller_->GetLegalMessageLines().empty())
    return nullptr;

  // Use BoxLayout to provide insets around the label.
  footnote_view_ = new View();
  footnote_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical));

  // Add a StyledLabel for each line of the legal message.
  for (const LegalMessageLine& line : controller_->GetLegalMessageLines()) {
    footnote_view_->AddChildView(
        CreateLegalMessageLineLabel(line, this).release());
  }

  // If on the first step of the 2-step upload flow, hide the footer area until
  // it's time to actually accept the dialog and ToS.
  if (GetCurrentFlowStep() == UPLOAD_SAVE_CVC_FIX_FLOW_STEP_1_OFFER_UPLOAD)
    footnote_view_->SetVisible(false);

  return footnote_view_;
}

bool SaveCardBubbleViews::Accept() {
  // The main content ViewStack for local save and happy-path upload save should
  // only ever have 1 View on it. Upload save can have a second View if CVC
  // needs to be requested. Assert that the ViewStack has no more than 2 Views
  // and that if it *does* have 2, it's because CVC is being requested.
  DCHECK_LE(view_stack_->size(), 2U);
  DCHECK(view_stack_->size() == 1 || controller_->ShouldRequestCvcFromUser());
  if (GetCurrentFlowStep() == UPLOAD_SAVE_CVC_FIX_FLOW_STEP_1_OFFER_UPLOAD) {
    // If user accepted upload but more info is needed, push the next view onto
    // the stack and update the bubble.
    DCHECK(controller_);
    controller_->ContinueToRequestCvcStage();
    view_stack_->Push(CreateRequestCvcView(), /*animate=*/true);
    GetWidget()->UpdateWindowTitle();
    GetWidget()->UpdateWindowIcon();
    // Disable the Save button until a valid CVC is entered:
    GetDialogClientView()->UpdateDialogButtons();
    // Make the legal messaging footer appear:
    DCHECK(footnote_view_);
    footnote_view_->SetVisible(true);
    // Resize the bubble if it's grown larger:
    SizeToContents();
    return false;
  }
  // Otherwise, close the bubble as normal.
  if (controller_)
    controller_->OnSaveButton(cvc_textfield_ ? cvc_textfield_->text()
                                             : base::string16());
  return true;
}

bool SaveCardBubbleViews::Cancel() {
  if (controller_)
    controller_->OnCancelButton();
  return true;
}

bool SaveCardBubbleViews::Close() {
  // Cancel is logged as a different user action than closing, so override
  // Close() to prevent the superclass' implementation from calling Cancel().
  // Additionally, both clicking the top-right [X] close button *and* focusing
  // then unfocusing the bubble count as a close action, which means we can't
  // tell the controller to permanently hide the bubble on close, because then
  // even things like switching tabs would dismiss the offer to save for good.
  // Return true to indicate that the bubble can be closed.
  return true;
}

int SaveCardBubbleViews::GetDialogButtons() const {
  if (GetCurrentFlowStep() == LOCAL_SAVE_ONLY_STEP ||
      !IsAutofillUpstreamShowNewUiExperimentEnabled())
    return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
  // For upload save when the new UI experiment is enabled, don't show the
  // [No thanks] cancel option; use the top-right [X] close button for that.
  return ui::DIALOG_BUTTON_OK;
}

base::string16 SaveCardBubbleViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (GetCurrentFlowStep()) {
    // Local save has two buttons:
    case LOCAL_SAVE_ONLY_STEP:
      return l10n_util::GetStringUTF16(
          button == ui::DIALOG_BUTTON_OK ? IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT
                                         : IDS_NO_THANKS);
    // Upload save has one button but it can say three different things:
    case UPLOAD_SAVE_ONLY_STEP:
      return l10n_util::GetStringUTF16(
          button == ui::DIALOG_BUTTON_OK ? IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT
                                         : IDS_NO_THANKS);
    case UPLOAD_SAVE_CVC_FIX_FLOW_STEP_1_OFFER_UPLOAD:
      return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                           ? IDS_AUTOFILL_SAVE_CARD_PROMPT_NEXT
                                           : IDS_NO_THANKS);
    case UPLOAD_SAVE_CVC_FIX_FLOW_STEP_2_REQUEST_CVC:
      return l10n_util::GetStringUTF16(
          button == ui::DIALOG_BUTTON_OK ? IDS_AUTOFILL_SAVE_CARD_PROMPT_CONFIRM
                                         : IDS_NO_THANKS);
    default:
      NOTREACHED();
      return base::string16();
  }
}

bool SaveCardBubbleViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_CANCEL)
    return true;

  DCHECK_EQ(ui::DIALOG_BUTTON_OK, button);
  return !cvc_textfield_ ||
         controller_->InputCvcIsValid(cvc_textfield_->text());
}

gfx::Size SaveCardBubbleViews::CalculatePreferredSize() const {
  return gfx::Size(kBubbleWidth, GetHeightForWidth(kBubbleWidth));
}

bool SaveCardBubbleViews::ShouldShowCloseButton() const {
  // Local save and Upload save on the old UI should have a [No thanks] button,
  // but Upload save on the new UI should surface the top-right [X] close button
  // instead.
  return GetCurrentFlowStep() != LOCAL_SAVE_ONLY_STEP &&
         IsAutofillUpstreamShowNewUiExperimentEnabled();
}

base::string16 SaveCardBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : base::string16();
}

gfx::ImageSkia SaveCardBubbleViews::GetWindowIcon() {
  if (IsAutofillUpstreamShowGoogleLogoExperimentEnabled())
    return gfx::CreateVectorIcon(kGoogleGLogoIcon, 16, gfx::kPlaceholderColor);
  return gfx::ImageSkia();
}

bool SaveCardBubbleViews::ShouldShowWindowIcon() const {
  return IsAutofillUpstreamShowGoogleLogoExperimentEnabled();
}

void SaveCardBubbleViews::WindowClosing() {
  if (controller_)
    controller_->OnBubbleClosed();
}

void SaveCardBubbleViews::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(source, learn_more_link_);
  if (controller_)
    controller_->OnLearnMoreClicked();
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
  DCHECK_EQ(cvc_textfield_, sender);
  GetDialogClientView()->UpdateDialogButtons();
}

SaveCardBubbleViews::~SaveCardBubbleViews() {}

SaveCardBubbleViews::CurrentFlowStep SaveCardBubbleViews::GetCurrentFlowStep()
    const {
  // No legal messages means this is not upload save.
  if (controller_->GetLegalMessageLines().empty())
    return LOCAL_SAVE_ONLY_STEP;
  // If we're not requesting CVC, this is the only step on the upload path.
  if (!controller_->ShouldRequestCvcFromUser())
    return UPLOAD_SAVE_ONLY_STEP;
  // Must be on the CVC fix flow on the upload path.
  if (view_stack_->size() == 1)
    return UPLOAD_SAVE_CVC_FIX_FLOW_STEP_1_OFFER_UPLOAD;
  if (view_stack_->size() == 2)
    return UPLOAD_SAVE_CVC_FIX_FLOW_STEP_2_REQUEST_CVC;
  // CVC fix flow should never have more than 3 views on the stack.
  NOTREACHED();
  return UNKNOWN_STEP;
}

// Create view containing everything except for the footnote.
std::unique_ptr<views::View> SaveCardBubbleViews::CreateMainContentView() {
  auto view = base::MakeUnique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));

  // If applicable, add the upload explanation label.  Appears above the card
  // info when new UI experiment is enabled.
  base::string16 explanation = controller_->GetExplanatoryMessage();
  if (!explanation.empty() && IsAutofillUpstreamShowNewUiExperimentEnabled()) {
    views::Label* explanation_label = new views::Label(explanation);
    explanation_label->SetMultiLine(true);
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view->AddChildView(explanation_label);
  }

  // Add the card type icon, last four digits and expiration date.
  views::View* description_view = new views::View();
  description_view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL)));
  view->AddChildView(description_view);

  const CreditCard& card = controller_->GetCard();
  views::ImageView* card_type_icon = new views::ImageView();
  card_type_icon->SetImage(
      ResourceBundle::GetSharedInstance()
          .GetImageNamed(CreditCard::IconResourceId(card.network()))
          .AsImageSkia());
  card_type_icon->SetTooltipText(card.NetworkForDisplay());
  card_type_icon->SetBorder(
      views::CreateSolidBorder(1, SkColorSetA(SK_ColorBLACK, 10)));
  description_view->AddChildView(card_type_icon);

  // Old UI shows last four digits and expiration.  New UI shows network, last
  // four digits, and expiration.
  if (IsAutofillUpstreamShowNewUiExperimentEnabled()) {
    description_view->AddChildView(
        new views::Label(card.NetworkAndLastFourDigits()));
  } else {
    description_view->AddChildView(new views::Label(
        base::string16(kMidlineEllipsis) + card.LastFourDigits()));
  }
  description_view->AddChildView(
      new views::Label(card.AbbreviatedExpirationDateForDisplay()));

  // If applicable, add the upload explanation label.  Appears below the card
  // info when new UI experiment is disabled.
  if (!explanation.empty() && !IsAutofillUpstreamShowNewUiExperimentEnabled()) {
    views::Label* explanation_label = new views::Label(explanation);
    explanation_label->SetMultiLine(true);
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view->AddChildView(explanation_label);
  }

  return view;
}

std::unique_ptr<views::View> SaveCardBubbleViews::CreateRequestCvcView() {
  auto request_cvc_view = base::MakeUnique<views::View>();
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  request_cvc_view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  request_cvc_view->SetBackground(views::CreateThemedSolidBackground(
      request_cvc_view.get(), ui::NativeTheme::kColorId_BubbleBackground));

  const CreditCard& card = controller_->GetCard();
  views::Label* explanation_label = new views::Label(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_SAVE_CARD_PROMPT_ENTER_CVC_EXPLANATION,
      card.NetworkAndLastFourDigits()));
  explanation_label->SetMultiLine(true);
  explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  request_cvc_view->AddChildView(explanation_label);

  views::View* cvc_entry_view = new views::View();
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  cvc_entry_view->SetLayoutManager(layout);

  DCHECK(!cvc_textfield_);
  cvc_textfield_ = CreateCvcTextfield();
  cvc_textfield_->set_controller(this);
  cvc_entry_view->AddChildView(cvc_textfield_);

  views::ImageView* cvc_image = new views::ImageView();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  cvc_image->SetImage(
      rb.GetImageSkiaNamed(controller_->GetCvcImageResourceId()));
  cvc_entry_view->AddChildView(cvc_image);

  request_cvc_view->AddChildView(cvc_entry_view);
  return request_cvc_view;
}

void SaveCardBubbleViews::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical));
  view_stack_ = new ViewStack();
  view_stack_->SetBackground(views::CreateThemedSolidBackground(
      view_stack_, ui::NativeTheme::kColorId_BubbleBackground));
  view_stack_->Push(CreateMainContentView(), /*animate=*/false);
  AddChildView(view_stack_);
}

}  // namespace autofill
