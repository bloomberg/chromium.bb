// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/shell_port_mash.h"

#include <memory>
#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_controller_delegate_classic.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/laser/laser_pointer_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/mus/accelerators/accelerator_controller_delegate_mus.h"
#include "ash/mus/accelerators/accelerator_controller_registrar.h"
#include "ash/mus/ash_window_tree_host_mus.h"
#include "ash/mus/bridge/immersive_handler_factory_mus.h"
#include "ash/mus/bridge/workspace_event_handler_mus.h"
#include "ash/mus/display_synchronizer.h"
#include "ash/mus/drag_window_resizer.h"
#include "ash/mus/keyboard_ui_mus.h"
#include "ash/mus/touch_transform_setter_mus.h"
#include "ash/mus/window_manager.h"
#include "ash/pointer_watcher_adapter_classic.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "ash/virtual_keyboard_controller.h"
#include "ash/wallpaper/wallpaper_delegate.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_event_handler.h"
#include "ash/wm/tablet_mode/tablet_mode_event_handler_classic.h"
#include "ash/wm/window_cycle_event_filter.h"
#include "ash/wm/window_cycle_event_filter_classic.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_handler_classic.h"
#include "base/memory/ptr_util.h"
#include "components/user_manager/user_info_impl.h"
#include "services/ui/public/interfaces/constants.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/focus_synchronizer.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/forwarding_display_delegate.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/views/mus/pointer_watcher_event_router.h"

