// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/accelerators/debug_commands.h"

#include "ash/common/accelerators/accelerator_commands.h"
#include "ash/common/ash_switches.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/system/toast/toast_data.h"
#include "ash/common/system/toast/toast_manager.h"
#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "ash/root_window_controller.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
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
  for (WmWindow* root : WmShell::Get()->GetAllRootWindows()) {
    ui::Layer* layer = root->GetLayer();
    if (layer)
      ui::PrintLayerHierarchy(
          layer, root->GetRootWindowController()->GetLastMouseLocationInRoot());
  }
}

void HandlePrintViewHierarchy() {
  WmWindow* active_window = WmShell::Get()->GetActiveWindow();
  if (!active_window)
    return;
  views::Widget* widget = active_window->GetInternalWidget();
  if (!widget)
    return;
  views::PrintViewHierarchy(widget->GetRootView());
}

void PrintWindowHierarchy(const WmWindow* active_window,
                          WmWindow* window,
                          int indent,
                          std::ostringstream* out) {
  std::string indent_str(indent, ' ');
  std::string name(window->GetName());
  if (name.empty())
    name = "\"\"";
  *out << indent_str << name << " (" << window << ")"
       << " type=" << window->GetType()
       << ((window == active_window) ? " [active] " : " ")
       << (window->IsVisible() ? " visible " : " ")
       << window->GetBounds().ToString()
       << (window->GetBoolProperty(
               WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUNDARY)
               ? " [snapped] "
               : "")
       << ", subpixel offset="
       << window->GetLayer()->subpixel_position_offset().ToString() << '\n';

  for (WmWindow* child : window->GetChildren())
    PrintWindowHierarchy(active_window, child, indent + 3, out);
}

void HandlePrintWindowHierarchy() {
  WmWindow* active_window = WmShell::Get()->GetActiveWindow();
  WmWindow::Windows roots = WmShell::Get()->GetAllRootWindows();
  for (size_t i = 0; i < roots.size(); ++i) {
    std::ostringstream out;
    out << "RootWindow " << i << ":\n";
    PrintWindowHierarchy(active_window, roots[i], 0, &out);
    // Error so logs can be collected from end-users.
    LOG(ERROR) << out.str();
  }
}

gfx::ImageSkia CreateWallpaperImage(SkColor fill, SkColor rect) {
  // TODO(oshima): Consider adding a command line option to control wallpaper
  // images for testing. The size is randomly picked.
  gfx::Size image_size(1366, 768);
  gfx::Canvas canvas(image_size, 1.0f, true);
  canvas.DrawColor(fill);
  cc::PaintFlags flags;
  flags.setColor(rect);
  flags.setStrokeWidth(10);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setBlendMode(SkBlendMode::kSrcOver);
  canvas.DrawRoundRect(gfx::Rect(image_size), 100, flags);
  return gfx::ImageSkia(canvas.ExtractImageRep());
}

void HandleToggleWallpaperMode() {
  static int index = 0;
  WallpaperController* wallpaper_controller =
      WmShell::Get()->wallpaper_controller();
  switch (++index % 4) {
    case 0:
      ash::WmShell::Get()->wallpaper_delegate()->InitializeWallpaper();
      break;
    case 1:
      wallpaper_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorRED, SK_ColorBLUE),
          wallpaper::WALLPAPER_LAYOUT_STRETCH);
      break;
    case 2:
      wallpaper_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorBLUE, SK_ColorGREEN),
          wallpaper::WALLPAPER_LAYOUT_CENTER);
      break;
    case 3:
      wallpaper_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorGREEN, SK_ColorRED),
          wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED);
      break;
  }
}

void HandleToggleTouchpad() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Touchpad"));
  ash::WmShell::Get()->delegate()->ToggleTouchpad();
}

void HandleToggleTouchscreen() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Touchscreen"));
  ShellDelegate* delegate = WmShell::Get()->delegate();
  delegate->SetTouchscreenEnabledInPrefs(
      !delegate->IsTouchscreenEnabledInPrefs(false /* use_local_state */),
      false /* use_local_state */);
  delegate->UpdateTouchscreenStatusFromPrefs();
}

void HandleToggleTouchView() {
  MaximizeModeController* controller =
      WmShell::Get()->maximize_mode_controller();
  controller->EnableMaximizeModeWindowManager(
      !controller->IsMaximizeModeWindowManagerEnabled());
}

void HandleTriggerCrash() {
  CHECK(false) << "Intentional crash via debug accelerator.";
}

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

bool DeveloperAcceleratorsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshDeveloperShortcuts);
}

void PerformDebugActionIfEnabled(AcceleratorAction action) {
  if (!DebugAcceleratorsEnabled())
    return;

  switch (action) {
    case DEBUG_PRINT_LAYER_HIERARCHY:
      HandlePrintLayerHierarchy();
      break;
    case DEBUG_PRINT_VIEW_HIERARCHY:
      HandlePrintViewHierarchy();
      break;
    case DEBUG_PRINT_WINDOW_HIERARCHY:
      HandlePrintWindowHierarchy();
      break;
    case DEBUG_SHOW_TOAST:
      WmShell::Get()->toast_manager()->Show(
          ToastData("id", base::ASCIIToUTF16("Toast"), 5000 /* duration_ms */,
                    base::ASCIIToUTF16("Dismiss")));
      break;
    case DEBUG_TOGGLE_TOUCH_PAD:
      HandleToggleTouchpad();
      break;
    case DEBUG_TOGGLE_TOUCH_SCREEN:
      HandleToggleTouchscreen();
      break;
    case DEBUG_TOGGLE_TOUCH_VIEW:
      HandleToggleTouchView();
      break;
    case DEBUG_TOGGLE_WALLPAPER_MODE:
      HandleToggleWallpaperMode();
      break;
    case DEBUG_TRIGGER_CRASH:
      HandleTriggerCrash();
      break;
    default:
      break;
  }
}

}  // namespace debug
}  // namespace ash
