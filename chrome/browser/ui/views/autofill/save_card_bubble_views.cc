// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_card_bubble_views.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

using views::GridLayout;

namespace {

// Fixed width of the column holding the message text.
const int kWidthOfMessageText = 375;

// TODO(bondd): BubbleManager will eventually move this logic somewhere else,
// and then kIsOkButtonOnLeftSide can be removed from here and
// dialog_client_view.cc.
#if defined(OS_WIN) || defined(OS_CHROMEOS)
const bool kIsOkButtonOnLeftSide = true;
#else
const bool kIsOkButtonOnLeftSide = false;
#endif

}  // namespace

namespace autofill {

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
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_BUBBLE_TITLE_LOCAL);
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

void SaveCardBubbleViews::Init() {
  enum {
    COLUMN_SET_ID_MESSAGE,
    COLUMN_SET_ID_BUTTONS,
  };

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  // Set up ColumnSet that will contain the full-width message text.
  int horizontal_inset = GetBubbleFrameView()->GetTitleInsets().left();
  views::ColumnSet* cs = layout->AddColumnSet(COLUMN_SET_ID_MESSAGE);
  cs->AddPaddingColumn(0, horizontal_inset);
  // TODO(bondd): Current dialog layout has no message text, but future layouts
  // will. This padding column is used until then to set the dialog width.
  cs->AddPaddingColumn(1, kWidthOfMessageText);
  cs->AddPaddingColumn(0, horizontal_inset);

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
  learn_more_link_->set_listener(this);
  layout->StartRow(0, COLUMN_SET_ID_BUTTONS);
  layout->AddView(learn_more_link_);

  // Create accept button.
  save_button_ = new views::BlueButton(
      this, l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_BUBBLE_ACCEPT));
  save_button_->SetIsDefault(true);

  // Create cancel button.
  cancel_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_CARD_BUBBLE_DENY));
  cancel_button_->SetStyle(views::Button::STYLE_BUTTON);

  if (kIsOkButtonOnLeftSide) {
    layout->AddView(save_button_);
    layout->AddView(cancel_button_);
  } else {
    layout->AddView(cancel_button_);
    layout->AddView(save_button_);
  }
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  set_margins(gfx::Insets(1, 0, 1, 0));
  Layout();
}

}  // namespace autofill
