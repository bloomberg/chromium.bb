// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/info_bubble.h"

#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
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

class InfoBubbleFrame : public views::BubbleFrameView {
 public:
  explicit InfoBubbleFrame(const gfx::Insets& content_margins)
      : views::BubbleFrameView(gfx::Insets(), content_margins) {}
  ~InfoBubbleFrame() override {}

  gfx::Rect GetAvailableScreenBounds(const gfx::Rect& rect) const override {
    return available_bounds_;
  }

  void set_available_bounds(const gfx::Rect& available_bounds) {
    available_bounds_ = available_bounds;
  }

 private:
  // Bounds that this frame should try to keep bubbles within (screen coords).
  gfx::Rect available_bounds_;

  DISALLOW_COPY_AND_ASSIGN(InfoBubbleFrame);
};

InfoBubble::InfoBubble(views::View* anchor,
                       const base::string16& message)
    : anchor_(anchor),
      frame_(NULL),
      align_to_anchor_edge_(false),
      preferred_width_(233),
      show_above_anchor_(false) {
  DCHECK(anchor_);
  SetAnchorView(anchor_);

  set_margins(gfx::Insets(kInfoBubbleVerticalMargin,
                          kInfoBubbleHorizontalMargin,
                          kInfoBubbleVerticalMargin,
                          kInfoBubbleHorizontalMargin));
  set_can_activate(false);

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
  if (show_above_anchor_)
    set_arrow(views::BubbleBorder::vertical_mirror(arrow()));

  widget_ = views::BubbleDelegateView::CreateBubble(this);

  if (align_to_anchor_edge_) {
    // The frame adjusts its arrow before the bubble's alignment can be changed.
    // Set the created bubble border back to the original arrow and re-adjust.
    frame_->bubble_border()->set_arrow(arrow());
    SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  }

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

views::NonClientFrameView* InfoBubble::CreateNonClientFrameView(
    views::Widget* widget) {
  DCHECK(!frame_);
  frame_ = new InfoBubbleFrame(margins());
  frame_->set_available_bounds(anchor_widget()->GetWindowBoundsInScreen());
  frame_->SetBubbleBorder(scoped_ptr<views::BubbleBorder>(
      new views::BubbleBorder(arrow(), shadow(), color())));
  return frame_;
}

gfx::Size InfoBubble::GetPreferredSize() const {
  int pref_width = preferred_width_;
  pref_width -= frame_->GetInsets().width();
  pref_width -= 2 * kBubbleBorderVisibleWidth;
  return gfx::Size(pref_width, GetHeightForWidth(pref_width));
}

void InfoBubble::OnWidgetDestroyed(views::Widget* widget) {
  if (widget == widget_)
    widget_ = NULL;
}

void InfoBubble::OnWidgetBoundsChanged(views::Widget* widget,
                                       const gfx::Rect& new_bounds) {
  views::BubbleDelegateView::OnWidgetBoundsChanged(widget, new_bounds);
  if (anchor_widget() == widget)
    frame_->set_available_bounds(widget->GetWindowBoundsInScreen());
}

}  // namespace autofill
