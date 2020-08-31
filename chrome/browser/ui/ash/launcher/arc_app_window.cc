// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_window.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_service/app_service_app_icon_loader.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_item_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/arc/arc_util.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace {
constexpr base::TimeDelta kSetDefaultIconDelayMs =
    base::TimeDelta::FromMilliseconds(1000);

constexpr int kArcAppWindowIconSize = extension_misc::EXTENSION_ICON_MEDIUM;
}  // namespace

ArcAppWindow::ArcAppWindow(int task_id,
                           const arc::ArcAppShelfId& app_shelf_id,
                           views::Widget* widget,
                           ArcAppWindowDelegate* owner,
                           Profile* profile)
    : AppWindowBase(ash::ShelfID(app_shelf_id.app_id()), widget),
      task_id_(task_id),
      app_shelf_id_(app_shelf_id),
      owner_(owner),
      profile_(profile) {
  DCHECK(owner_);

  // AppService uses app_shelf_id as the app_id to construct ShelfID.
  if (base::FeatureList::IsEnabled(features::kAppServiceInstanceRegistry))
    set_shelf_id(ash::ShelfID(app_shelf_id.ToString()));

  SetDefaultAppIcon();
}

ArcAppWindow::~ArcAppWindow() {
  ImageDecoder::Cancel(this);
}

void ArcAppWindow::SetFullscreenMode(FullScreenMode mode) {
  DCHECK(mode != FullScreenMode::kNotDefined);
  fullscreen_mode_ = mode;
}

void ArcAppWindow::SetDescription(
    const std::string& title,
    const std::vector<uint8_t>& unsafe_icon_data_png) {
  if (!title.empty())
    GetNativeWindow()->SetTitle(base::UTF8ToUTF16(title));
  ImageDecoder::Cancel(this);
  if (unsafe_icon_data_png.empty()) {
    // Reset custom icon. Switch back to default.
    SetDefaultAppIcon();
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
  return widget()->IsActive() && owner_->GetActiveTaskId() == task_id_;
}

void ArcAppWindow::Close() {
  arc::CloseTask(task_id_);
}

void ArcAppWindow::OnAppImageUpdated(const std::string& app_id,
                                     const gfx::ImageSkia& image) {
  if (image_fetching_) {
    // This is default app icon. Don't assign it right now to avoid flickering.
    // Wait for another image is loaded and only in case next image is not
    // coming set this as a fallback.
    apply_default_image_timer_.Start(
        FROM_HERE, kSetDefaultIconDelayMs,
        base::BindOnce(&ArcAppWindow::SetIcon, base::Unretained(this), image));
  } else {
    SetIcon(image);
  }
}

void ArcAppWindow::SetDefaultAppIcon() {
  if (!app_icon_loader_) {
    app_icon_loader_ = std::make_unique<AppServiceAppIconLoader>(
        profile_, kArcAppWindowIconSize, this);
  }
  DCHECK(!image_fetching_);
  base::AutoReset<bool> auto_image_fetching(&image_fetching_, true);
  app_icon_loader_->FetchImage(app_shelf_id_.ToString());
}

void ArcAppWindow::SetIcon(const gfx::ImageSkia& icon) {
  // Reset any pending request to set default app icon.
  apply_default_image_timer_.Stop();

  if (!exo::GetShellMainSurface(GetNativeWindow())) {
    // Support unit tests where we don't have exo system initialized.
    views::NativeWidgetAura::AssignIconToAuraWindow(
        GetNativeWindow(), gfx::ImageSkia() /* window_icon */,
        icon /* app_icon */);
    return;
  }
  exo::ShellSurfaceBase* shell_surface = static_cast<exo::ShellSurfaceBase*>(
      widget()->widget_delegate()->GetContentsView());
  if (!shell_surface)
    return;
  shell_surface->SetIcon(icon);
}

void ArcAppWindow::OnImageDecoded(const SkBitmap& decoded_bitmap) {
  // Use the custom icon and stop observing updates.
  app_icon_loader_.reset();
  const gfx::ImageSkia decoded_image(gfx::ImageSkiaRep(decoded_bitmap, 1.0f));
  if (kArcAppWindowIconSize > decoded_image.width() ||
      kArcAppWindowIconSize > decoded_image.height()) {
    LOG(WARNING) << "An icon of size " << decoded_image.width() << "x"
                 << decoded_image.height()
                 << " is being scaled up and will look blurry.";
  }
  SetIcon(gfx::ImageSkiaOperations::CreateResizedImage(
      decoded_image, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(kArcAppWindowIconSize, kArcAppWindowIconSize)));
}
