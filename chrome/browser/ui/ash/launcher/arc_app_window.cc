// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_window.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_item_controller.h"
#include "components/exo/shell_surface_base.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

ArcAppWindow::ArcAppWindow(int task_id,
                           const arc::ArcAppShelfId& app_shelf_id,
                           views::Widget* widget,
                           ArcAppWindowLauncherController* owner)
    : task_id_(task_id),
      app_shelf_id_(app_shelf_id),
      widget_(widget),
      owner_(owner) {}

ArcAppWindow::~ArcAppWindow() {
  ImageDecoder::Cancel(this);
}

void ArcAppWindow::SetController(
    ArcAppWindowLauncherItemController* controller) {
  DCHECK(!controller_ || !controller);
  controller_ = controller;
}

void ArcAppWindow::SetFullscreenMode(FullScreenMode mode) {
  DCHECK(mode != FullScreenMode::NOT_DEFINED);
  fullscreen_mode_ = mode;
}

void ArcAppWindow::SetDescription(
    const std::string& title,
    const std::vector<uint8_t>& unsafe_icon_data_png) {
  if (!title.empty())
    GetNativeWindow()->SetTitle(base::UTF8ToUTF16(title));
  ImageDecoder::Cancel(this);
  if (unsafe_icon_data_png.empty()) {
    SetIcon(gfx::ImageSkia());
    return;
  }

  if (ArcAppIcon::IsSafeDecodingDisabledForTesting()) {
    SkBitmap bitmap;
    if (gfx::PNGCodec::Decode(&unsafe_icon_data_png[0],
                              unsafe_icon_data_png.size(), &bitmap)) {
      OnImageDecoded(bitmap);
    } else {
      OnDecodeImageFailed();
    }
  } else {
    ImageDecoder::Start(this, unsafe_icon_data_png);
  }
}

bool ArcAppWindow::IsActive() const {
  return widget_->IsActive() && owner_->active_task_id() == task_id_;
}

bool ArcAppWindow::IsMaximized() const {
  NOTREACHED();
  return false;
}

bool ArcAppWindow::IsMinimized() const {
  NOTREACHED();
  return false;
}

bool ArcAppWindow::IsFullscreen() const {
  NOTREACHED();
  return false;
}

gfx::NativeWindow ArcAppWindow::GetNativeWindow() const {
  return widget_->GetNativeWindow();
}

gfx::Rect ArcAppWindow::GetRestoredBounds() const {
  NOTREACHED();
  return gfx::Rect();
}

ui::WindowShowState ArcAppWindow::GetRestoredState() const {
  NOTREACHED();
  return ui::SHOW_STATE_NORMAL;
}

gfx::Rect ArcAppWindow::GetBounds() const {
  NOTREACHED();
  return gfx::Rect();
}

void ArcAppWindow::Show() {
  widget_->Show();
}

void ArcAppWindow::ShowInactive() {
  NOTREACHED();
}

void ArcAppWindow::Hide() {
  NOTREACHED();
}

bool ArcAppWindow::IsVisible() const {
  NOTREACHED();
  return true;
}

void ArcAppWindow::Close() {
  arc::CloseTask(task_id_);
}

void ArcAppWindow::Activate() {
  widget_->Activate();
}

void ArcAppWindow::Deactivate() {
  NOTREACHED();
}

void ArcAppWindow::Maximize() {
  NOTREACHED();
}

void ArcAppWindow::Minimize() {
  widget_->Minimize();
}

void ArcAppWindow::Restore() {
  NOTREACHED();
}

void ArcAppWindow::SetBounds(const gfx::Rect& bounds) {
  NOTREACHED();
}

void ArcAppWindow::FlashFrame(bool flash) {
  NOTREACHED();
}

bool ArcAppWindow::IsAlwaysOnTop() const {
  NOTREACHED();
  return false;
}

void ArcAppWindow::SetAlwaysOnTop(bool always_on_top) {
  NOTREACHED();
}

void ArcAppWindow::SetIcon(const gfx::ImageSkia& icon) {
  if (!exo::ShellSurfaceBase::GetMainSurface(GetNativeWindow())) {
    // Support unit tests where we don't have exo system initialized.
    views::NativeWidgetAura::AssignIconToAuraWindow(
        GetNativeWindow(), gfx::ImageSkia() /* window_icon */,
        icon /* app_icon */);
    return;
  }
  exo::ShellSurfaceBase* shell_surface = static_cast<exo::ShellSurfaceBase*>(
      widget_->widget_delegate()->GetContentsView());
  if (!shell_surface)
    return;
  shell_surface->SetIcon(icon);
}

void ArcAppWindow::OnImageDecoded(const SkBitmap& decoded_image) {
  SetIcon(gfx::ImageSkiaOperations::CreateResizedImage(
      gfx::ImageSkia(gfx::ImageSkiaRep(decoded_image, 1.0f)),
      skia::ImageOperations::RESIZE_BEST,
      gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                extension_misc::EXTENSION_ICON_SMALL)));
}
