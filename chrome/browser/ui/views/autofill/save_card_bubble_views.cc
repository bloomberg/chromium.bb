// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
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

// TODO(bondd): BubbleManager will eventually move this logic somewhere else,
// and then kIsOkButtonOnLeftSide can be removed from here and
// dialog_client_view.cc.
#if defined(OS_WIN) || defined(OS_CHROMEOS)
const bool kIsOkButtonOnLeftSide = true;
#else
const bool kIsOkButtonOnLeftSide = false;
#endif

scoped_ptr<views::StyledLabel> CreateLegalMessageLineLabel(
    const LegalMessageLine& line,
    views::StyledLabelListener* listener) {
  scoped_ptr<views::StyledLabel> label(
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
      save_button_(nullptr),
      cancel_button_(nullptr),
      learn_more_link_(nullptr) {
  DCHECK(controller);
  views::BubbleDelegateView::CreateBubble(this);
}

SaveCardBubbleViews::~SaveCardBubbleViews() {}

void SaveCardBubbleViews::Show(DisplayReason reason) {
  ShowForReason(reason);
}

void SaveCardBubbleViews::Hide() {
  controller_ = nullptr;
  Close();
}

scoped_ptr<views::View> SaveCardBubbleViews::CreateFootnoteView() {
  if (controller_->GetLegalMessageLines().empty())
    return nullptr;

  // Use BoxLayout to provide insets around the label.
  scoped_ptr<View> view(new View());
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  // Add a StyledLabel for each line of the legal message.
  for (const LegalMessageLine& line : controller_->GetLegalMessageLines())
    view->AddChildView(CreateLegalMessageLineLabel(line, this).release());

  return view;
}

gfx::Size SaveCardBubbleViews::GetPreferredSize() const {
  return gfx::Size(kBubbleWidth, GetHeightForWidth(kBubbleWidth));
}

views::View* SaveCardBubbleViews::GetInitiallyFocusedView() {
  return save_button_;
}

base::string16 SaveCardBubbleViews::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

void SaveCardBubbleViews::WindowClosing() {
  if (controller_)
    controller_->OnBubbleClosed();
}

void SaveCardBubbleViews::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  if (sender == save_button_) {
    controller_->OnSaveButton();
  } else {
    DCHECK_EQ(sender, cancel_button_);
    controller_->OnCancelButton();
  }
  Close();
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
scoped_ptr<views::View> SaveCardBubbleViews::CreateMainContentView() {
  scoped_ptr<View> view(new View());
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
      views::Border::CreateSolidBorder(1, kSubtleBorderColor));
  description_view->AddChildView(card_type_icon);

  // Midline horizontal ellipsis follwed by last four digits:
  description_view->AddChildView(new views::Label(
      base::UTF8ToUTF16("\xE2\x8B\xAF") + card.LastFourDigits()));
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

  // Add "learn more" link and accept/cancel buttons.
  views::View* button_view = new views::View();
  views::BoxLayout* button_view_layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, views::kRelatedButtonHSpacing);
  button_view->SetLayoutManager(button_view_layout);
  view->AddChildView(button_view);

  learn_more_link_ = new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  learn_more_link_->SetUnderline(false);
  learn_more_link_->set_listener(this);
  button_view->AddChildView(learn_more_link_);
  button_view_layout->SetFlexForView(learn_more_link_, 1);

  save_button_ = new views::BlueButton(
      this, l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT));
  save_button_->SetIsDefault(true);

  cancel_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_DENY));
  cancel_button_->SetStyle(views::Button::STYLE_BUTTON);

  if (kIsOkButtonOnLeftSide) {
    button_view->AddChildView(save_button_);
    button_view->AddChildView(cancel_button_);
  } else {
    button_view->AddChildView(cancel_button_);
    button_view->AddChildView(save_button_);
  }

  return view;
}

void SaveCardBubbleViews::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(CreateMainContentView().release());
}

}  // namespace autofill
