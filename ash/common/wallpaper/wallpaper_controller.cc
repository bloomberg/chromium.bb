// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wallpaper/wallpaper_controller.h"

#include <string>
#include <utility>

#include "ash/common/ash_switches.h"
#include "ash/common/wallpaper/wallpaper_controller_observer.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wallpaper/wallpaper_view.h"
#include "ash/common/wallpaper/wallpaper_widget_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/task_runner.h"
#include "components/wallpaper/wallpaper_color_calculator.h"
#include "components/wallpaper/wallpaper_resizer.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_analysis.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// How long to wait reloading the wallpaper after the display size has changed.
const int kWallpaperReloadDelayMs = 100;

}  // namespace

// static
bool WallpaperController::GetProminentColorProfile(
    color_utils::LumaRange* luma,
    color_utils::SaturationRange* saturation) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshShelfColor)) {
    return false;
  }

  *luma = color_utils::LumaRange::NORMAL;
  *saturation = color_utils::SaturationRange::VIBRANT;

  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAshShelfColor);
  if (switch_value.find("light") != std::string::npos)
    *luma = color_utils::LumaRange::LIGHT;
  else if (switch_value.find("normal") != std::string::npos)
    *luma = color_utils::LumaRange::NORMAL;
  else if (switch_value.find("dark") != std::string::npos)
    *luma = color_utils::LumaRange::DARK;

  if (switch_value.find("vibrant") != std::string::npos)
    *saturation = color_utils::SaturationRange::VIBRANT;
  else if (switch_value.find("muted") != std::string::npos)
    *saturation = color_utils::SaturationRange::MUTED;

  return true;
}

WallpaperController::WallpaperController(
    const scoped_refptr<base::TaskRunner>& task_runner)
    : locked_(false),
      wallpaper_mode_(WALLPAPER_NONE),
      prominent_color_(SK_ColorTRANSPARENT),
      wallpaper_reload_delay_(kWallpaperReloadDelayMs),
      task_runner_(task_runner) {
  WmShell::Get()->AddDisplayObserver(this);
  WmShell::Get()->AddShellObserver(this);
}

WallpaperController::~WallpaperController() {
  if (current_wallpaper_)
    current_wallpaper_->RemoveObserver(this);
  if (color_calculator_)
    color_calculator_->RemoveObserver(this);
  WmShell::Get()->RemoveDisplayObserver(this);
  WmShell::Get()->RemoveShellObserver(this);
}

void WallpaperController::BindRequest(
    mojom::WallpaperControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

gfx::ImageSkia WallpaperController::GetWallpaper() const {
  if (current_wallpaper_)
    return current_wallpaper_->image();
  return gfx::ImageSkia();
}

void WallpaperController::AddObserver(WallpaperControllerObserver* observer) {
  observers_.AddObserver(observer);
}

void WallpaperController::RemoveObserver(
    WallpaperControllerObserver* observer) {
  observers_.RemoveObserver(observer);
}

wallpaper::WallpaperLayout WallpaperController::GetWallpaperLayout() const {
  if (current_wallpaper_)
    return current_wallpaper_->layout();
  return wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
}

void WallpaperController::SetWallpaperImage(const gfx::ImageSkia& image,
                                            wallpaper::WallpaperLayout layout) {
  VLOG(1) << "SetWallpaper: image_id="
          << wallpaper::WallpaperResizer::GetImageId(image)
          << " layout=" << layout;

  if (WallpaperIsAlreadyLoaded(image, true /* compare_layouts */, layout)) {
    VLOG(1) << "Wallpaper is already loaded";
    return;
  }

  current_wallpaper_.reset(new wallpaper::WallpaperResizer(
      image, GetMaxDisplaySizeInNative(), layout, task_runner_));
  current_wallpaper_->AddObserver(this);
  current_wallpaper_->StartResize();

  for (auto& observer : observers_)
    observer.OnWallpaperDataChanged();
  wallpaper_mode_ = WALLPAPER_IMAGE;
  InstallDesktopControllerForAllWindows();
}

void WallpaperController::CreateEmptyWallpaper() {
  SetProminentColor(SK_ColorTRANSPARENT);
  current_wallpaper_.reset();
  wallpaper_mode_ = WALLPAPER_IMAGE;
  InstallDesktopControllerForAllWindows();
}

bool WallpaperController::MoveToLockedContainer() {
  if (locked_)
    return false;
  locked_ = true;
  return ReparentWallpaper(GetWallpaperContainerId(true));
}

bool WallpaperController::MoveToUnlockedContainer() {
  if (!locked_)
    return false;
  locked_ = false;
  return ReparentWallpaper(GetWallpaperContainerId(false));
}

void WallpaperController::OnDisplayConfigurationChanged() {
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ != max_display_size) {
    current_max_display_size_ = max_display_size;
    if (wallpaper_mode_ == WALLPAPER_IMAGE && current_wallpaper_) {
      timer_.Stop();
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMilliseconds(wallpaper_reload_delay_),
                   base::Bind(&WallpaperController::UpdateWallpaper,
                              base::Unretained(this), false /* clear cache */));
    }
  }
}

void WallpaperController::OnRootWindowAdded(WmWindow* root_window) {
  // The wallpaper hasn't been set yet.
  if (wallpaper_mode_ == WALLPAPER_NONE)
    return;

  // Handle resolution change for "built-in" images.
  gfx::Size max_display_size = GetMaxDisplaySizeInNative();
  if (current_max_display_size_ != max_display_size) {
    current_max_display_size_ = max_display_size;
    if (wallpaper_mode_ == WALLPAPER_IMAGE && current_wallpaper_)
      UpdateWallpaper(true /* clear cache */);
  }

  InstallDesktopController(root_window);
}

