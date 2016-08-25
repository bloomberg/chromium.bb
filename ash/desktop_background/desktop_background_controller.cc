// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/display/display_info.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/wallpaper/wallpaper_resizer.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// How long to wait reloading the wallpaper after the display size has changed.
const int kWallpaperReloadDelayMs = 100;

}  // namespace

DesktopBackgroundController::DesktopBackgroundController(
    base::SequencedWorkerPool* blocking_pool)
    : locked_(false),
      desktop_background_mode_(BACKGROUND_NONE),
      wallpaper_reload_delay_(kWallpaperReloadDelayMs),
      blocking_pool_(blocking_pool) {
  WmShell::Get()->AddDisplayObserver(this);
  WmShell::Get()->AddShellObserver(this);
}

DesktopBackgroundController::~DesktopBackgroundController() {
  WmShell::Get()->RemoveDisplayObserver(this);
  WmShell::Get()->RemoveShellObserver(this);
}

gfx::ImageSkia DesktopBackgroundController::GetWallpaper() const {
  if (current_wallpaper_)
    return current_wallpaper_->image();
  return gfx::ImageSkia();
}

void DesktopBackgroundController::AddObserver(
    DesktopBackgroundControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void DesktopBackgroundController::RemoveObserver(
    DesktopBackgroundControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

wallpaper::WallpaperLayout DesktopBackgroundController::GetWallpaperLayout()
    const {
  if (current_wallpaper_)
    return current_wallpaper_->layout();
  return wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
}

bool DesktopBackgroundController::SetWallpaperImage(
    const gfx::ImageSkia& image,
    wallpaper::WallpaperLayout layout) {
  VLOG(1) << "SetWallpaper: image_id="
          << wallpaper::WallpaperResizer::GetImageId(image)
          << " layout=" << layout;

  if (WallpaperIsAlreadyLoaded(image, true /* compare_layouts */, layout)) {
    VLOG(1) << "Wallpaper is already loaded";
    return false;
  }

  current_wallpaper_.reset(new wallpaper::WallpaperResizer(
      image, GetMaxDisplaySizeInNative(), layout, blocking_pool_));
  current_wallpaper_->StartResize();

  FOR_EACH_OBSERVER(DesktopBackgroundControllerObserver, observers_,
                    OnWallpaperDataChanged());
  desktop_background_mode_ = BACKGROUND_IMAGE;
  InstallDesktopControllerForAllWindows();
  return true;
}

void DesktopBackgroundController::CreateEmptyWallpaper() {
  current_wallpaper_.reset();
  desktop_background_mode_ = BACKGROUND_IMAGE;
  InstallDesktopControllerForAllWindows();
}

bool DesktopBackgroundController::MoveDesktopToLockedContainer() {
  if (locked_)
    return false;
  locked_ = true;
  return ReparentBackgroundWidgets(GetBackgroundContainerId(false),
                                   GetBackgroundContainerId(true));
}

bool DesktopBackgroundController::MoveDesktopToUnlockedContainer() {
  if (!locked_)
    return false;
  locked_ = false;
  return ReparentBackgroundWidgets(GetBackgroundContainerId(true),
                                   GetBackgroundContainerId(false));
}

void DesktopBackgroundController::OnDisplayConfigurationChanged() {
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ != max_display_size) {
    current_max_display_size_ = max_display_size;
    if (desktop_background_mode_ == BACKGROUND_IMAGE && current_wallpaper_) {
      timer_.Stop();
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(wallpaper_reload_delay_),
                   base::Bind(&DesktopBackgroundController::UpdateWallpaper,
                              base::Unretained(this), false /* clear cache */));
    }
  }
}

void DesktopBackgroundController::OnRootWindowAdded(WmWindow* root_window) {
  // The background hasn't been set yet.
  if (desktop_background_mode_ == BACKGROUND_NONE)
    return;

  // Handle resolution change for "built-in" images.
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ != max_display_size) {
    current_max_display_size_ = max_display_size;
    if (desktop_background_mode_ == BACKGROUND_IMAGE && current_wallpaper_)
      UpdateWallpaper(true /* clear cache */);
  }

  InstallDesktopController(root_window);
}

