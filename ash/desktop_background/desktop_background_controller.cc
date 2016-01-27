// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/desktop_background_controller.h"

#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller_observer.h"
#include "ash/desktop_background/desktop_background_view.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/root_window_layout_manager.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/wallpaper/wallpaper_resizer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

using wallpaper::WallpaperResizer;
using wallpaper::WallpaperLayout;
using wallpaper::WALLPAPER_LAYOUT_CENTER;
using wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
using wallpaper::WALLPAPER_LAYOUT_STRETCH;
using wallpaper::WALLPAPER_LAYOUT_TILE;

namespace ash {
namespace {

// How long to wait reloading the wallpaper after the max display has
// changed?
const int kWallpaperReloadDelayMs = 100;

}  // namespace

DesktopBackgroundController::DesktopBackgroundController(
    base::SequencedWorkerPool* blocking_pool)
    : locked_(false),
      desktop_background_mode_(BACKGROUND_NONE),
      wallpaper_reload_delay_(kWallpaperReloadDelayMs),
      blocking_pool_(blocking_pool) {
  Shell::GetInstance()->window_tree_host_manager()->AddObserver(this);
  Shell::GetInstance()->AddShellObserver(this);
}

DesktopBackgroundController::~DesktopBackgroundController() {
  Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);
  Shell::GetInstance()->RemoveShellObserver(this);
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

WallpaperLayout DesktopBackgroundController::GetWallpaperLayout() const {
  if (current_wallpaper_)
    return current_wallpaper_->layout();
  return WALLPAPER_LAYOUT_CENTER_CROPPED;
}

bool DesktopBackgroundController::SetWallpaperImage(const gfx::ImageSkia& image,
                                                    WallpaperLayout layout) {
  VLOG(1) << "SetWallpaper: image_id=" << WallpaperResizer::GetImageId(image)
          << " layout=" << layout;

  if (WallpaperIsAlreadyLoaded(image, true /* compare_layouts */, layout)) {
    VLOG(1) << "Wallpaper is already loaded";
    return false;
  }

  current_wallpaper_.reset(new WallpaperResizer(
      image, GetMaxDisplaySizeInNative(), layout, blocking_pool_));
  current_wallpaper_->StartResize();

  FOR_EACH_OBSERVER(DesktopBackgroundControllerObserver,
                    observers_,
                    OnWallpaperDataChanged());
  SetDesktopBackgroundImageMode();
  return true;
}

void DesktopBackgroundController::CreateEmptyWallpaper() {
  current_wallpaper_.reset(NULL);
  SetDesktopBackgroundImageMode();
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
    if (desktop_background_mode_ == BACKGROUND_IMAGE &&
        current_wallpaper_.get()) {
      timer_.Stop();
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(wallpaper_reload_delay_),
                   base::Bind(&DesktopBackgroundController::UpdateWallpaper,
                              base::Unretained(this), false /* clear cache */));
    }
  }
}

void DesktopBackgroundController::OnRootWindowAdded(aura::Window* root_window) {
  // The background hasn't been set yet.
  if (desktop_background_mode_ == BACKGROUND_NONE)
    return;

  // Handle resolution change for "built-in" images.
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ != max_display_size) {
    current_max_display_size_ = max_display_size;
    if (desktop_background_mode_ == BACKGROUND_IMAGE &&
        current_wallpaper_.get())
      UpdateWallpaper(true /* clear cache */);
  }

  InstallDesktopController(root_window);
}

// static
gfx::Size DesktopBackgroundController::GetMaxDisplaySizeInNative() {
  int width = 0;
  int height = 0;
  std::vector<gfx::Display> displays =
      gfx::Screen::GetScreen()->GetAllDisplays();
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();

  for (std::vector<gfx::Display>::iterator iter = displays.begin();
       iter != displays.end(); ++iter) {
    // Don't use size_in_pixel because we want to use the native pixel size.
    gfx::Size size_in_pixel =
        display_manager->GetDisplayInfo(iter->id()).bounds_in_native().size();
    if (iter->rotation() == gfx::Display::ROTATE_90 ||
        iter->rotation() == gfx::Display::ROTATE_270) {
      size_in_pixel = gfx::Size(size_in_pixel.height(), size_in_pixel.width());
    }
    width = std::max(size_in_pixel.width(), width);
    height = std::max(size_in_pixel.height(), height);
  }
  return gfx::Size(width, height);
}

bool DesktopBackgroundController::WallpaperIsAlreadyLoaded(
    const gfx::ImageSkia& image,
    bool compare_layouts,
    WallpaperLayout layout) const {
  if (!current_wallpaper_.get())
    return false;

  // Compare layouts only if necessary.
  if (compare_layouts && layout != current_wallpaper_->layout())
    return false;

  return WallpaperResizer::GetImageId(image) ==
         current_wallpaper_->original_image_id();
}

void DesktopBackgroundController::SetDesktopBackgroundImageMode() {
  desktop_background_mode_ = BACKGROUND_IMAGE;
  InstallDesktopControllerForAllWindows();
}

void DesktopBackgroundController::InstallDesktopController(
    aura::Window* root_window) {
  DesktopBackgroundWidgetController* component = NULL;
  int container_id = GetBackgroundContainerId(locked_);

  switch (desktop_background_mode_) {
    case BACKGROUND_IMAGE: {
      views::Widget* widget =
          CreateDesktopBackground(root_window, container_id);
      component = new DesktopBackgroundWidgetController(widget);
      break;
    }
    case BACKGROUND_NONE:
      NOTREACHED();
      return;
  }
  GetRootWindowController(root_window)->SetAnimatingWallpaperController(
      new AnimatingDesktopController(component));

  component->StartAnimating(GetRootWindowController(root_window));
}

void DesktopBackgroundController::InstallDesktopControllerForAllWindows() {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window::Windows::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    InstallDesktopController(*iter);
  }
  current_max_display_size_ = GetMaxDisplaySizeInNative();
}

bool DesktopBackgroundController::ReparentBackgroundWidgets(int src_container,
                                                            int dst_container) {
  bool moved = false;
  Shell::RootWindowControllerList controllers =
      Shell::GetAllRootWindowControllers();
  for (Shell::RootWindowControllerList::iterator iter = controllers.begin();
    iter != controllers.end(); ++iter) {
    RootWindowController* root_window_controller = *iter;
    // In the steady state (no animation playing) the background widget
    // controller exists in the RootWindowController.
    DesktopBackgroundWidgetController* desktop_controller =
        root_window_controller->wallpaper_controller();
    if (desktop_controller) {
      moved |=
          desktop_controller->Reparent(root_window_controller->GetRootWindow(),
                                       src_container,
                                       dst_container);
    }
    // During desktop show animations the controller lives in
    // AnimatingDesktopController owned by RootWindowController.
    // NOTE: If a wallpaper load happens during a desktop show animation there
    // can temporarily be two desktop background widgets.  We must reparent
    // both of them - one above and one here.
    DesktopBackgroundWidgetController* animating_controller =
        root_window_controller->animating_wallpaper_controller() ?
        root_window_controller->animating_wallpaper_controller()->
            GetController(false) :
        NULL;
    if (animating_controller) {
      moved |= animating_controller->Reparent(
          root_window_controller->GetRootWindow(),
          src_container,
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
  current_wallpaper_.reset(NULL);
  ash::Shell::GetInstance()->user_wallpaper_delegate()->UpdateWallpaper(
      clear_cache);
}

}  // namespace ash
