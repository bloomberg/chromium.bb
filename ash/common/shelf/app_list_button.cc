// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/app_list_button.h"

#include "ash/common/shelf/ink_drop_button_listener.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_view.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/ptr_util.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/painter.h"

namespace ash {

AppListButton::AppListButton(InkDropButtonListener* listener,
                             ShelfView* shelf_view,
                             WmShelf* wm_shelf)
    : views::ImageButton(nullptr),
      is_showing_app_list_(false),
      background_color_(kShelfDefaultBaseColor),
      listener_(listener),
      shelf_view_(shelf_view),
      wm_shelf_(wm_shelf) {
  DCHECK(listener_);
  DCHECK(shelf_view_);
  DCHECK(wm_shelf_);

  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
  set_ink_drop_base_color(kShelfInkDropBaseColor);
  set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE));
  SetSize(
      gfx::Size(GetShelfConstant(SHELF_SIZE), GetShelfConstant(SHELF_SIZE)));
  SetFocusPainter(TrayPopupUtils::CreateFocusPainter());
  set_notify_action(CustomButton::NOTIFY_ON_PRESS);
}

AppListButton::~AppListButton() {}

void AppListButton::OnAppListShown() {
  AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
  is_showing_app_list_ = true;
  wm_shelf_->UpdateAutoHideState();
}

void AppListButton::OnAppListDismissed() {
  AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  is_showing_app_list_ = false;
  wm_shelf_->UpdateAutoHideState();
}

void AppListButton::UpdateShelfItemBackground(SkColor color) {
  background_color_ = color;
  SchedulePaint();
}

bool AppListButton::OnMousePressed(const ui::MouseEvent& event) {
  ImageButton::OnMousePressed(event);
  shelf_view_->PointerPressedOnButton(this, ShelfView::MOUSE, event);
  return true;
}

void AppListButton::OnMouseReleased(const ui::MouseEvent& event) {
  ImageButton::OnMouseReleased(event);
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, false);
}

void AppListButton::OnMouseCaptureLost() {
  shelf_view_->PointerReleasedOnButton(this, ShelfView::MOUSE, true);
  ImageButton::OnMouseCaptureLost();
}

bool AppListButton::OnMouseDragged(const ui::MouseEvent& event) {
  ImageButton::OnMouseDragged(event);
  shelf_view_->PointerDraggedOnButton(this, ShelfView::MOUSE, event);
  return true;
}

void AppListButton::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      AnimateInkDrop(views::InkDropState::HIDDEN, event);
      shelf_view_->PointerPressedOnButton(this, ShelfView::TOUCH, *event);
      event->SetHandled();
      return;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      shelf_view_->PointerDraggedOnButton(this, ShelfView::TOUCH, *event);
      event->SetHandled();
      return;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      shelf_view_->PointerReleasedOnButton(this, ShelfView::TOUCH, false);
      event->SetHandled();
      return;
    case ui::ET_GESTURE_TAP_DOWN:
      if (!WmShell::Get()->IsApplistVisible())
        AnimateInkDrop(views::InkDropState::ACTION_PENDING, event);
      ImageButton::OnGestureEvent(event);
      break;
    default:
      ImageButton::OnGestureEvent(event);
      return;
  }
}

void AppListButton::OnPaint(gfx::Canvas* canvas) {
  // Call the base class first to paint any background/borders.
  View::OnPaint(canvas);

  gfx::PointF circle_center(GetCenterPoint());

  // Paint the circular background.
  cc::PaintFlags bg_flags;
  bg_flags.setColor(background_color_);
  bg_flags.setAntiAlias(true);
  bg_flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(circle_center, kAppListButtonRadius, bg_flags);

  // Paint a white ring as the foreground. The ceil/dsf math assures that the
  // ring draws sharply and is centered at all scale factors.
  const float kRingOuterRadiusDp = 7.f;
  const float kRingThicknessDp = 1.5f;

  {
    gfx::ScopedCanvas scoped_canvas(canvas);
    const float dsf = canvas->UndoDeviceScaleFactor();
    circle_center.Scale(dsf);

    cc::PaintFlags fg_flags;
    fg_flags.setAntiAlias(true);
    fg_flags.setStyle(cc::PaintFlags::kStroke_Style);
    fg_flags.setColor(kShelfIconColor);
    const float thickness = std::ceil(kRingThicknessDp * dsf);
    const float radius = std::ceil(kRingOuterRadiusDp * dsf) - thickness / 2;
    fg_flags.setStrokeWidth(thickness);
    // Make sure the center of the circle lands on pixel centers.
    canvas->DrawCircle(circle_center, radius, fg_flags);
  }

  views::Painter::PaintFocusPainter(this, canvas, focus_painter());
}

void AppListButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_BUTTON;
  node_data->SetName(shelf_view_->GetTitleForView(this));
}

std::unique_ptr<views::InkDropRipple> AppListButton::CreateInkDropRipple()
    const {
  gfx::Point center = GetCenterPoint();
  gfx::Rect bounds(center.x() - kAppListButtonRadius,
                   center.y() - kAppListButtonRadius, 2 * kAppListButtonRadius,
                   2 * kAppListButtonRadius);
  return base::MakeUnique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

void AppListButton::NotifyClick(const ui::Event& event) {
  ImageButton::NotifyClick(event);
  if (listener_)
    listener_->ButtonPressed(this, event, GetInkDrop());
}

bool AppListButton::ShouldEnterPushedState(const ui::Event& event) {
  if (!shelf_view_->ShouldEventActivateButton(this, event))
    return false;
  if (WmShell::Get()->IsApplistVisible())
    return false;
  return views::ImageButton::ShouldEnterPushedState(event);
}

std::unique_ptr<views::InkDrop> AppListButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      CustomButton::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropMask> AppListButton::CreateInkDropMask() const {
  return base::MakeUnique<views::CircleInkDropMask>(size(), GetCenterPoint(),
                                                    kAppListButtonRadius);
}

gfx::Point AppListButton::GetCenterPoint() const {
  // For a bottom-aligned shelf, the button bounds could have a larger height
  // than width (in the case of touch-dragging the shelf updwards) or a larger
  // width than height (in the case of a shelf hide/show animation), so adjust
  // the y-position of the circle's center to ensure correct layout. Similarly
  // adjust the x-position for a left- or right-aligned shelf.
  const int x_mid = width() / 2.f;
  const int y_mid = height() / 2.f;
  ShelfAlignment alignment = wm_shelf_->GetAlignment();
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    return gfx::Point(x_mid, x_mid);
  } else if (alignment == SHELF_ALIGNMENT_RIGHT) {
    return gfx::Point(y_mid, y_mid);
  } else {
    DCHECK_EQ(alignment, SHELF_ALIGNMENT_LEFT);
    return gfx::Point(width() - y_mid, y_mid);
  }
}

}  // namespace ash
