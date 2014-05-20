// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_bubble_view.h"

#include <algorithm>

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ui/events/event.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Max bubble size to screen size ratio.
const float kMaxBubbleSizeToScreenRatio = 0.5f;

// Inner padding in pixels for shelf view inside bubble.
const int kPadding = 2;

// Padding space in pixels between ShelfView's left/top edge to its contents.
const int kShelfViewLeadingInset = 8;

}  // namespace

OverflowBubbleView::OverflowBubbleView()
    : shelf_view_(NULL) {
}

OverflowBubbleView::~OverflowBubbleView() {
}

void OverflowBubbleView::InitOverflowBubble(views::View* anchor,
                                            ShelfView* shelf_view) {
  // set_anchor_view needs to be called before GetShelfLayoutManager() can be
  // called.
  SetAnchorView(anchor);
  set_arrow(GetBubbleArrow());
  set_background(NULL);
  set_color(SkColorSetARGB(kShelfBackgroundAlpha, 0, 0, 0));
  set_margins(gfx::Insets(kPadding, kPadding, kPadding, kPadding));
  // Overflow bubble should not get focus. If it get focus when it is shown,
  // active state item is changed to running state.
  set_use_focusless(true);

  // Makes bubble view has a layer and clip its children layers.
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);

  shelf_view_ = shelf_view;
  AddChildView(shelf_view_);

  set_parent_window(Shell::GetContainer(
      anchor->GetWidget()->GetNativeWindow()->GetRootWindow(),
      kShellWindowId_ShelfBubbleContainer));
  views::BubbleDelegateView::CreateBubble(this);
}

bool OverflowBubbleView::IsHorizontalAlignment() const {
  ShelfLayoutManager* shelf_layout_manager = GetShelfLayoutManager();
  return shelf_layout_manager ? shelf_layout_manager->IsHorizontalAlignment()
                              : false;
}

const gfx::Size OverflowBubbleView::GetContentsSize() const {
  return static_cast<views::View*>(shelf_view_)->GetPreferredSize();
}

// Gets arrow location based on shelf alignment.
views::BubbleBorder::Arrow OverflowBubbleView::GetBubbleArrow() const {
  ShelfLayoutManager* shelf_layout_manager = GetShelfLayoutManager();
  return shelf_layout_manager ?
      shelf_layout_manager->SelectValueForShelfAlignment(
          views::BubbleBorder::BOTTOM_LEFT,
          views::BubbleBorder::LEFT_TOP,
          views::BubbleBorder::RIGHT_TOP,
          views::BubbleBorder::TOP_LEFT) :
      views::BubbleBorder::NONE;
}

void OverflowBubbleView::ScrollByXOffset(int x_offset) {
  const gfx::Rect visible_bounds(GetContentsBounds());
  const gfx::Size contents_size(GetContentsSize());

  DCHECK_GE(contents_size.width(), visible_bounds.width());
  int x = std::min(contents_size.width() - visible_bounds.width(),
                   std::max(0, scroll_offset_.x() + x_offset));
  scroll_offset_.set_x(x);
}

void OverflowBubbleView::ScrollByYOffset(int y_offset) {
  const gfx::Rect visible_bounds(GetContentsBounds());
  const gfx::Size contents_size(GetContentsSize());

  DCHECK_GE(contents_size.width(), visible_bounds.width());
  int y = std::min(contents_size.height() - visible_bounds.height(),
                   std::max(0, scroll_offset_.y() + y_offset));
  scroll_offset_.set_y(y);
}

gfx::Size OverflowBubbleView::GetPreferredSize() const {
  gfx::Size preferred_size = GetContentsSize();

  const gfx::Rect monitor_rect = Shell::GetScreen()->GetDisplayNearestPoint(
      GetAnchorRect().CenterPoint()).work_area();
  if (!monitor_rect.IsEmpty()) {
    if (IsHorizontalAlignment()) {
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
  shelf_view_->SetBoundsRect(gfx::Rect(
      gfx::PointAtOffsetFromOrigin(-scroll_offset_), GetContentsSize()));
}

void OverflowBubbleView::ChildPreferredSizeChanged(views::View* child) {
  // When contents size is changed, ContentsBounds should be updated before
  // calculating scroll offset.
  SizeToContents();

  // Ensures |shelf_view_| is still visible.
  if (IsHorizontalAlignment())
    ScrollByXOffset(0);
  else
    ScrollByYOffset(0);
  Layout();
}

bool OverflowBubbleView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // The MouseWheelEvent was changed to support both X and Y offsets
  // recently, but the behavior of this function was retained to continue
  // using Y offsets only. Might be good to simply scroll in both
  // directions as in OverflowBubbleView::OnScrollEvent.
  if (IsHorizontalAlignment())
    ScrollByXOffset(-event.y_offset());
  else
    ScrollByYOffset(-event.y_offset());
  Layout();

  return true;
}

ShelfLayoutManager* OverflowBubbleView::GetShelfLayoutManager() const {
  return GetAnchorView() ? ShelfLayoutManager::ForShelf(
                               GetAnchorView()->GetWidget()->GetNativeView())
                         : NULL;
}

void OverflowBubbleView::OnScrollEvent(ui::ScrollEvent* event) {
  ScrollByXOffset(-event->x_offset());
  ScrollByYOffset(-event->y_offset());
  Layout();
  event->SetHandled();
}

gfx::Rect OverflowBubbleView::GetBubbleBounds() {
  views::BubbleBorder* border = GetBubbleFrameView()->bubble_border();
  gfx::Insets bubble_insets = border->GetInsets();

  const int border_size =
      views::BubbleBorder::is_arrow_on_horizontal(arrow()) ?
      bubble_insets.left() : bubble_insets.top();
  const int arrow_offset = border_size + kPadding + kShelfViewLeadingInset +
      kShelfSize / 2;

  const gfx::Size content_size = GetPreferredSize();
  border->set_arrow_offset(arrow_offset);

  const gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect bubble_rect = GetBubbleFrameView()->GetUpdatedWindowBounds(
      anchor_rect,
      content_size,
      false);

  gfx::Rect monitor_rect = Shell::GetScreen()->GetDisplayNearestPoint(
      anchor_rect.CenterPoint()).work_area();

  int offset = 0;
  if (views::BubbleBorder::is_arrow_on_horizontal(arrow())) {
    if (bubble_rect.x() < monitor_rect.x())
      offset = monitor_rect.x() - bubble_rect.x();
    else if (bubble_rect.right() > monitor_rect.right())
      offset = monitor_rect.right() - bubble_rect.right();

    bubble_rect.Offset(offset, 0);
    border->set_arrow_offset(anchor_rect.CenterPoint().x() - bubble_rect.x());
  } else {
    if (bubble_rect.y() < monitor_rect.y())
      offset = monitor_rect.y() - bubble_rect.y();
    else if (bubble_rect.bottom() > monitor_rect.bottom())
      offset =  monitor_rect.bottom() - bubble_rect.bottom();

    bubble_rect.Offset(0, offset);
    border->set_arrow_offset(anchor_rect.CenterPoint().y() - bubble_rect.y());
  }

  GetBubbleFrameView()->SchedulePaint();
  return bubble_rect;
}

}  // namespace ash