// static
gfx::Size DesktopBackgroundController::GetMaxDisplaySizeInNative() {
  // Return an empty size for test environments where the screen is null.
  if (!display::Screen::GetScreen())
    return gfx::Size();

  // Note that |shell| is null when this is called from Chrome running in Mash.
  WmShell* shell = WmShell::Get();

  gfx::Size max;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    // Use the native size, not DisplayInfo::size_in_pixel or Display::size.
    // TODO(msw): Avoid using Display::size here; see http://crbug.com/613657.
    gfx::Size size = display.size();
    if (shell)
      size = shell->GetDisplayInfo(display.id()).bounds_in_native().size();
    if (display.rotation() == display::Display::ROTATE_90 ||
        display.rotation() == display::Display::ROTATE_270) {
      size = gfx::Size(size.height(), size.width());
    }
    max.SetToMax(size);
  }

  return max;
}

bool DesktopBackgroundController::WallpaperIsAlreadyLoaded(
    const gfx::ImageSkia& image,
    bool compare_layouts,
    wallpaper::WallpaperLayout layout) const {
  if (!current_wallpaper_)
    return false;

  // Compare layouts only if necessary.
  if (compare_layouts && layout != current_wallpaper_->layout())
    return false;

  return wallpaper::WallpaperResizer::GetImageId(image) ==
         current_wallpaper_->original_image_id();
}

void DesktopBackgroundController::InstallDesktopController(
    WmWindow* root_window) {
  DesktopBackgroundWidgetController* component = nullptr;
  int container_id = GetBackgroundContainerId(locked_);

  switch (desktop_background_mode_) {
    case BACKGROUND_IMAGE: {
      component = new DesktopBackgroundWidgetController(
          CreateDesktopBackground(root_window, container_id));
      break;
    }
    case BACKGROUND_NONE:
      NOTREACHED();
      return;
  }

  aura::Window* aura_root_window = WmWindowAura::GetAuraWindow(root_window);
  RootWindowController* controller = GetRootWindowController(aura_root_window);
  controller->SetAnimatingWallpaperController(
      new AnimatingDesktopController(component));
  component->StartAnimating(controller);
}

void DesktopBackgroundController::InstallDesktopControllerForAllWindows() {
  for (WmWindow* root : WmShell::Get()->GetAllRootWindows())
    InstallDesktopController(root);
  current_max_display_size_ = GetMaxDisplaySizeInNative();
}

bool DesktopBackgroundController::ReparentBackgroundWidgets(int src_container,
                                                            int dst_container) {
  bool moved = false;
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    // In the steady state (no animation playing) the background widget
    // controller exists in the RootWindowController.
    DesktopBackgroundWidgetController* desktop_controller =
        root_window_controller->wallpaper_controller();
    if (desktop_controller) {
      moved |=
          desktop_controller->Reparent(root_window_controller->GetRootWindow(),
                                       src_container, dst_container);
    }
    // During desktop show animations the controller lives in
    // AnimatingDesktopController owned by RootWindowController.
    // NOTE: If a wallpaper load happens during a desktop show animation there
    // can temporarily be two desktop background widgets.  We must reparent
    // both of them - one above and one here.
    DesktopBackgroundWidgetController* animating_controller =
        root_window_controller->animating_wallpaper_controller()
            ? root_window_controller->animating_wallpaper_controller()
                  ->GetController(false)
            : nullptr;
    if (animating_controller) {
      moved |= animating_controller->Reparent(
          root_window_controller->GetRootWindow(), src_container,
          dst_container);
    }
  }
  return moved;
}

int DesktopBackgroundController::GetBackgroundContainerId(bool locked) {
  return locked ? kShellWindowId_LockScreenBackgroundContainer
                : kShellWindowId_DesktopBackgroundContainer;
}

void DesktopBackgroundController::UpdateWallpaper(bool clear_cache) {
  current_wallpaper_.reset();
  WmShell::Get()->wallpaper_delegate()->UpdateWallpaper(clear_cache);
}

}  // namespace ash
