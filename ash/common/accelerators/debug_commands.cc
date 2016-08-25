// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/accelerators/debug_commands.h"

#include "ash/common/accelerators/accelerator_commands.h"
#include "ash/common/ash_switches.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/system/toast/toast_data.h"
#include "ash/common/system/toast/toast_manager.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/compositor/debug_utils.h"
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
       << window->GetBounds().ToString() << '\n';

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

#if defined(OS_CHROMEOS)

void HandleToggleTouchpad() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Touchpad"));
  ash::WmShell::Get()->delegate()->ToggleTouchpad();
}

void HandleToggleTouchscreen() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Touchscreen"));
  ash::WmShell::Get()->delegate()->ToggleTouchscreen();
}

void HandleToggleToggleTouchView() {
  MaximizeModeController* controller =
      WmShell::Get()->maximize_mode_controller();
  controller->EnableMaximizeModeWindowManager(
      !controller->IsMaximizeModeWindowManagerEnabled());
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
      HandleToggleToggleTouchView();
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
    default:
      break;
  }
}

}  // namespace debug
}  // namespace ash
