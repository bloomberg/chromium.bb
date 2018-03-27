// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/crostini_app_window.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/launcher/crostini_app_window_shelf_controller.h"
#include "chrome/browser/ui/ash/launcher/crostini_app_window_shelf_item_controller.h"
#include "components/exo/shell_surface_base.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

CrostiniAppWindow::CrostiniAppWindow(const ash::ShelfID& shelf_id,
                                     views::Widget* widget)
    : shelf_id_(shelf_id), widget_(widget) {}

void CrostiniAppWindow::SetController(
    CrostiniAppWindowShelfItemController* controller) {
  DCHECK(!controller_ || !controller);
  controller_ = controller;
}

bool CrostiniAppWindow::IsActive() const {
  return widget_->IsActive();
}

bool CrostiniAppWindow::IsMaximized() const {
  NOTREACHED();
  return false;
}

bool CrostiniAppWindow::IsMinimized() const {
  NOTREACHED();
  return false;
}

bool CrostiniAppWindow::IsFullscreen() const {
  NOTREACHED();
  return false;
}

gfx::NativeWindow CrostiniAppWindow::GetNativeWindow() const {
  return widget_->GetNativeWindow();
}

gfx::Rect CrostiniAppWindow::GetRestoredBounds() const {
  NOTREACHED();
  return gfx::Rect();
}

ui::WindowShowState CrostiniAppWindow::GetRestoredState() const {
  NOTREACHED();
  return ui::SHOW_STATE_NORMAL;
}

gfx::Rect CrostiniAppWindow::GetBounds() const {
  NOTREACHED();
  return gfx::Rect();
}

void CrostiniAppWindow::Show() {
  widget_->Show();
}

void CrostiniAppWindow::ShowInactive() {
  NOTREACHED();
}

void CrostiniAppWindow::Hide() {
  NOTREACHED();
}

bool CrostiniAppWindow::IsVisible() const {
  NOTREACHED();
  return true;
}

void CrostiniAppWindow::Close() {
  widget_->Close();
}

void CrostiniAppWindow::Activate() {
  widget_->Activate();
}

void CrostiniAppWindow::Deactivate() {
  NOTREACHED();
}

void CrostiniAppWindow::Maximize() {
  NOTREACHED();
}

void CrostiniAppWindow::Minimize() {
  widget_->Minimize();
}

void CrostiniAppWindow::Restore() {
  NOTREACHED();
}

void CrostiniAppWindow::SetBounds(const gfx::Rect& bounds) {
  NOTREACHED();
}

void CrostiniAppWindow::FlashFrame(bool flash) {
  NOTREACHED();
}

bool CrostiniAppWindow::IsAlwaysOnTop() const {
  NOTREACHED();
  return false;
}

void CrostiniAppWindow::SetAlwaysOnTop(bool always_on_top) {
  NOTREACHED();
}
