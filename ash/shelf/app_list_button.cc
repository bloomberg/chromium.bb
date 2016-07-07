// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_button.h"

#include "ash/common/ash_constants.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_item_types.h"
#include "ash/common/shelf/shelf_types.h"
#include "ash/common/shelf/wm_shelf_util.h"
#include "ash/shelf/ink_drop_button_listener.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/animation/square_ink_drop_ripple.h"
#include "ui/views/painter.h"

namespace ash {

AppListButton::AppListButton(InkDropButtonListener* listener,
                             ShelfView* shelf_view)
    : views::ImageButton(nullptr),
      draw_background_as_active_(false),
      listener_(listener),
      shelf_view_(shelf_view) {
  if (ash::MaterialDesignController::IsShelfMaterial()) {
    SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);
    set_ink_drop_base_color(kShelfInkDropBaseColor);
    set_ink_drop_visible_opacity(kShelfInkDropVisibleOpacity);
  }
  SetAccessibleName(
      app_list::switches::IsExperimentalAppListEnabled()
          ? l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE)
          : l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_TITLE));
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
  const bool touch_feedback =
      !is_material && switches::IsTouchFeedbackEnabled();
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (touch_feedback)
        SetDrawBackgroundAsActive(false);
      else if (is_material)
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
      if (touch_feedback)
        SetDrawBackgroundAsActive(true);
      else if (is_material && !Shell::GetInstance()->IsApplistVisible())
        AnimateInkDrop(views::InkDropState::ACTION_PENDING, event);
      ImageButton::OnGestureEvent(event);
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_TAP:
      if (touch_feedback)
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
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  const gfx::ImageSkia& foreground_image =
      MaterialDesignController::IsShelfMaterial()
          ? CreateVectorIcon(gfx::VectorIconId::SHELF_APPLIST, kShelfIconColor)
          : *rb.GetImageNamed(IDR_ASH_SHELF_ICON_APPLIST).ToImageSkia();

  if (ash::MaterialDesignController::IsShelfMaterial()) {
    PaintBackgroundMD(canvas);
    PaintForegroundMD(canvas, foreground_image);
  } else {
    PaintAppListButton(canvas, foreground_image);
  }

  views::Painter::PaintFocusPainter(this, canvas, focus_painter());
}

void AppListButton::PaintBackgroundMD(gfx::Canvas* canvas) {
  SkPaint background_paint;
  background_paint.setColor(SK_ColorTRANSPARENT);
  background_paint.setFlags(SkPaint::kAntiAlias_Flag);
  background_paint.setStyle(SkPaint::kFill_Style);

  const ShelfWidget* shelf_widget = shelf_view_->shelf()->shelf_widget();
  if (shelf_widget &&
      shelf_widget->GetBackgroundType() ==
          ShelfBackgroundType::SHELF_BACKGROUND_DEFAULT) {
    background_paint.setColor(
        SkColorSetA(kShelfBaseColor, GetShelfConstant(SHELF_BACKGROUND_ALPHA)));
  }

  // Paint the circular background of AppList button.
  gfx::Point circle_center = GetContentsBounds().CenterPoint();
  if (!IsHorizontalAlignment(shelf_view_->shelf()->alignment()))
    circle_center = gfx::Point(circle_center.y(), circle_center.x());

  canvas->DrawCircle(circle_center, kAppListButtonRadius, background_paint);
}

void AppListButton::PaintForegroundMD(gfx::Canvas* canvas,
                                      const gfx::ImageSkia& foreground_image) {
  gfx::Rect foreground_bounds(foreground_image.size());
  gfx::Rect contents_bounds = GetContentsBounds();

  if (IsHorizontalAlignment(shelf_view_->shelf()->alignment())) {
    foreground_bounds.set_x(
        (contents_bounds.width() - foreground_bounds.width()) / 2);
    foreground_bounds.set_y(
        (contents_bounds.height() - foreground_bounds.height()) / 2);
  } else {
    foreground_bounds.set_x(
        (contents_bounds.height() - foreground_bounds.height()) / 2);
    foreground_bounds.set_y(
        (contents_bounds.width() - foreground_bounds.width()) / 2);
  }
  canvas->DrawImageInt(foreground_image, foreground_bounds.x(),
                       foreground_bounds.y());
}

void AppListButton::PaintAppListButton(gfx::Canvas* canvas,
                                       const gfx::ImageSkia& foreground_image) {
  int background_image_id = 0;

  if (Shell::GetInstance()->GetAppListTargetVisibility() ||
      draw_background_as_active_) {
    background_image_id = IDR_AURA_NOTIFICATION_BACKGROUND_PRESSED;
  } else {
    if (shelf_view_->shelf()->shelf_widget()->GetDimsShelf()) {
      background_image_id = IDR_AURA_NOTIFICATION_BACKGROUND_ON_BLACK;
    } else {
      background_image_id = IDR_AURA_NOTIFICATION_BACKGROUND_NORMAL;
    }
  }

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::ImageSkia background_image =
      *rb.GetImageNamed(background_image_id).ToImageSkia();
  gfx::Rect background_bounds(background_image.size());
  ShelfAlignment alignment = shelf_view_->shelf()->alignment();
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

void AppListButton::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_BUTTON;
  state->name = shelf_view_->GetTitleForView(this);
}

std::unique_ptr<views::InkDropRipple> AppListButton::CreateInkDropRipple()
    const {
  // TODO(mohsen): A circular SquareInkDropRipple is created with equal small
  // and large sizes to mimic a circular flood fill. Replace with an actual
  // flood fill when circular flood fills are implemented.
  gfx::Size ripple_size(2 * kAppListButtonRadius, 2 * kAppListButtonRadius);
  auto ink_drop_ripple = new views::SquareInkDropRipple(
      ripple_size, 0, ripple_size, 0, GetContentsBounds().CenterPoint(),
      GetInkDropBaseColor(), ink_drop_visible_opacity());
  ink_drop_ripple->set_activated_shape(views::SquareInkDropRipple::CIRCLE);
  return base::WrapUnique(ink_drop_ripple);
}

void AppListButton::NotifyClick(const ui::Event& event) {
  ImageButton::NotifyClick(event);
  if (listener_)
    listener_->ButtonPressed(this, event, ink_drop());
}

bool AppListButton::ShouldEnterPushedState(const ui::Event& event) {
  if (!shelf_view_->ShouldEventActivateButton(this, event))
    return false;
  if (Shell::GetInstance()->IsApplistVisible())
    return false;
  return views::ImageButton::ShouldEnterPushedState(event);
}

bool AppListButton::ShouldShowInkDropHighlight() const {
  return false;
}

void AppListButton::SetDrawBackgroundAsActive(bool draw_background_as_active) {
  if (draw_background_as_active_ == draw_background_as_active)
    return;
  draw_background_as_active_ = draw_background_as_active;
  SchedulePaint();
}

}  // namespace ash
