// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/overflow_bubble.h"

#include <algorithm>

#include "ash/launcher/launcher_types.h"
#include "ash/launcher/launcher_view.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"

namespace ash {
namespace internal {

namespace {

// This should be the same color as the darkest launcher bar.
const SkColor kLauncherColor = SkColorSetARGB(0x80, 0, 0, 0);

// Max bubble size to screen size ratio.
const float kMaxBubbleSizeToScreenRatio = 0.5f;

// Inner padding in pixels for launcher view inside bubble.
const int kPadding = 2;

// Padding space in pixels between LauncherView's left/top edge to its contents.
const int kLauncherViewLeadingInset = 8;

// Gets arrow location based on shelf alignment.
views::BubbleBorder::ArrowLocation GetBubbleArrowLocation(
    ShelfAlignment shelf_alignment) {
  switch (shelf_alignment) {
    case ash::SHELF_ALIGNMENT_BOTTOM:
      return views::BubbleBorder::BOTTOM_LEFT;
    case ash::SHELF_ALIGNMENT_LEFT:
      return views::BubbleBorder::LEFT_TOP;
    case ash::SHELF_ALIGNMENT_RIGHT:
      return views::BubbleBorder::RIGHT_TOP;
    default:
      NOTREACHED() << "Unknown shelf alignment " << shelf_alignment;
      return views::BubbleBorder::BOTTOM_LEFT;
  }
}

////////////////////////////////////////////////////////////////////////////////
// OverflowBubbleView
// OverflowBubbleView hosts a LauncherView to display overflown items.

class OverflowBubbleView : public views::BubbleDelegateView {
 public:
  OverflowBubbleView();
  virtual ~OverflowBubbleView();

  void InitOverflowBubble(LauncherDelegate* delegate,
                          LauncherModel* model,
                          views::View* anchor,
                          ShelfAlignment shelf_alignment,
                          int overflow_start_index);

 private:
  bool is_horizontal_alignment() const {
    return shelf_alignment_ == SHELF_ALIGNMENT_BOTTOM;
  }

  const gfx::Size GetContentsSize() const {
    return static_cast<views::View*>(launcher_view_)->GetPreferredSize();
  }

  void ScrollByXOffset(int x_offset);
  void ScrollByYOffset(int y_offset);

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;
  virtual bool OnMouseWheel(const views::MouseWheelEvent& event) OVERRIDE;
  virtual bool OnScrollEvent(const views::ScrollEvent & event) OVERRIDE;

  // views::BubbleDelegate overrides:
  virtual gfx::Rect GetBubbleBounds() OVERRIDE;

  ShelfAlignment shelf_alignment_;
  LauncherView* launcher_view_;  // Owned by views hierarchy.
  gfx::Point scroll_offset_;

