// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/generated_credit_card_bubble_views.h"

#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/gfx/font.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/size.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

// Get the view this bubble will be anchored to via |controller|.
views::View* GetAnchor(
    const base::WeakPtr<GeneratedCreditCardBubbleController>& controller) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(
      chrome::FindBrowserWithWebContents(controller->web_contents()));
  return browser_view->GetLocationBarView()->generated_credit_card_view();
}

}  // namespace

GeneratedCreditCardBubbleViews::~GeneratedCreditCardBubbleViews() {}

void GeneratedCreditCardBubbleViews::Show() {
  views::BubbleDelegateView::CreateBubble(this)->Show();

  // This bubble doesn't render correctly on Windows without calling
  // |SizeToContents()|. This must be called after showing the widget.
  SizeToContents();
}

void GeneratedCreditCardBubbleViews::Hide() {
  GetWidget()->Close();
}

bool GeneratedCreditCardBubbleViews::IsHiding() const {
  return GetWidget() && GetWidget()->IsClosed();
}

gfx::Size GeneratedCreditCardBubbleViews::GetPreferredSize() {
  return gfx::Size(
      GeneratedCreditCardBubbleView::kContentsWidth,
      GetHeightForWidth(GeneratedCreditCardBubbleViews::kContentsWidth));
}

base::string16 GeneratedCreditCardBubbleViews::GetWindowTitle() const {
  return controller_ ? controller_->TitleText() : base::string16();
}

void GeneratedCreditCardBubbleViews::Init() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                                        views::kRelatedControlVerticalSpacing));

  const base::string16& contents_text = controller_->ContentsText();
  views::StyledLabel* contents = new views::StyledLabel(contents_text, this);

  const std::vector<TextRange>& text_ranges = controller_->ContentsTextRanges();
  for (size_t i = 0; i < text_ranges.size(); ++i) {
    views::StyledLabel::RangeStyleInfo style;

    if (text_ranges[i].is_link)
      style = views::StyledLabel::RangeStyleInfo::CreateForLink();
    else
      style.font_style = gfx::Font::BOLD;

    contents->AddStyleRange(text_ranges[i].range, style);
  }

  AddChildView(contents);
}

void GeneratedCreditCardBubbleViews::StyledLabelLinkClicked(const gfx::Range& r,
                                                            int event_flags) {
  if (controller_)
    controller_->OnLinkClicked();
}

// static
base::WeakPtr<GeneratedCreditCardBubbleView>
    GeneratedCreditCardBubbleView::Create(
        const base::WeakPtr<GeneratedCreditCardBubbleController>& controller) {
  return (new GeneratedCreditCardBubbleViews(controller))->weak_ptr_factory_.
      GetWeakPtr();
}

GeneratedCreditCardBubbleViews::GeneratedCreditCardBubbleViews(
    const base::WeakPtr<GeneratedCreditCardBubbleController>& controller)
    : BubbleDelegateView(GetAnchor(controller), views::BubbleBorder::TOP_RIGHT),
      controller_(controller),
      weak_ptr_factory_(this) {
  gfx::Insets insets = views::BubbleFrameView::GetTitleInsets();
  set_margins(gfx::Insets(0, insets.left(), insets.top(), insets.left()));
}

}  // namespace autofill