namespace ash {
namespace mus {

ShellPortMash::MashSpecificState::MashSpecificState() = default;

ShellPortMash::MashSpecificState::~MashSpecificState() = default;

ShellPortMash::MusSpecificState::MusSpecificState() = default;

ShellPortMash::MusSpecificState::~MusSpecificState() = default;

ShellPortMash::ShellPortMash(
    WindowManager* window_manager,
    views::PointerWatcherEventRouter* pointer_watcher_event_router)
    : window_manager_(window_manager) {
  if (GetAshConfig() == Config::MASH) {
    mash_state_ = std::make_unique<MashSpecificState>();
    mash_state_->pointer_watcher_event_router = pointer_watcher_event_router;
    mash_state_->immersive_handler_factory =
        std::make_unique<ImmersiveHandlerFactoryMus>();
  } else {
    DCHECK_EQ(Config::MUS, GetAshConfig());
    mus_state_ = std::make_unique<MusSpecificState>();
  }
}

ShellPortMash::~ShellPortMash() {}

// static
ShellPortMash* ShellPortMash::Get() {
  const ash::Config config = ShellPort::Get()->GetAshConfig();
  CHECK(config == Config::MUS || config == Config::MASH);
  return static_cast<ShellPortMash*>(ShellPort::Get());
}

RootWindowController* ShellPortMash::GetRootWindowControllerWithDisplayId(
    int64_t id) {
  for (RootWindowController* root_window_controller :
       RootWindowController::root_window_controllers()) {
    RootWindowSettings* settings =
        GetRootWindowSettings(root_window_controller->GetRootWindow());
    DCHECK(settings);
    if (settings->display_id == id)
      return root_window_controller;
  }
  return nullptr;
}

aura::WindowTreeClient* ShellPortMash::window_tree_client() {
  return window_manager_->window_tree_client();
}

void ShellPortMash::Shutdown() {
  display_synchronizer_.reset();

  if (mus_state_)
    mus_state_->pointer_watcher_adapter.reset();

  ShellPort::Shutdown();
}

Config ShellPortMash::GetAshConfig() const {
  return window_manager_->config();
}

std::unique_ptr<display::TouchTransformSetter>
ShellPortMash::CreateTouchTransformDelegate() {
  return std::make_unique<TouchTransformSetterMus>(
      window_manager_->connector());
}

void ShellPortMash::LockCursor() {
  // When we are running in mus, we need to keep track of state not just in the
  // window server, but also locally in ash because ash treats the cursor
  // manager as the canonical state for now. NativeCursorManagerAsh will keep
  // this state, while also forwarding it to the window manager for us.
  if (GetAshConfig() == Config::MUS)
    Shell::Get()->cursor_manager()->LockCursor();
  else
    window_manager_->window_manager_client()->LockCursor();
}

void ShellPortMash::UnlockCursor() {
  if (GetAshConfig() == Config::MUS)
    Shell::Get()->cursor_manager()->UnlockCursor();
  else
    window_manager_->window_manager_client()->UnlockCursor();
}

void ShellPortMash::ShowCursor() {
  if (GetAshConfig() == Config::MUS)
    Shell::Get()->cursor_manager()->ShowCursor();
  else
    window_manager_->window_manager_client()->SetCursorVisible(true);
}

void ShellPortMash::HideCursor() {
  if (GetAshConfig() == Config::MUS)
    Shell::Get()->cursor_manager()->HideCursor();
  else
    window_manager_->window_manager_client()->SetCursorVisible(false);
}

void ShellPortMash::SetCursorSize(ui::CursorSize cursor_size) {
  if (GetAshConfig() == Config::MUS)
    Shell::Get()->cursor_manager()->SetCursorSize(cursor_size);
  else
    window_manager_->window_manager_client()->SetCursorSize(cursor_size);
}

void ShellPortMash::SetGlobalOverrideCursor(
    base::Optional<ui::CursorData> cursor) {
  DCHECK(mash_state_);
  window_manager_->window_manager_client()->SetGlobalOverrideCursor(
      std::move(cursor));
}

bool ShellPortMash::IsMouseEventsEnabled() {
  if (GetAshConfig() == Config::MASH)
    return cursor_touch_visible_;

  return Shell::Get()->cursor_manager()->IsMouseEventsEnabled();
}

void ShellPortMash::SetCursorTouchVisible(bool enabled) {
  DCHECK_EQ(GetAshConfig(), Config::MASH);
  window_manager_->window_manager_client()->SetCursorTouchVisible(enabled);
}

void ShellPortMash::OnCursorTouchVisibleChanged(bool enabled) {
  if (GetAshConfig() == Config::MASH)
    cursor_touch_visible_ = enabled;
}

std::unique_ptr<WindowResizer> ShellPortMash::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  if (GetAshConfig() == Config::MUS) {
    return base::WrapUnique(ash::DragWindowResizer::Create(
        next_window_resizer.release(), window_state));
  }
  return std::make_unique<ash::mus::DragWindowResizer>(
      std::move(next_window_resizer), window_state);
}

std::unique_ptr<WindowCycleEventFilter>
ShellPortMash::CreateWindowCycleEventFilter() {
  if (GetAshConfig() == Config::MUS)
    return std::make_unique<WindowCycleEventFilterClassic>();

  // TODO: implement me, http://crbug.com/629191.
  return nullptr;
}

std::unique_ptr<wm::TabletModeEventHandler>
ShellPortMash::CreateTabletModeEventHandler() {
  if (GetAshConfig() == Config::MUS)
    return std::make_unique<wm::TabletModeEventHandlerClassic>();

  // TODO: need support for window manager to get events before client:
  // http://crbug.com/624157.
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<WorkspaceEventHandler>
ShellPortMash::CreateWorkspaceEventHandler(aura::Window* workspace_window) {
  if (GetAshConfig() == Config::MUS)
    return std::make_unique<WorkspaceEventHandlerClassic>(workspace_window);

  return std::make_unique<WorkspaceEventHandlerMus>(workspace_window);
}

std::unique_ptr<ImmersiveFullscreenController>
ShellPortMash::CreateImmersiveFullscreenController() {
  return std::make_unique<ImmersiveFullscreenController>();
}

std::unique_ptr<KeyboardUI> ShellPortMash::CreateKeyboardUI() {
  if (GetAshConfig() == Config::MUS)
    return KeyboardUI::Create();

  return KeyboardUIMus::Create(window_manager_->connector());
}

void ShellPortMash::AddPointerWatcher(views::PointerWatcher* watcher,
                                      views::PointerWatcherEventTypes events) {
  if (GetAshConfig() == Config::MUS) {
    mus_state_->pointer_watcher_adapter->AddPointerWatcher(watcher, events);
    return;
  }

  // TODO: implement drags for mus pointer watcher, http://crbug.com/641164.
  // NOTIMPLEMENTED drags for mus pointer watcher.
  mash_state_->pointer_watcher_event_router->AddPointerWatcher(
      watcher, events == views::PointerWatcherEventTypes::MOVES);
}

void ShellPortMash::RemovePointerWatcher(views::PointerWatcher* watcher) {
  if (GetAshConfig() == Config::MUS) {
    mus_state_->pointer_watcher_adapter->RemovePointerWatcher(watcher);
    return;
  }

  mash_state_->pointer_watcher_event_router->RemovePointerWatcher(watcher);
}

bool ShellPortMash::IsTouchDown() {
  if (GetAshConfig() == Config::MUS)
    return aura::Env::GetInstance()->is_touch_down();

  // TODO: implement me, http://crbug.com/634967.
  // NOTIMPLEMENTED is too spammy here.
  return false;
}

void ShellPortMash::ToggleIgnoreExternalKeyboard() {
  if (GetAshConfig() == Config::MUS) {
    Shell::Get()->virtual_keyboard_controller()->ToggleIgnoreExternalKeyboard();
    return;
  }

  NOTIMPLEMENTED();
}

void ShellPortMash::SetLaserPointerEnabled(bool enabled) {
  if (GetAshConfig() == Config::MUS) {
    Shell::Get()->laser_pointer_controller()->SetEnabled(enabled);
    return;
  }

  NOTIMPLEMENTED();
}

void ShellPortMash::SetPartialMagnifierEnabled(bool enabled) {
  if (GetAshConfig() == Config::MUS) {
    Shell::Get()->partial_magnification_controller()->SetEnabled(enabled);
    return;
  }

  NOTIMPLEMENTED();
}

void ShellPortMash::CreatePointerWatcherAdapter() {
  // In Config::MUS PointerWatcherAdapterClassic must be created when this
  // function is called (it is order dependent), that is not the case with
  // Config::MASH.
  if (GetAshConfig() == Config::MUS) {
    mus_state_->pointer_watcher_adapter =
        std::make_unique<PointerWatcherAdapterClassic>();
  }
}

std::unique_ptr<AshWindowTreeHost> ShellPortMash::CreateAshWindowTreeHost(
    const AshWindowTreeHostInitParams& init_params) {
  std::unique_ptr<aura::DisplayInitParams> display_params =
      std::make_unique<aura::DisplayInitParams>();
  display_params->viewport_metrics.bounds_in_pixels =
      init_params.initial_bounds;
  display_params->viewport_metrics.device_scale_factor =
      init_params.device_scale_factor;
  display_params->viewport_metrics.ui_scale_factor =
      init_params.ui_scale_factor;
  display::Display mirrored_display =
      Shell::Get()->display_manager()->GetMirroringDisplayById(
          init_params.display_id);
  if (mirrored_display.is_valid()) {
    display_params->display =
        std::make_unique<display::Display>(mirrored_display);
  }
  display_params->is_primary_display = true;
  aura::WindowTreeHostMusInitParams aura_init_params =
      window_manager_->window_manager_client()->CreateInitParamsForNewDisplay();
  aura_init_params.display_id = init_params.display_id;
  aura_init_params.display_init_params = std::move(display_params);
  aura_init_params.use_classic_ime = !Shell::ShouldUseIMEService();
  return std::make_unique<AshWindowTreeHostMus>(std::move(aura_init_params));
}

void ShellPortMash::OnCreatedRootWindowContainers(
    RootWindowController* root_window_controller) {
  // TODO: To avoid lots of IPC AddActivationParent() should take an array.
  // http://crbug.com/682048.
  aura::Window* root_window = root_window_controller->GetRootWindow();
  for (size_t i = 0; i < kNumActivatableShellWindowIds; ++i) {
    window_manager_->window_manager_client()->AddActivationParent(
        root_window->GetChildById(kActivatableShellWindowIds[i]));
  }

  UpdateSystemModalAndBlockingContainers();
}

void ShellPortMash::UpdateSystemModalAndBlockingContainers() {
  std::vector<aura::BlockingContainers> all_blocking_containers;
  for (RootWindowController* root_window_controller :
       Shell::GetAllRootWindowControllers()) {
    aura::BlockingContainers blocking_containers;
    wm::GetBlockingContainersForRoot(
        root_window_controller->GetRootWindow(),
        &blocking_containers.min_container,
        &blocking_containers.system_modal_container);
    all_blocking_containers.push_back(blocking_containers);
  }
  window_manager_->window_manager_client()->SetBlockingContainers(
      all_blocking_containers);
}

void ShellPortMash::OnHostsInitialized() {
  display_synchronizer_ = std::make_unique<DisplaySynchronizer>(
      window_manager_->window_manager_client());
}

std::unique_ptr<display::NativeDisplayDelegate>
ShellPortMash::CreateNativeDisplayDelegate() {
  display::mojom::NativeDisplayDelegatePtr native_display_delegate;
  if (window_manager_->connector()) {
    window_manager_->connector()->BindInterface(ui::mojom::kServiceName,
                                                &native_display_delegate);
  }
  return std::make_unique<display::ForwardingDisplayDelegate>(
      std::move(native_display_delegate));
}

std::unique_ptr<AcceleratorController>
ShellPortMash::CreateAcceleratorController() {
  if (GetAshConfig() == Config::MUS) {
    DCHECK(!mus_state_->accelerator_controller_delegate);
    mus_state_->accelerator_controller_delegate =
        std::make_unique<AcceleratorControllerDelegateClassic>();
    return std::make_unique<AcceleratorController>(
        mus_state_->accelerator_controller_delegate.get(), nullptr);
  }

  DCHECK(!mash_state_->accelerator_controller_delegate);

  uint16_t accelerator_namespace_id = 0u;
  const bool add_result =
      window_manager_->GetNextAcceleratorNamespaceId(&accelerator_namespace_id);
  // ShellPortMash is created early on, so that GetNextAcceleratorNamespaceId()
  // should always succeed.
  DCHECK(add_result);

  mash_state_->accelerator_controller_delegate =
      std::make_unique<AcceleratorControllerDelegateMus>(window_manager_);
  mash_state_->accelerator_controller_registrar =
      base ::MakeUnique<AcceleratorControllerRegistrar>(
          window_manager_, accelerator_namespace_id);
  return std::make_unique<AcceleratorController>(
      mash_state_->accelerator_controller_delegate.get(),
      mash_state_->accelerator_controller_registrar.get());
}

}  // namespace mus
}  // namespace ash
