// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/info_bubble.h"

#include "base/i18n/rtl.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

// The visible width of bubble borders (differs from the actual width) in px.
const int kBubbleBorderVisibleWidth = 1;

// The margin between the content of the error bubble and its border.
const int kInfoBubbleHorizontalMargin = 14;
const int kInfoBubbleVerticalMargin = 12;

}  // namespace

InfoBubble::InfoBubble(views::View* anchor, const base::string16& message)
    : anchor_(anchor),
      align_to_anchor_edge_(false),
      preferred_width_(233),
      show_above_anchor_(false) {
  DCHECK(anchor_);
  SetAnchorView(anchor_);

  set_margins(gfx::Insets(kInfoBubbleVerticalMargin,
                          kInfoBubbleHorizontalMargin,
                          kInfoBubbleVerticalMargin,
                          kInfoBubbleHorizontalMargin));
  set_use_focusless(true);

  SetLayoutManager(new views::FillLayout);
  views::Label* label = new views::Label(message);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  AddChildView(label);
}

InfoBubble::~InfoBubble() {}

void InfoBubble::Show() {
  // TODO(dbeam): currently we assume that combobox menus always show downward
  // (which isn't true). If the invalid combobox is low enough on the screen,
  // its menu will actually show upward and obscure the bubble. Figure out when
  // this might happen and adjust |show_above_anchor_| accordingly. This is not
  // that big of deal because it rarely happens in practice.
  if (show_above_anchor_) {
    set_arrow(ShouldArrowGoOnTheRight() ? views::BubbleBorder::BOTTOM_RIGHT :
                                          views::BubbleBorder::BOTTOM_LEFT);
  } else {
    set_arrow(ShouldArrowGoOnTheRight() ? views::BubbleBorder::TOP_RIGHT :
                                          views::BubbleBorder::TOP_LEFT);
  }

  widget_ = views::BubbleDelegateView::CreateBubble(this);
  UpdatePosition();
}

void InfoBubble::Hide() {
  views::Widget* widget = GetWidget();
  if (widget && !widget->IsClosed())
    widget->Close();
}

void InfoBubble::UpdatePosition() {
  if (!widget_)
    return;

  if (!anchor_->GetVisibleBounds().IsEmpty()) {
    SizeToContents();
    widget_->SetVisibilityChangedAnimationsEnabled(true);
    widget_->ShowInactive();
  } else {
    widget_->SetVisibilityChangedAnimationsEnabled(false);
    widget_->Hide();
  }
}

gfx::Size InfoBubble::GetPreferredSize() {
  int pref_width = preferred_width_;
  pref_width -= GetBubbleFrameView()->GetInsets().width();
  pref_width -= 2 * kBubbleBorderVisibleWidth;
  return gfx::Size(pref_width, GetHeightForWidth(pref_width));
}

gfx::Rect InfoBubble::GetBubbleBounds() {
  gfx::Rect bounds = views::BubbleDelegateView::GetBubbleBounds();
  gfx::Rect anchor_rect = GetAnchorRect();

  if (show_above_anchor_)
    bounds.set_y(anchor_rect.y() - GetBubbleFrameView()->height());
  else
    bounds.set_y(anchor_rect.bottom());

  if (align_to_anchor_edge_) {
    anchor_rect.Inset(-GetBubbleFrameView()->bubble_border()->GetInsets());
    bounds.set_x(ShouldArrowGoOnTheRight() ?
        anchor_rect.right() - bounds.width() - kBubbleBorderVisibleWidth :
        anchor_rect.x() + kBubbleBorderVisibleWidth);
  }

  return bounds;
}

void InfoBubble::OnWidgetClosing(views::Widget* widget) {
  if (widget == widget_)
    widget_ = NULL;
}

bool InfoBubble::ShouldFlipArrowForRtl() const {
  return false;
}

bool InfoBubble::ShouldArrowGoOnTheRight() {
  views::View* container = anchor_->GetWidget()->non_client_view();
  DCHECK(container->Contains(anchor_));

  gfx::Point anchor_offset;
  views::View::ConvertPointToTarget(anchor_, container, &anchor_offset);
  anchor_offset.Offset(-container_insets_.left(), 0);

  if (base::i18n::IsRTL()) {
    int anchor_right_x = anchor_offset.x() + anchor_->width();
    return anchor_right_x >= preferred_width_;
  }

  int container_width = container->width() - container_insets_.width();
  return anchor_offset.x() + preferred_width_ > container_width;
}

}  // namespace autofill
