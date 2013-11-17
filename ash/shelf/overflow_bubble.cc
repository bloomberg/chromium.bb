// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_bubble.h"

#include <algorithm>

#include "ash/launcher/launcher_types.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/root_window.h"
#include "ui/events/event.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

// Max bubble size to screen size ratio.
const float kMaxBubbleSizeToScreenRatio = 0.5f;

// Inner padding in pixels for shelf view inside bubble.
const int kPadding = 2;

// Padding space in pixels between ShelfView's left/top edge to its contents.
const int kShelfViewLeadingInset = 8;

////////////////////////////////////////////////////////////////////////////////
// OverflowBubbleView
// OverflowBubbleView hosts a ShelfView to display overflown items.

class OverflowBubbleView : public views::BubbleDelegateView {
 public:
  OverflowBubbleView();
  virtual ~OverflowBubbleView();

  void InitOverflowBubble(views::View* anchor, ShelfView* shelf_view);

 private:
  bool IsHorizontalAlignment() const {
    return GetShelfLayoutManagerForLauncher()->IsHorizontalAlignment();
  }

  const gfx::Size GetContentsSize() const {
    return static_cast<views::View*>(shelf_view_)->GetPreferredSize();
  }

  // Gets arrow location based on shelf alignment.
  views::BubbleBorder::Arrow GetBubbleArrow() const {
    return GetShelfLayoutManagerForLauncher()->SelectValueForShelfAlignment(
        views::BubbleBorder::BOTTOM_LEFT,
        views::BubbleBorder::LEFT_TOP,
        views::BubbleBorder::RIGHT_TOP,
        views::BubbleBorder::TOP_LEFT);
  }

  void ScrollByXOffset(int x_offset);
  void ScrollByYOffset(int y_offset);

  // views::View overrides:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;
  virtual bool OnMouseWheel(const ui::MouseWheelEvent& event) OVERRIDE;

  // ui::EventHandler overrides:
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;

  // views::BubbleDelegate overrides:
  virtual gfx::Rect GetBubbleBounds() OVERRIDE;

  ShelfLayoutManager* GetShelfLayoutManagerForLauncher() const {
    return ShelfLayoutManager::ForLauncher(
        GetAnchorView()->GetWidget()->GetNativeView());
  }

  ShelfView* shelf_view_;  // Owned by views hierarchy.
  gfx::Vector2d scroll_offset_;

  DISALLOW_COPY_AND_ASSIGN(OverflowBubbleView);
};

OverflowBubbleView::OverflowBubbleView()
    : shelf_view_(NULL) {
}

OverflowBubbleView::~OverflowBubbleView() {
}

void OverflowBubbleView::InitOverflowBubble(views::View* anchor,
                                            ShelfView* shelf_view) {
  // set_anchor_view needs to be called before GetShelfLayoutManagerForLauncher
  // can be called.
  SetAnchorView(anchor);
  set_arrow(GetBubbleArrow());
  set_background(NULL);
  set_color(SkColorSetARGB(kLauncherBackgroundAlpha, 0, 0, 0));
  set_margins(gfx::Insets(kPadding, kPadding, kPadding, kPadding));
  set_move_with_anchor(true);
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
        internal::kShellWindowId_ShelfBubbleContainer));
  views::BubbleDelegateView::CreateBubble(this);
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

gfx::Size OverflowBubbleView::GetPreferredSize() {
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
      ShelfLayoutManager::GetPreferredShelfSize() / 2;

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

}  // namespace

OverflowBubble::OverflowBubble()
    : bubble_(NULL),
      anchor_(NULL),
      shelf_view_(NULL) {
}

OverflowBubble::~OverflowBubble() {
  Hide();
}

void OverflowBubble::Show(views::View* anchor, ShelfView* shelf_view) {
  Hide();

  OverflowBubbleView* bubble_view = new OverflowBubbleView();
  bubble_view->InitOverflowBubble(anchor, shelf_view);
  shelf_view_ = shelf_view;
  anchor_ = anchor;

  Shell::GetInstance()->AddPreTargetHandler(this);

  bubble_ = bubble_view;
  RootWindowController::ForWindow(anchor->GetWidget()->GetNativeView())->
      GetSystemTray()->InitializeBubbleAnimations(bubble_->GetWidget());
  bubble_->GetWidget()->AddObserver(this);
  bubble_->GetWidget()->Show();
}

void OverflowBubble::Hide() {
  if (!IsShowing())
    return;

  Shell::GetInstance()->RemovePreTargetHandler(this);
  bubble_->GetWidget()->RemoveObserver(this);
  bubble_->GetWidget()->Close();
  bubble_ = NULL;
  anchor_ = NULL;
  shelf_view_ = NULL;
}

void OverflowBubble::HideBubbleAndRefreshButton() {
  if (!IsShowing())
    return;

  views::View* anchor = anchor_;
  Hide();
  // Update overflow button (|anchor|) status when overflow bubble is hidden
  // by outside event of overflow button.
  anchor->SchedulePaint();
}

void OverflowBubble::ProcessPressedEvent(ui::LocatedEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::Point event_location_in_screen = event->location();
  aura::client::GetScreenPositionClient(target->GetRootWindow())->
      ConvertPointToScreen(target, &event_location_in_screen);
  if (!shelf_view_->IsShowingMenu() &&
      !bubble_->GetBoundsInScreen().Contains(event_location_in_screen) &&
      !anchor_->GetBoundsInScreen().Contains(event_location_in_screen)) {
    HideBubbleAndRefreshButton();
  }
}

void OverflowBubble::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessPressedEvent(event);
}

void OverflowBubble::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    ProcessPressedEvent(event);
}

void OverflowBubble::OnWidgetDestroying(views::Widget* widget) {
  DCHECK(widget == bubble_->GetWidget());
  bubble_ = NULL;
  anchor_ = NULL;
  shelf_view_ = NULL;
  ShelfLayoutManager::ForLauncher(
      widget->GetNativeView())->shelf_widget()->launcher()->SchedulePaint();
}

}  // namespace internal
}  // namespace ash
