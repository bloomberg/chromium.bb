// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/shell_port_classic.h"

#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_controller_delegate_aura.h"
#include "ash/aura/pointer_watcher_adapter.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/keyboard/keyboard_ui.h"
#include "ash/laser/laser_pointer_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/shell.h"
#include "ash/virtual_keyboard_controller.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/tablet_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "ash/wm/tablet_mode/tablet_mode_event_handler_aura.h"
#include "ash/wm/window_cycle_event_filter_aura.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_handler_aura.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/env.h"
#include "ui/display/manager/chromeos/default_touch_transform_setter.h"
#include "ui/display/types/native_display_delegate.h"

#if defined(USE_X11)
#include "ui/display/manager/chromeos/x11/native_display_delegate_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace ash {

ShellPortClassic::ShellPortClassic() {}

ShellPortClassic::~ShellPortClassic() {}

// static
ShellPortClassic* ShellPortClassic::Get() {
  CHECK(Shell::GetAshConfig() == Config::CLASSIC);
  return static_cast<ShellPortClassic*>(ShellPort::Get());
}

void ShellPortClassic::Shutdown() {
  pointer_watcher_adapter_.reset();

  ShellPort::Shutdown();
}

Config ShellPortClassic::GetAshConfig() const {
  return Config::CLASSIC;
}

std::unique_ptr<display::TouchTransformSetter>
ShellPortClassic::CreateTouchTransformDelegate() {
  return base::MakeUnique<display::DefaultTouchTransformSetter>();
}

void ShellPortClassic::LockCursor() {
  Shell::Get()->cursor_manager()->LockCursor();
}

void ShellPortClassic::UnlockCursor() {
  Shell::Get()->cursor_manager()->UnlockCursor();
}

void ShellPortClassic::ShowCursor() {
  Shell::Get()->cursor_manager()->ShowCursor();
}

void ShellPortClassic::HideCursor() {
  Shell::Get()->cursor_manager()->HideCursor();
}

void ShellPortClassic::SetCursorSize(ui::CursorSize cursor_size) {
  Shell::Get()->cursor_manager()->SetCursorSize(cursor_size);
}

void ShellPortClassic::SetGlobalOverrideCursor(
    base::Optional<ui::CursorData> cursor) {
  // This is part of a fat interface that is only implemented on the mash side;
  // there isn't an equivalent operation in ::wm::CursorManager. We also can't
  // just call into ShellPortMash because of library linking issues.
  NOTREACHED();
}

bool ShellPortClassic::IsMouseEventsEnabled() {
  return Shell::Get()->cursor_manager()->IsMouseEventsEnabled();
}

std::unique_ptr<WindowResizer> ShellPortClassic::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  return base::WrapUnique(
      DragWindowResizer::Create(next_window_resizer.release(), window_state));
}

std::unique_ptr<WindowCycleEventFilter>
ShellPortClassic::CreateWindowCycleEventFilter() {
  return base::MakeUnique<WindowCycleEventFilterAura>();
}

std::unique_ptr<wm::TabletModeEventHandler>
ShellPortClassic::CreateTabletModeEventHandler() {
  return base::WrapUnique(new wm::TabletModeEventHandlerAura);
}

std::unique_ptr<WorkspaceEventHandler>
ShellPortClassic::CreateWorkspaceEventHandler(aura::Window* workspace_window) {
  return base::MakeUnique<WorkspaceEventHandlerAura>(workspace_window);
}

std::unique_ptr<ImmersiveFullscreenController>
ShellPortClassic::CreateImmersiveFullscreenController() {
  return base::MakeUnique<ImmersiveFullscreenController>();
}

std::unique_ptr<KeyboardUI> ShellPortClassic::CreateKeyboardUI() {
  return KeyboardUI::Create();
}

void ShellPortClassic::AddPointerWatcher(
    views::PointerWatcher* watcher,
    views::PointerWatcherEventTypes events) {
  pointer_watcher_adapter_->AddPointerWatcher(watcher, events);
}

void ShellPortClassic::RemovePointerWatcher(views::PointerWatcher* watcher) {
  pointer_watcher_adapter_->RemovePointerWatcher(watcher);
}

bool ShellPortClassic::IsTouchDown() {
  return aura::Env::GetInstance()->is_touch_down();
}

void ShellPortClassic::ToggleIgnoreExternalKeyboard() {
  Shell::Get()->virtual_keyboard_controller()->ToggleIgnoreExternalKeyboard();
}

void ShellPortClassic::SetLaserPointerEnabled(bool enabled) {
  Shell::Get()->laser_pointer_controller()->SetEnabled(enabled);
}

void ShellPortClassic::SetPartialMagnifierEnabled(bool enabled) {
  Shell::Get()->partial_magnification_controller()->SetEnabled(enabled);
}

void ShellPortClassic::CreatePointerWatcherAdapter() {
  pointer_watcher_adapter_ = base::MakeUnique<PointerWatcherAdapter>();
}

std::unique_ptr<AshWindowTreeHost> ShellPortClassic::CreateAshWindowTreeHost(
    const AshWindowTreeHostInitParams& init_params) {
  // A return value of null results in falling back to the default.
  return nullptr;
}

void ShellPortClassic::OnCreatedRootWindowContainers(
    RootWindowController* root_window_controller) {}

void ShellPortClassic::OnHostsInitialized() {}

std::unique_ptr<display::NativeDisplayDelegate>
ShellPortClassic::CreateNativeDisplayDelegate() {
#if defined(USE_OZONE)
  return ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate();
#else
  return base::MakeUnique<display::NativeDisplayDelegateX11>();
#endif
}

std::unique_ptr<AcceleratorController>
ShellPortClassic::CreateAcceleratorController() {
  DCHECK(!accelerator_controller_delegate_);
  accelerator_controller_delegate_ =
      base::MakeUnique<AcceleratorControllerDelegateAura>();
  return base::MakeUnique<AcceleratorController>(
      accelerator_controller_delegate_.get(), nullptr);
}

}  // namespace ash