  DISALLOW_COPY_AND_ASSIGN(OverflowBubbleView);
};

OverflowBubbleView::OverflowBubbleView()
    : shelf_alignment_(SHELF_ALIGNMENT_BOTTOM),
      launcher_view_(NULL) {
}

OverflowBubbleView::~OverflowBubbleView() {
}

void OverflowBubbleView::InitOverflowBubble(LauncherDelegate* delegate,
                                            LauncherModel* model,
                                            views::View* anchor,
                                            ShelfAlignment shelf_alignment,
                                            int overflow_start_index) {
  shelf_alignment_ = shelf_alignment;

  // Makes bubble view has a layer and clip its children layers.
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  launcher_view_ = new LauncherView(model, delegate, NULL);
  launcher_view_->set_first_visible_index(overflow_start_index);
  launcher_view_->set_leading_inset(kLauncherViewLeadingInset);
  launcher_view_->Init();
  launcher_view_->SetAlignment(shelf_alignment);
  AddChildView(launcher_view_);

  set_anchor_view(anchor);
  set_arrow_location(GetBubbleArrowLocation(shelf_alignment));
  set_background(NULL);
  set_color(kLauncherColor);
  set_margins(gfx::Insets(kPadding, kPadding, kPadding, kPadding));
  set_move_with_anchor(true);
  views::BubbleDelegateView::CreateBubble(this);
}

void OverflowBubbleView::ScrollByXOffset(int x_offset) {
  const gfx::Rect visible_bounds(GetContentsBounds());
  const gfx::Size contents_size(GetContentsSize());

  int x = std::min(contents_size.width() - visible_bounds.width(),
                   std::max(0, scroll_offset_.x() + x_offset));
  scroll_offset_.set_x(x);
}

void OverflowBubbleView::ScrollByYOffset(int y_offset) {
  const gfx::Rect visible_bounds(GetContentsBounds());
  const gfx::Size contents_size(GetContentsSize());

  int y = std::min(contents_size.height() - visible_bounds.height(),
                   std::max(0, scroll_offset_.y() + y_offset));
  scroll_offset_.set_y(y);
}

gfx::Size OverflowBubbleView::GetPreferredSize() {
  gfx::Size preferred_size = GetContentsSize();

  const gfx::Rect monitor_rect = gfx::Screen::GetDisplayNearestPoint(
      GetAnchorRect().CenterPoint()).work_area();
  if (!monitor_rect.IsEmpty()) {
    if (is_horizontal_alignment()) {
      preferred_size.set_width(std::min(
          preferred_size.width(),
          static_cast<int>(monitor_rect.width() *
                           kMaxBubbleSizeToScreenRatio)));
    } else {
      preferred_size.set_height(std::min(
          preferred_size.height(),
          static_cast<int>(monitor_rect.height() *
                           kMaxBubbleSizeToScreenRatio)));
    }
  }

  return preferred_size;
}

void OverflowBubbleView::Layout() {
  const gfx::Point origin(-scroll_offset_.x(), -scroll_offset_.y());
  launcher_view_->SetBoundsRect(gfx::Rect(origin, GetContentsSize()));
}

void OverflowBubbleView::ChildPreferredSizeChanged(views::View* child) {
  // Ensures |launch_view_| is still visible.
  ScrollByXOffset(0);
  ScrollByYOffset(0);
  Layout();

  SizeToContents();
}

bool OverflowBubbleView::OnMouseWheel(const views::MouseWheelEvent& event) {
  if (is_horizontal_alignment())
    ScrollByXOffset(-event.offset());
  else
    ScrollByYOffset(-event.offset());
  Layout();

  return true;
}

bool OverflowBubbleView::OnScrollEvent(const views::ScrollEvent & event) {
  ScrollByXOffset(-event.x_offset());
  ScrollByYOffset(-event.y_offset());
  Layout();
  return true;
}

gfx::Rect OverflowBubbleView::GetBubbleBounds() {
  views::BubbleBorder* border = GetBubbleFrameView()->bubble_border();
  gfx::Insets bubble_insets;
  border->GetInsets(&bubble_insets);

  const int border_size =
      views::BubbleBorder::is_arrow_on_horizontal(arrow_location()) ?
      bubble_insets.left() : bubble_insets.top();
  const int arrow_offset = border_size + kPadding + kLauncherViewLeadingInset +
      kLauncherPreferredSize / 2;

  const gfx::Size content_size = GetPreferredSize();
  border->SetArrowOffset(arrow_offset, content_size);

  const gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect bubble_rect = GetBubbleFrameView()->GetUpdatedWindowBounds(
      anchor_rect,
      content_size,
      false);

  gfx::Rect monitor_rect = gfx::Screen::GetDisplayNearestPoint(
      anchor_rect.CenterPoint()).work_area();

  int offset = 0;
  if (views::BubbleBorder::is_arrow_on_horizontal(arrow_location())) {
    if (bubble_rect.x() < monitor_rect.x())
      offset = monitor_rect.x() - bubble_rect.x();
    else if (bubble_rect.right() > monitor_rect.right())
      offset = monitor_rect.right() - bubble_rect.right();

    bubble_rect.Offset(offset, 0);
    border->SetArrowOffset(anchor_rect.CenterPoint().x() - bubble_rect.x(),
                           content_size);
  } else {
    if (bubble_rect.y() < monitor_rect.y())
      offset = monitor_rect.y() - bubble_rect.y();
    else if (bubble_rect.bottom() > monitor_rect.bottom())
      offset =  monitor_rect.bottom() - bubble_rect.bottom();

    bubble_rect.Offset(0, offset);
    border->SetArrowOffset(anchor_rect.CenterPoint().y() - bubble_rect.y(),
                           content_size);
  }

  GetBubbleFrameView()->SchedulePaint();
  return bubble_rect;
}

}  // namespace

OverflowBubble::OverflowBubble()
    : bubble_(NULL) {
}

OverflowBubble::~OverflowBubble() {
  Hide();
}

void OverflowBubble::Show(LauncherDelegate* delegate,
                          LauncherModel* model,
                          views::View* anchor,
                          ShelfAlignment shelf_alignment,
                          int overflow_start_index) {
  Hide();

  OverflowBubbleView* bubble_view = new OverflowBubbleView();
  bubble_view->InitOverflowBubble(delegate,
                                  model,
                                  anchor,
                                  shelf_alignment,
                                  overflow_start_index);

  bubble_ = bubble_view;
  bubble_->GetWidget()->AddObserver(this);
  bubble_->GetWidget()->Show();
}

void OverflowBubble::Hide() {
  if (!IsShowing())
    return;

  bubble_->GetWidget()->RemoveObserver(this);
  bubble_->GetWidget()->Close();
  bubble_ = NULL;
}

void OverflowBubble::OnWidgetClosing(views::Widget* widget) {
  DCHECK(widget == bubble_->GetWidget());
  bubble_ = NULL;
}

}  // namespace internal
}  // namespace ash
