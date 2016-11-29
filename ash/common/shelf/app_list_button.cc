// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/app_list_button.h"

#include "ash/common/ash_constants.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/ink_drop_button_listener.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_item_types.h"
#include "ash/common/shelf/shelf_view.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/command_line.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/painter.h"

namespace ash {

AppListButton::AppListButton(InkDropButtonListener* listener,
                             ShelfView* shelf_view,
                             WmShelf* wm_shelf)
    : views::ImageButton(nullptr),
      draw_background_as_active_(false),
      background_alpha_(0),
      listener_(listener),
      shelf_view_(shelf_view),
      wm_shelf_(wm_shelf) {
  DCHECK(listener_);
  DCHECK(shelf_view_);
  DCHECK(wm_shelf_);
  if (ash::MaterialDesignController::IsShelfMaterial()) {
    SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
    set_ink_drop_base_color(kShelfInkDropBaseColor);
    set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);
  }
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE));
  SetSize(
      gfx::Size(GetShelfConstant(SHELF_SIZE), GetShelfConstant(SHELF_SIZE)));
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, gfx::Insets(1, 1, 1, 1)));
  set_notify_action(CustomButton::NOTIFY_ON_PRESS);
}

AppListButton::~AppListButton() {}

void AppListButton::OnAppListShown() {
  if (ash::MaterialDesignController::IsShelfMaterial())
    AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
  else
    SchedulePaint();
}

void AppListButton::OnAppListDismissed() {
  if (ash::MaterialDesignController::IsShelfMaterial())
    AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  else
    SchedulePaint();
}

void AppListButton::SetBackgroundAlpha(int alpha) {
  background_alpha_ = alpha;
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
  const bool is_material = ash::MaterialDesignController::IsShelfMaterial();
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (is_material)
        AnimateInkDrop(views::InkDropState::HIDDEN, event);
      else
        SetDrawBackgroundAsActive(false);
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
      if (!is_material)
        SetDrawBackgroundAsActive(true);
      else if (!WmShell::Get()->IsApplistVisible())
        AnimateInkDrop(views::InkDropState::ACTION_PENDING, event);
      ImageButton::OnGestureEvent(event);
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_TAP:
      if (!is_material)
        SetDrawBackgroundAsActive(false);
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

  if (ash::MaterialDesignController::IsShelfMaterial()) {
    PaintMd(canvas);
  } else {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const gfx::ImageSkia& foreground_image =
        *rb.GetImageNamed(IDR_ASH_SHELF_ICON_APPLIST).ToImageSkia();
    PaintAppListButton(canvas, foreground_image);
  }

  views::Painter::PaintFocusPainter(this, canvas, focus_painter());
}

void AppListButton::PaintMd(gfx::Canvas* canvas) {
  gfx::PointF circle_center(GetCenterPoint());

  // Paint the circular background.
  SkPaint bg_paint;
  bg_paint.setColor(SkColorSetA(kShelfBaseColor, background_alpha_));
  bg_paint.setFlags(SkPaint::kAntiAlias_Flag);
  bg_paint.setStyle(SkPaint::kFill_Style);
  canvas->DrawCircle(circle_center, kAppListButtonRadius, bg_paint);

  // Paint a white ring as the foreground. The ceil/dsf math assures that the
  // ring draws sharply and is centered at all scale factors.
  const float kRingOuterRadiusDp = 7.f;
  const float kRingThicknessDp = 1.5f;
  const float dsf = canvas->UndoDeviceScaleFactor();
  circle_center.Scale(dsf);

  SkPaint fg_paint;
  fg_paint.setFlags(SkPaint::kAntiAlias_Flag);
  fg_paint.setStyle(SkPaint::kStroke_Style);
  fg_paint.setColor(kShelfIconColor);
  const float thickness = std::ceil(kRingThicknessDp * dsf);
  const float radius = std::ceil(kRingOuterRadiusDp * dsf) - thickness / 2;
  fg_paint.setStrokeWidth(thickness);
  // Make sure the center of the circle lands on pixel centers.
  canvas->DrawCircle(circle_center, radius, fg_paint);
}

void AppListButton::PaintAppListButton(gfx::Canvas* canvas,
                                       const gfx::ImageSkia& foreground_image) {
  int background_image_id = 0;

  if (WmShell::Get()->GetAppListTargetVisibility() ||
      draw_background_as_active_) {
    background_image_id = IDR_AURA_LAUNCHER_BACKGROUND_PRESSED;
  } else if (wm_shelf_->IsDimmed()) {
    background_image_id = IDR_AURA_LAUNCHER_BACKGROUND_ON_BLACK;
  } else {
    background_image_id = IDR_AURA_LAUNCHER_BACKGROUND_NORMAL;
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::ImageSkia background_image =
      *rb.GetImageNamed(background_image_id).ToImageSkia();
  gfx::Rect background_bounds(background_image.size());
  ShelfAlignment alignment = wm_shelf_->GetAlignment();
  gfx::Rect contents_bounds = GetContentsBounds();

  if (alignment == SHELF_ALIGNMENT_LEFT) {
    background_bounds.set_x(contents_bounds.width() - kShelfItemInset -
                            background_image.width());
    background_bounds.set_y(
        contents_bounds.y() +
        (contents_bounds.height() - background_image.height()) / 2);
  } else if (alignment == SHELF_ALIGNMENT_RIGHT) {
    background_bounds.set_x(kShelfItemInset);
    background_bounds.set_y(
        contents_bounds.y() +
        (contents_bounds.height() - background_image.height()) / 2);
  } else {
    background_bounds.set_y(kShelfItemInset);
    background_bounds.set_x(
        contents_bounds.x() +
        (contents_bounds.width() - background_image.width()) / 2);
  }

  canvas->DrawImageInt(background_image, background_bounds.x(),
                       background_bounds.y());
  gfx::Rect foreground_bounds(foreground_image.size());
  foreground_bounds.set_x(
      background_bounds.x() +
      std::max(0, (background_bounds.width() - foreground_bounds.width()) / 2));
  foreground_bounds.set_y(
      background_bounds.y() +
      std::max(0,
               (background_bounds.height() - foreground_bounds.height()) / 2));
  canvas->DrawImageInt(foreground_image, foreground_bounds.x(),
                       foreground_bounds.y());
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

void AppListButton::SetDrawBackgroundAsActive(bool draw_background_as_active) {
  if (draw_background_as_active_ == draw_background_as_active)
    return;
  draw_background_as_active_ = draw_background_as_active;
  SchedulePaint();
}

gfx::Point AppListButton::GetCenterPoint() const {
  // During shelf hide/show animations, width and height may not be equal. Take
  // the greater of the two as the one that represents the normal size of the
  // button.
  int center = std::max(width(), height()) / 2.f;
  gfx::Point centroid(center, center);
  // For the left shelf alignment, we need to right-justify. For other shelf
  // alignments, left/top justification (i.e. no adjustments are necessary).
  if (SHELF_ALIGNMENT_LEFT == wm_shelf_->GetAlignment())
    centroid.set_x(width() - center);
  return centroid;
}

}  // namespace ash
