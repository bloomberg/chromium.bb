// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/debug_commands.h"

#include "ash/accelerators/accelerator_commands.h"
#include "ash/ash_switches.h"
#include "ash/debug.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/debug_utils.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/debug_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace debug {
namespace {

void HandlePrintLayerHierarchy() {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (size_t i = 0; i < root_windows.size(); ++i) {
    ui::PrintLayerHierarchy(
        root_windows[i]->layer(),
        root_windows[i]->GetHost()->dispatcher()->GetLastMouseLocationInRoot());
  }
}

void HandlePrintViewHierarchy() {
  aura::Window* active_window = ash::wm::GetActiveWindow();
  if (!active_window)
    return;
  views::Widget* browser_widget =
      views::Widget::GetWidgetForNativeWindow(active_window);
  if (!browser_widget)
    return;
  views::PrintViewHierarchy(browser_widget->GetRootView());
}

void PrintWindowHierarchy(aura::Window* window,
                          int indent,
                          std::ostringstream* out) {
  std::string indent_str(indent, ' ');
  std::string name(window->name());
  if (name.empty())
    name = "\"\"";
  *out << indent_str << name << " (" << window << ")"
       << " type=" << window->type()
       << (wm::IsActiveWindow(window) ? " [active] " : " ")
       << (window->IsVisible() ? " visible " : " ")
       << window->bounds().ToString()
       << '\n';

  for (size_t i = 0; i < window->children().size(); ++i)
    PrintWindowHierarchy(window->children()[i], indent + 3, out);
}

void HandlePrintWindowHierarchy() {
  Shell::RootWindowControllerList controllers =
      Shell::GetAllRootWindowControllers();
  for (size_t i = 0; i < controllers.size(); ++i) {
    std::ostringstream out;
    out << "RootWindow " << i << ":\n";
    PrintWindowHierarchy(controllers[i]->GetRootWindow(), 0, &out);
    // Error so logs can be collected from end-users.
    LOG(ERROR) << out.str();
  }
}

gfx::ImageSkia CreateWallpaperImage(SkColor fill, SkColor rect) {
  // TODO(oshima): Consider adding a command line option to control
  // wallpaper images for testing.
  // The size is randomly picked.
  gfx::Size image_size(1366, 768);
  gfx::Canvas canvas(image_size, 1.0f, true);
  canvas.DrawColor(fill);
  SkPaint paint;
  paint.setColor(rect);
  paint.setStrokeWidth(10);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  canvas.DrawRoundRect(gfx::Rect(image_size), 100, paint);
  return gfx::ImageSkia(canvas.ExtractImageRep());
}

void HandleToggleDesktopBackgroundMode() {
  static int index = 0;
  DesktopBackgroundController* desktop_background_controller =
      Shell::GetInstance()->desktop_background_controller();
  switch (++index % 4) {
    case 0:
      ash::Shell::GetInstance()->user_wallpaper_delegate()->
          InitializeWallpaper();
      break;
    case 1:
      desktop_background_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorRED, SK_ColorBLUE),
          wallpaper::WALLPAPER_LAYOUT_STRETCH);
      break;
    case 2:
      desktop_background_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorBLUE, SK_ColorGREEN),
          wallpaper::WALLPAPER_LAYOUT_CENTER);
      break;
    case 3:
      desktop_background_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorGREEN, SK_ColorRED),
          wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED);
      break;
  }
}

#if defined(OS_CHROMEOS)

void HandleToggleTouchpad() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Touchpad"));

  ash::Shell::GetInstance()->delegate()->ToggleTouchpad();
}

void HandleToggleTouchscreen() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Touchscreen"));

  ash::Shell::GetInstance()->delegate()->ToggleTouchscreen();
}

#endif  // defined(OS_CHROMEOS)

}  // namespace

void PrintUIHierarchies() {
  // This is a separate command so the user only has to hit one key to generate
  // all the logs. Developers use the individual dumps repeatedly, so keep
  // those as separate commands to avoid spamming their logs.
  HandlePrintLayerHierarchy();
  HandlePrintWindowHierarchy();
  HandlePrintViewHierarchy();
}

bool DebugAcceleratorsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshDebugShortcuts);
}

void PerformDebugActionIfEnabled(AcceleratorAction action) {
  if (!DebugAcceleratorsEnabled())
    return;

  switch (action) {
#if defined(OS_CHROMEOS)
    case DEBUG_ADD_REMOVE_DISPLAY:
      Shell::GetInstance()->display_manager()->AddRemoveDisplay();
      break;
    case DEBUG_TOGGLE_TOUCH_PAD:
      HandleToggleTouchpad();
      break;
    case DEBUG_TOGGLE_TOUCH_SCREEN:
      HandleToggleTouchscreen();
      break;
    case DEBUG_TOGGLE_UNIFIED_DESKTOP:
      Shell::GetInstance()->display_manager()->SetUnifiedDesktopEnabled(
          !Shell::GetInstance()->display_manager()->unified_desktop_enabled());
      break;
#endif
    case DEBUG_PRINT_LAYER_HIERARCHY:
      HandlePrintLayerHierarchy();
      break;
    case DEBUG_PRINT_VIEW_HIERARCHY:
      HandlePrintViewHierarchy();
      break;
    case DEBUG_PRINT_WINDOW_HIERARCHY:
      HandlePrintWindowHierarchy();
      break;
    case DEBUG_TOGGLE_DESKTOP_BACKGROUND_MODE:
      HandleToggleDesktopBackgroundMode();
      break;
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
      Shell::GetInstance()->display_manager()->ToggleDisplayScaleFactor();
      break;
    case DEBUG_TOGGLE_ROOT_WINDOW_FULL_SCREEN:
      Shell::GetPrimaryRootWindowController()->ash_host()->ToggleFullScreen();
      break;
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
      ToggleShowDebugBorders();
      break;
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
      ToggleShowFpsCounter();
      break;
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
      ToggleShowPaintRects();
      break;
    default:
      break;
  }
}

}  // namespace debug
}  // namespace ash