// static
gfx::Size WallpaperController::GetMaxDisplaySizeInNative() {
  // Return an empty size for test environments where the screen is null.
  if (!display::Screen::GetScreen())
    return gfx::Size();

  // Note that |shell| is null when this is called from Chrome running in Mash.
  WmShell* shell = WmShell::Get();

  gfx::Size max;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    // Use the native size, not ManagedDisplayInfo::size_in_pixel or
    // Display::size.
    // TODO(msw): Avoid using Display::size here; see http://crbug.com/613657.
    gfx::Size size = display.size();
    if (shell) {
      display::ManagedDisplayInfo info = shell->GetDisplayInfo(display.id());
      // TODO(mash): Mash returns a fake ManagedDisplayInfo. crbug.com/622480
      if (info.id() == display.id())
        size = info.bounds_in_native().size();
    }
    if (display.rotation() == display::Display::ROTATE_90 ||
        display.rotation() == display::Display::ROTATE_270) {
      size = gfx::Size(size.height(), size.width());
    }
    max.SetToMax(size);
  }

  return max;
}

bool WallpaperController::WallpaperIsAlreadyLoaded(
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

void WallpaperController::OpenSetWallpaperPage() {
  if (wallpaper_picker_ &&
      WmShell::Get()->wallpaper_delegate()->CanOpenSetWallpaperPage()) {
    wallpaper_picker_->Open();
  }
}

void WallpaperController::SetWallpaperPicker(mojom::WallpaperPickerPtr picker) {
  wallpaper_picker_ = std::move(picker);
}

void WallpaperController::SetWallpaper(const SkBitmap& wallpaper,
                                       wallpaper::WallpaperLayout layout) {
  if (wallpaper.isNull())
    return;

  SetWallpaperImage(gfx::ImageSkia::CreateFrom1xBitmap(wallpaper), layout);
}

void WallpaperController::OnWallpaperResized() {
  CalculateWallpaperColors();
}

void WallpaperController::OnColorCalculationComplete() {
  const SkColor color = color_calculator_->prominent_color();
  color_calculator_.reset();
  SetProminentColor(color);
}

void WallpaperController::InstallDesktopController(WmWindow* root_window) {
  WallpaperWidgetController* component = nullptr;
  int container_id = GetWallpaperContainerId(locked_);

  switch (wallpaper_mode_) {
    case WALLPAPER_IMAGE: {
      component = new WallpaperWidgetController(
          CreateWallpaper(root_window, container_id));
      break;
    }
    case WALLPAPER_NONE:
      NOTREACHED();
      return;
  }

  RootWindowController* controller = root_window->GetRootWindowController();
  controller->SetAnimatingWallpaperWidgetController(
      new AnimatingWallpaperWidgetController(component));
  component->StartAnimating(controller);
}

void WallpaperController::InstallDesktopControllerForAllWindows() {
  for (WmWindow* root : WmShell::Get()->GetAllRootWindows())
    InstallDesktopController(root);
  current_max_display_size_ = GetMaxDisplaySizeInNative();
}

bool WallpaperController::ReparentWallpaper(int container) {
  bool moved = false;
  for (WmWindow* root_window : WmShell::Get()->GetAllRootWindows()) {
    RootWindowController* root_window_controller =
        root_window->GetRootWindowController();
    // In the steady state (no animation playing) the wallpaper widget
    // controller exists in the RootWindowController.
    WallpaperWidgetController* wallpaper_widget_controller =
        root_window_controller->wallpaper_widget_controller();
    if (wallpaper_widget_controller) {
      moved |= wallpaper_widget_controller->Reparent(
          root_window_controller->GetWindow(), container);
    }
    // During wallpaper show animations the controller lives in
    // AnimatingWallpaperWidgetController owned by RootWindowController.
    // NOTE: If an image load happens during a wallpaper show animation there
    // can temporarily be two wallpaper widgets. We must reparent both of them,
    // one above and one here.
    WallpaperWidgetController* animating_controller =
        root_window_controller->animating_wallpaper_widget_controller()
            ? root_window_controller->animating_wallpaper_widget_controller()
                  ->GetController(false)
            : nullptr;
    if (animating_controller) {
      moved |= animating_controller->Reparent(
          root_window_controller->GetWindow(), container);
    }
  }
  return moved;
}

int WallpaperController::GetWallpaperContainerId(bool locked) {
  return locked ? kShellWindowId_LockScreenWallpaperContainer
                : kShellWindowId_WallpaperContainer;
}

void WallpaperController::UpdateWallpaper(bool clear_cache) {
  current_wallpaper_.reset();
  WmShell::Get()->wallpaper_delegate()->UpdateWallpaper(clear_cache);
}

void WallpaperController::SetProminentColor(SkColor color) {
  if (prominent_color_ == color)
    return;

  prominent_color_ = color;
  for (auto& observer : observers_)
    observer.OnWallpaperColorsChanged();
}

void WallpaperController::CalculateWallpaperColors() {
  color_utils::LumaRange luma;
  color_utils::SaturationRange saturation;
  if (!GetProminentColorProfile(&luma, &saturation))
    return;

  if (color_calculator_)
    color_calculator_->RemoveObserver(this);

  color_calculator_ = base::MakeUnique<wallpaper::WallpaperColorCalculator>(
      GetWallpaper(), luma, saturation, task_runner_);
  color_calculator_->AddObserver(this);
  if (!color_calculator_->StartCalculation())
    SetProminentColor(SK_ColorTRANSPARENT);
}

}  // namespace ash
