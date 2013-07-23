// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_credit_card_bubble_views.h"

#include "chrome/browser/ui/autofill/autofill_credit_card_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/gfx/font.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/size.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace autofill {

AutofillCreditCardBubbleViews::~AutofillCreditCardBubbleViews() {}

void AutofillCreditCardBubbleViews::Show() {
  // TODO(dbeam): investigate why this steals focus from the web contents.
  views::BubbleDelegateView::CreateBubble(this);

  GetBubbleFrameView()->SetTitle(controller_->BubbleTitle());

  GetWidget()->Show();
  SizeToContents();
}

void AutofillCreditCardBubbleViews::Hide() {
  GetWidget()->Close();
}

bool AutofillCreditCardBubbleViews::IsHiding() const {
  return GetWidget() && GetWidget()->IsClosed();
}

void AutofillCreditCardBubbleViews::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                                        views::kRelatedControlVerticalSpacing));

  views::StyledLabel::RangeStyleInfo bold;
  bold.font_style = gfx::Font::BOLD;
  const std::vector<ui::Range>& ranges = controller_->BubbleTextRanges();

  views::StyledLabel* contents =
      new views::StyledLabel(controller_->BubbleText(), NULL);
  for (size_t i = 0; i < ranges.size(); ++i) {
    contents->AddStyleRange(ranges[i], bold);
  }
  AddChildView(contents);

  views::Link* link = new views::Link();
  link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link->SetText(controller_->LinkText());
  link->set_listener(this);
  AddChildView(link);
}

gfx::Size AutofillCreditCardBubbleViews::GetPreferredSize() {
  return gfx::Size(
      AutofillCreditCardBubbleViews::kContentWidth,
      GetHeightForWidth(AutofillCreditCardBubble::kContentWidth));
}

void AutofillCreditCardBubbleViews::LinkClicked(views::Link* source,
                                                int event_flags) {
  if (controller_)
    controller_->OnLinkClicked();
}

// static
base::WeakPtr<AutofillCreditCardBubble> AutofillCreditCardBubble::Create(
    const base::WeakPtr<AutofillCreditCardBubbleController>& controller) {
  AutofillCreditCardBubbleViews* bubble =
      new AutofillCreditCardBubbleViews(controller);
  return bubble->weak_ptr_factory_.GetWeakPtr();
}

AutofillCreditCardBubbleViews::AutofillCreditCardBubbleViews(
    const base::WeakPtr<AutofillCreditCardBubbleController>& controller)
    : BubbleDelegateView(BrowserView::GetBrowserViewForBrowser(
          chrome::FindBrowserWithWebContents(controller->web_contents()))->
              GetLocationBarView()->autofill_credit_card_view(),
        views::BubbleBorder::TOP_RIGHT),
      controller_(controller),
      weak_ptr_factory_(this) {
  // Match bookmarks bubble view's anchor view insets and margins.
  set_anchor_view_insets(gfx::Insets(7, 0, 7, 0));
  set_margins(gfx::Insets(0, 19, 18, 18));
  set_move_with_anchor(true);
}

}  // namespace autofill
