// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

using views::GridLayout;

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
    const SaveCardBubbleController::LegalMessageLine& line,
    views::StyledLabelListener* listener) {
  scoped_ptr<views::StyledLabel> label(
      new views::StyledLabel(line.text, listener));
  for (const SaveCardBubbleController::LegalMessageLine::Link& link :
       line.links) {
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

views::View* SaveCardBubbleViews::GetInitiallyFocusedView() {
  return save_button_;
}

base::string16 SaveCardBubbleViews::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

bool SaveCardBubbleViews::ShouldShowWindowTitle() const {
  return true;
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
          .links;
  for (const SaveCardBubbleController::LegalMessageLine::Link& link : links) {
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
  enum {
    COLUMN_SET_ID_SPACER,
    COLUMN_SET_ID_EXPLANATION,
    COLUMN_SET_ID_BUTTONS,
  };

  scoped_ptr<View> view(new View());
  GridLayout* layout = new GridLayout(view.get());
  view->SetLayoutManager(layout);

  // Add a column set with padding to establish a minimum width.
  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_SPACER);
  cs->AddPaddingColumn(0, kBubbleWidth);
  layout->StartRow(0, COLUMN_SET_ID_SPACER);

  int horizontal_inset = GetBubbleFrameView()->GetTitleInsets().left();
  // Optionally set up ColumnSet and label that will contain an explanation for
  // upload.
  base::string16 explanation = controller_->GetExplanatoryMessage();
  if (!explanation.empty()) {
    cs = layout->AddColumnSet(COLUMN_SET_ID_EXPLANATION);
    cs->AddPaddingColumn(0, horizontal_inset);
    // Fix the width of the label to ensure it breaks within the preferred size
    // of the bubble.
    cs->AddColumn(GridLayout::FILL, GridLayout::FILL, 0, GridLayout::FIXED,
                  kBubbleWidth - (2 * horizontal_inset), 0);
    cs->AddPaddingColumn(0, horizontal_inset);

    layout->StartRow(0, COLUMN_SET_ID_EXPLANATION);
    views::Label* explanation_label = new views::Label(explanation);
    explanation_label->SetMultiLine(true);
    explanation_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    layout->AddView(explanation_label);

    layout->AddPaddingRow(0, views::kUnrelatedControlLargeHorizontalSpacing);
  }

  // Set up ColumnSet that will contain the buttons and "learn more" link.
  cs = layout->AddColumnSet(COLUMN_SET_ID_BUTTONS);
  cs->AddPaddingColumn(0, horizontal_inset);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, horizontal_inset);

  // Create "learn more" link and add it to layout.
  learn_more_link_ = new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_link_->SetUnderline(false);
  learn_more_link_->set_listener(this);
  layout->StartRow(0, COLUMN_SET_ID_BUTTONS);
  layout->AddView(learn_more_link_);

  // Create accept button.
  save_button_ = new views::BlueButton(
      this, l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT));
  save_button_->SetIsDefault(true);

  // Create cancel button.
  cancel_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_PROMPT_DENY));
  cancel_button_->SetStyle(views::Button::STYLE_BUTTON);

  if (kIsOkButtonOnLeftSide) {
    layout->AddView(save_button_);
    layout->AddView(cancel_button_);
  } else {
    layout->AddView(cancel_button_);
    layout->AddView(save_button_);
  }
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  return view;
}

// Create view containing the legal message text.
scoped_ptr<views::View> SaveCardBubbleViews::CreateFootnoteView() {
  // Use BoxLayout to provide insets around the label.
  scoped_ptr<View> view(new View());
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           GetBubbleFrameView()->GetTitleInsets().left(),
                           views::kRelatedControlVerticalSpacing, 0));
  view->SetBorder(
      views::Border::CreateSolidSidedBorder(1, 0, 0, 0, kSubtleBorderColor));
  view->set_background(
      views::Background::CreateSolidBackground(kLightShadingColor));

  // Add a StyledLabel for each line of the legal message.
  for (const SaveCardBubbleController::LegalMessageLine& line :
       controller_->GetLegalMessageLines()) {
    view->AddChildView(CreateLegalMessageLineLabel(line, this).release());
  }

  return view;
}

void SaveCardBubbleViews::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(CreateMainContentView().release());
  if (!controller_->GetLegalMessageLines().empty())
    AddChildView(CreateFootnoteView().release());

  set_margins(gfx::Insets(1, 0, 1, 0));
}

}  // namespace autofill
