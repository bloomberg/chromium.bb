// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/legal_message_line.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace autofill {

namespace {

// Fixed width of the bubble.
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
                                         content::WebContents* web_contents,
                                         SaveCardBubbleController* controller)
    : LocationBarBubbleDelegateView(anchor_view, web_contents),
      controller_(controller),
      learn_more_link_(nullptr) {
  DCHECK(controller);
  views::BubbleDialogDelegateView::CreateBubble(this);
}

SaveCardBubbleViews::~SaveCardBubbleViews() {}

void SaveCardBubbleViews::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void SaveCardBubbleViews::Hide() {
  controller_ = nullptr;
  CloseBubble();
}

views::View* SaveCardBubbleViews::CreateExtraView() {
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
  View* view = new View();
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  // Add a StyledLabel for each line of the legal message.
  for (const LegalMessageLine& line : controller_->GetLegalMessageLines())
    view->AddChildView(CreateLegalMessageLineLabel(line, this).release());

  return view;
}

bool SaveCardBubbleViews::Accept() {
  controller_->OnSaveButton();
  return true;
}

bool SaveCardBubbleViews::Cancel() {
  if (controller_)
    controller_->OnCancelButton();
  return true;
}

bool SaveCardBubbleViews::Close() {
  // Override to prevent Cancel from being called when the bubble is hidden.
  // Return true to indicate that the bubble can be closed.
  return true;
}

int SaveCardBubbleViews::GetDialogButtons() const {
  // This is the default for BubbleDialogDelegateView, but it's not the default
  // for LocationBarBubbleDelegateView.
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

base::string16 SaveCardBubbleViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                       ? IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT
                                       : IDS_AUTOFILL_SAVE_CARD_PROMPT_DENY);
}

bool SaveCardBubbleViews::ShouldDefaultButtonBeBlue() const {
  return true;
}

gfx::Size SaveCardBubbleViews::GetPreferredSize() const {
  return gfx::Size(kBubbleWidth, GetHeightForWidth(kBubbleWidth));
}

base::string16 SaveCardBubbleViews::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void SaveCardBubbleViews::WindowClosing() {
  if (controller_)
    controller_->OnBubbleClosed();
}

void SaveCardBubbleViews::LinkClicked(views::Link* source, int event_flags) {
  DCHECK_EQ(source, learn_more_link_);
  controller_->OnLearnMoreClicked();
}

void SaveCardBubbleViews::StyledLabelLinkClicked(views::StyledLabel* label,
                                                 const gfx::Range& range,
                                                 int event_flags) {
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

// Create view containing everything except for the footnote.
std::unique_ptr<views::View> SaveCardBubbleViews::CreateMainContentView() {
  std::unique_ptr<View> view(new View());
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                           views::kUnrelatedControlVerticalSpacing));

  // Add the card type icon, last four digits and expiration date.
  views::View* description_view = new views::View();
  description_view->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, views::kRelatedButtonHSpacing));
  view->AddChildView(description_view);

  const CreditCard& card = controller_->GetCard();
  views::ImageView* card_type_icon = new views::ImageView();
  card_type_icon->SetImage(
      ResourceBundle::GetSharedInstance()
          .GetImageNamed(CreditCard::IconResourceId(card.type()))
          .AsImageSkia());
  card_type_icon->SetTooltipText(card.TypeForDisplay());
  card_type_icon->SetBorder(
      views::Border::CreateSolidBorder(1, SkColorSetA(SK_ColorBLACK, 10)));
  description_view->AddChildView(card_type_icon);

  description_view->AddChildView(new views::Label(
      base::string16(kMidlineEllipsis) + card.LastFourDigits()));
  description_view->AddChildView(
      new views::Label(card.AbbreviatedExpirationDateForDisplay()));

  // Optionally add label that will contain an explanation for upload.
  base::string16 explanation = controller_->GetExplanatoryMessage();
  if (!explanation.empty()) {
    views::Label* explanation_label = new views::Label(explanation);
    explanation_label->SetMultiLine(true);
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view->AddChildView(explanation_label);
  }

  return view;
}

void SaveCardBubbleViews::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(CreateMainContentView().release());
}

}  // namespace autofill
