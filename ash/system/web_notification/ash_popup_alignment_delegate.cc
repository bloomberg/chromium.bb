// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/ash_popup_alignment_delegate.h"

#include "ash/display/display_controller.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "base/i18n/rtl.h"
#include "ui/aura/window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_popup_collection.h"

namespace ash {

namespace {

const int kToastMarginX = 3;

// If there should be no margin for the first item, this value needs to be
// substracted to flush the message to the shelf (the width of the border +
// shadow).
const int kNoToastMarginBorderAndShadowOffset = 2;

}

AshPopupAlignmentDelegate::AshPopupAlignmentDelegate()
    : display_id_(gfx::Display::kInvalidDisplayID),
      screen_(NULL),
      root_window_(NULL),
      shelf_(NULL),
      system_tray_height_(0) {
}

AshPopupAlignmentDelegate::~AshPopupAlignmentDelegate() {
  if (screen_)
    screen_->RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
  if (shelf_)
    shelf_->RemoveObserver(this);
}

void AshPopupAlignmentDelegate::StartObserving(gfx::Screen* screen,
                                               const gfx::Display& display) {
  screen_ = screen;
  display_id_ = display.id();
  UpdateShelf();
  screen->AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
  if (system_tray_height_ > 0)
    OnAutoHideStateChanged(shelf_->auto_hide_state());
}

void AshPopupAlignmentDelegate::SetSystemTrayHeight(int height) {
  system_tray_height_ = height;

  // If the shelf is shown during auto-hide state, the distance from the edge
  // should be reduced by the height of shelf's shown height.
  if (shelf_ && shelf_->visibility_state() == SHELF_AUTO_HIDE &&
      shelf_->auto_hide_state() == SHELF_AUTO_HIDE_SHOWN) {
    system_tray_height_ -= kShelfSize - ShelfLayoutManager::kAutoHideSize;
  }

  if (system_tray_height_ > 0)
    system_tray_height_ += message_center::kMarginBetweenItems;
  else
    system_tray_height_ = 0;

  if (!shelf_)
    return;

  DoUpdateIfPossible();
}

int AshPopupAlignmentDelegate::GetToastOriginX(
    const gfx::Rect& toast_bounds) const {
  // In Ash, RTL UI language mirrors the whole ash layout, so the toast
  // widgets should be at the bottom-left instead of bottom right.
  if (base::i18n::IsRTL())
    return work_area_.x() + kToastMarginX;

  if (IsFromLeft())
    return work_area_.x() + kToastMarginX;
  return work_area_.right() - kToastMarginX - toast_bounds.width();
}

int AshPopupAlignmentDelegate::GetBaseLine() const {
  return IsTopDown()
      ? work_area_.y() + kNoToastMarginBorderAndShadowOffset +
        system_tray_height_
      : work_area_.bottom() - kNoToastMarginBorderAndShadowOffset -
        system_tray_height_;
}

int AshPopupAlignmentDelegate::GetWorkAreaBottom() const {
  return work_area_.bottom() - system_tray_height_;
}

bool AshPopupAlignmentDelegate::IsTopDown() const {
  return GetAlignment() == SHELF_ALIGNMENT_TOP;
}

bool AshPopupAlignmentDelegate::IsFromLeft() const {
  return GetAlignment() == SHELF_ALIGNMENT_LEFT;
}

void AshPopupAlignmentDelegate::RecomputeAlignment(
    const gfx::Display& display) {
  // Nothing needs to be done.
}

ShelfAlignment AshPopupAlignmentDelegate::GetAlignment() const {
  return shelf_ ? shelf_->GetAlignment() : SHELF_ALIGNMENT_BOTTOM;
}

void AshPopupAlignmentDelegate::UpdateShelf() {
  if (shelf_)
    return;

  aura::Window* root_window = ash::Shell::GetInstance()->display_controller()->
      GetRootWindowForDisplayId(display_id_);
  shelf_ = ShelfLayoutManager::ForShelf(root_window);
  if (shelf_)
    shelf_->AddObserver(this);
}

void AshPopupAlignmentDelegate::OnDisplayWorkAreaInsetsChanged() {
  UpdateShelf();

  work_area_ = Shell::GetScreen()->GetDisplayNearestWindow(
      shelf_->shelf_widget()->GetNativeView()).work_area();
}

void AshPopupAlignmentDelegate::OnAutoHideStateChanged(
    ShelfAutoHideState new_state) {
  work_area_ = Shell::GetScreen()->GetDisplayNearestWindow(
      shelf_->shelf_widget()->GetNativeView()).work_area();
  int width = 0;
  if ((shelf_->visibility_state() == SHELF_AUTO_HIDE) &&
      new_state == SHELF_AUTO_HIDE_SHOWN) {
    // Since the work_area is already reduced by kAutoHideSize, the inset width
    // should be just the difference.
    width = kShelfSize - ShelfLayoutManager::kAutoHideSize;
  }
  work_area_.Inset(shelf_->SelectValueForShelfAlignment(
      gfx::Insets(0, 0, width, 0),
      gfx::Insets(0, width, 0, 0),
      gfx::Insets(0, 0, 0, width),
      gfx::Insets(width, 0, 0, 0)));

  DoUpdateIfPossible();
}

void AshPopupAlignmentDelegate::OnDisplayAdded(
    const gfx::Display& new_display) {
}

void AshPopupAlignmentDelegate::OnDisplayRemoved(
    const gfx::Display& old_display) {
}

void AshPopupAlignmentDelegate::OnDisplayMetricsChanged(
    const gfx::Display& display,
    uint32_t metrics) {
  if (display.id() == display_id_ && shelf_)
    OnAutoHideStateChanged(shelf_->auto_hide_state());
}

}  // namespace ash
