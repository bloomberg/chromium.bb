// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/shell_port_mash.h"

#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_controller_delegate_aura.h"
#include "ash/aura/key_event_watcher_aura.h"
#include "ash/aura/pointer_watcher_adapter.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/key_event_watcher.h"
#include "ash/laser/laser_pointer_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/mus/accelerators/accelerator_controller_delegate_mus.h"
#include "ash/mus/accelerators/accelerator_controller_registrar.h"
#include "ash/mus/ash_window_tree_host_mus.h"
#include "ash/mus/bridge/immersive_handler_factory_mus.h"
#include "ash/mus/bridge/workspace_event_handler_mus.h"
#include "ash/mus/drag_window_resizer.h"
#include "ash/mus/keyboard_ui_mus.h"
#include "ash/mus/screen_mus.h"
#include "ash/mus/window_manager.h"
#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shared/immersive_fullscreen_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_init_params.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/touch/touch_uma.h"
#include "ash/virtual_keyboard_controller.h"
#include "ash/wallpaper/wallpaper_delegate.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/maximize_mode/maximize_mode_event_handler.h"
#include "ash/wm/maximize_mode/maximize_mode_event_handler_aura.h"
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_ozone.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_cycle_event_filter.h"
#include "ash/wm/window_cycle_event_filter_aura.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_event_handler_aura.h"
#include "ash/wm_display_observer.h"
#include "ash/wm_window.h"
#include "base/command_line.h"
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
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/views/mus/pointer_watcher_event_router.h"

#if defined(USE_OZONE)
#include "ui/display/manager/forwarding_display_delegate.h"
#endif

namespace ash {
namespace mus {

namespace {

// TODO(jamescook): After ShellDelegate is ported to ash/common use
// ShellDelegate::CreateSessionStateDelegate() to construct the mus version
// of SessionStateDelegate.
class SessionStateDelegateStub : public SessionStateDelegate {
 public:
  SessionStateDelegateStub() : user_info_(new user_manager::UserInfoImpl()) {}

  ~SessionStateDelegateStub() override {}

  // SessionStateDelegate:
  bool ShouldShowAvatar(WmWindow* window) const override {
    NOTIMPLEMENTED();
    return !user_info_->GetImage().isNull();
  }
  gfx::ImageSkia GetAvatarImageForWindow(WmWindow* window) const override {
    NOTIMPLEMENTED();
    return gfx::ImageSkia();
  }

 private:
  // A pseudo user info.
  std::unique_ptr<user_manager::UserInfo> user_info_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateStub);
};

}  // namespace

ShellPortMash::MashSpecificState::MashSpecificState() = default;

ShellPortMash::MashSpecificState::~MashSpecificState() = default;

ShellPortMash::MusSpecificState::MusSpecificState() = default;

ShellPortMash::MusSpecificState::~MusSpecificState() = default;

ShellPortMash::ShellPortMash(
    WmWindow* primary_root_window,
    WindowManager* window_manager,
    views::PointerWatcherEventRouter* pointer_watcher_event_router,
    bool create_session_state_delegate_stub)
    : window_manager_(window_manager),
      primary_root_window_(primary_root_window) {
  if (create_session_state_delegate_stub)
    session_state_delegate_ = base::MakeUnique<SessionStateDelegateStub>();

  if (GetAshConfig() == Config::MASH) {
    mash_state_ = base::MakeUnique<MashSpecificState>();
    mash_state_->pointer_watcher_event_router = pointer_watcher_event_router;
    mash_state_->immersive_handler_factory =
        base::MakeUnique<ImmersiveHandlerFactoryMus>();
  } else {
    DCHECK_EQ(Config::MUS, GetAshConfig());
    mus_state_ = base::MakeUnique<MusSpecificState>();
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
  if (added_display_observer_)
    Shell::Get()->window_tree_host_manager()->RemoveObserver(this);

  if (mus_state_)
    mus_state_->pointer_watcher_adapter.reset();

  ShellPort::Shutdown();

  // TODO(sky): Config::MASH should use WindowTreeHostManager too.
  if (GetAshConfig() == Config::MUS)
    Shell::Get()->window_tree_host_manager()->Shutdown();
  else
    window_manager_->DeleteAllRootWindowControllers();
}

Config ShellPortMash::GetAshConfig() const {
  return window_manager_->config();
}

WmWindow* ShellPortMash::GetPrimaryRootWindow() {
  if (GetAshConfig() == Config::MUS) {
    return WmWindow::Get(
        Shell::Get()->window_tree_host_manager()->GetPrimaryRootWindow());
  }
  // NOTE: This is called before the RootWindowController has been created, so
  // it can't call through to RootWindowController to get all windows.
  return primary_root_window_;
}

WmWindow* ShellPortMash::GetRootWindowForDisplayId(int64_t display_id) {
  if (GetAshConfig() == Config::MUS) {
    return WmWindow::Get(
        Shell::Get()->window_tree_host_manager()->GetRootWindowForDisplayId(
            display_id));
  }
  RootWindowController* root_window_controller =
      GetRootWindowControllerWithDisplayId(display_id);
  return root_window_controller
             ? WmWindow::Get(root_window_controller->GetRootWindow())
             : nullptr;
}

const display::ManagedDisplayInfo& ShellPortMash::GetDisplayInfo(
    int64_t display_id) const {
  // TODO(sky): mash should use this too http://crbug.com/718860.
  if (GetAshConfig() == Config::MUS)
    return Shell::Get()->display_manager()->GetDisplayInfo(display_id);

  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  static display::ManagedDisplayInfo fake_info;
  return fake_info;
}

bool ShellPortMash::IsActiveDisplayId(int64_t display_id) const {
  // TODO(sky): mash should use this too http://crbug.com/718860.
  if (GetAshConfig() == Config::MUS)
    return Shell::Get()->display_manager()->IsActiveDisplayId(display_id);

  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return true;
}

display::Display ShellPortMash::GetFirstDisplay() const {
  // TODO(sky): mash should use this too http://crbug.com/718860.
  if (GetAshConfig() == Config::MUS) {
    return Shell::Get()
        ->display_manager()
        ->software_mirroring_display_list()[0];
  }

  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return display::Screen::GetScreen()->GetPrimaryDisplay();
}

bool ShellPortMash::IsInUnifiedMode() const {
  // TODO(sky): mash should use this too http://crbug.com/718860.
  if (GetAshConfig() == Config::MUS)
    return Shell::Get()->display_manager()->IsInUnifiedMode();

  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return false;
}

bool ShellPortMash::IsInUnifiedModeIgnoreMirroring() const {
  // TODO(sky): mash should use this too http://crbug.com/718860.
  if (GetAshConfig() == Config::MUS) {
    return Shell::Get()
               ->display_manager()
               ->current_default_multi_display_mode() ==
           display::DisplayManager::UNIFIED;
  }

  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return false;
}

void ShellPortMash::SetDisplayWorkAreaInsets(WmWindow* window,
                                             const gfx::Insets& insets) {
  if (GetAshConfig() == Config::MUS) {
    Shell::Get()
        ->window_tree_host_manager()
        ->UpdateWorkAreaOfDisplayNearestWindow(window->aura_window(), insets);
    return;
  }
  window_manager_->screen()->SetWorkAreaInsets(window->aura_window(), insets);
}

void ShellPortMash::LockCursor() {
  // TODO: http://crbug.com/637853
  NOTIMPLEMENTED();
}

void ShellPortMash::UnlockCursor() {
  // TODO: http://crbug.com/637853
  NOTIMPLEMENTED();
}

bool ShellPortMash::IsMouseEventsEnabled() {
  // TODO: http://crbug.com/637853
  NOTIMPLEMENTED();
  return true;
}

std::vector<WmWindow*> ShellPortMash::GetAllRootWindows() {
  if (GetAshConfig() == Config::MUS) {
    aura::Window::Windows root_windows =
        Shell::Get()->window_tree_host_manager()->GetAllRootWindows();
    std::vector<WmWindow*> wm_windows(root_windows.size());
    for (size_t i = 0; i < root_windows.size(); ++i)
      wm_windows[i] = WmWindow::Get(root_windows[i]);
    return wm_windows;
  }
  std::vector<WmWindow*> root_windows;
  for (RootWindowController* root_window_controller :
       RootWindowController::root_window_controllers()) {
    root_windows.push_back(root_window_controller->GetWindow());
  }
  return root_windows;
}

void ShellPortMash::RecordGestureAction(GestureActionType action) {
  if (GetAshConfig() == Config::MUS) {
    TouchUMA::GetInstance()->RecordGestureAction(action);
    return;
  }
  // TODO: http://crbug.com/616581.
  NOTIMPLEMENTED();
}

void ShellPortMash::RecordUserMetricsAction(UserMetricsAction action) {
  if (GetAshConfig() == Config::MUS) {
    Shell::Get()->metrics()->RecordUserMetricsAction(action);
    return;
  }
  // TODO: http://crbug.com/616581.
  NOTIMPLEMENTED();
}

void ShellPortMash::RecordTaskSwitchMetric(TaskSwitchSource source) {
  if (GetAshConfig() == Config::MUS) {
    Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
        source);
    return;
  }
  // TODO: http://crbug.com/616581.
  NOTIMPLEMENTED();
}

std::unique_ptr<WindowResizer> ShellPortMash::CreateDragWindowResizer(
    std::unique_ptr<WindowResizer> next_window_resizer,
    wm::WindowState* window_state) {
  if (GetAshConfig() == Config::MUS) {
    return base::WrapUnique(ash::DragWindowResizer::Create(
        next_window_resizer.release(), window_state));
  }
  return base::MakeUnique<ash::mus::DragWindowResizer>(
      std::move(next_window_resizer), window_state);
}

std::unique_ptr<WindowCycleEventFilter>
ShellPortMash::CreateWindowCycleEventFilter() {
  if (GetAshConfig() == Config::MUS)
    return base::MakeUnique<WindowCycleEventFilterAura>();

  // TODO: implement me, http://crbug.com/629191.
  return nullptr;
}

std::unique_ptr<wm::MaximizeModeEventHandler>
ShellPortMash::CreateMaximizeModeEventHandler() {
  if (GetAshConfig() == Config::MUS)
    return base::MakeUnique<wm::MaximizeModeEventHandlerAura>();

  // TODO: need support for window manager to get events before client:
  // http://crbug.com/624157.
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<ScopedDisableInternalMouseAndKeyboard>
ShellPortMash::CreateScopedDisableInternalMouseAndKeyboard() {
  if (GetAshConfig() == Config::MUS) {
#if defined(USE_OZONE)
    return base::MakeUnique<ScopedDisableInternalMouseAndKeyboardOzone>();
#else
    // TODO: remove this conditional. Bots build this config, but it is never
    // actually used. http://crbug.com/671355.
    NOTREACHED();
    return nullptr;
#endif
  }

  // TODO: needs implementation for mus, http://crbug.com/624967.
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<WorkspaceEventHandler>
ShellPortMash::CreateWorkspaceEventHandler(WmWindow* workspace_window) {
  if (GetAshConfig() == Config::MUS)
    return base::MakeUnique<WorkspaceEventHandlerAura>(workspace_window);

  return base::MakeUnique<WorkspaceEventHandlerMus>(
      WmWindow::GetAuraWindow(workspace_window));
}

std::unique_ptr<ImmersiveFullscreenController>
ShellPortMash::CreateImmersiveFullscreenController() {
  return base::MakeUnique<ImmersiveFullscreenController>();
}

std::unique_ptr<KeyboardUI> ShellPortMash::CreateKeyboardUI() {
  if (GetAshConfig() == Config::MUS)
    return KeyboardUI::Create();

  return KeyboardUIMus::Create(window_manager_->connector());
}

std::unique_ptr<KeyEventWatcher> ShellPortMash::CreateKeyEventWatcher() {
  if (GetAshConfig() == Config::MUS)
    return base::MakeUnique<KeyEventWatcherAura>();

  // TODO: needs implementation for mus, http://crbug.com/649600.
  NOTIMPLEMENTED();
  return std::unique_ptr<KeyEventWatcher>();
}

SessionStateDelegate* ShellPortMash::GetSessionStateDelegate() {
  return session_state_delegate_ ? session_state_delegate_.get()
                                 : Shell::Get()->session_state_delegate();
}

void ShellPortMash::AddDisplayObserver(WmDisplayObserver* observer) {
  // TODO(sky): mash should use the same code as mus/classic and
  // WmDisplayObserver should be removed; http://crbug.com/718860.
  if (GetAshConfig() == Config::MASH) {
    NOTIMPLEMENTED();
    return;
  }
  if (!added_display_observer_) {
    added_display_observer_ = true;
    Shell::Get()->window_tree_host_manager()->AddObserver(this);
  }
  display_observers_.AddObserver(observer);
}

void ShellPortMash::RemoveDisplayObserver(WmDisplayObserver* observer) {
  // TODO(sky): mash should use the same code as mus/classic and
  // WmDisplayObserver should be removed; http://crbug.com/718860.
  if (GetAshConfig() == Config::MASH) {
    NOTIMPLEMENTED();
    return;
  }
  display_observers_.RemoveObserver(observer);
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
  // In Config::MUS PointerWatcherAdapter must be created when this function is
  // called (it is order dependent), that is not the case with Config::MASH.
  if (GetAshConfig() == Config::MUS) {
    mus_state_->pointer_watcher_adapter =
        base::MakeUnique<PointerWatcherAdapter>();
  }
}

std::unique_ptr<AshWindowTreeHost> ShellPortMash::CreateAshWindowTreeHost(
    const AshWindowTreeHostInitParams& init_params) {
  // TODO(sky): make this work for mash too.
  if (GetAshConfig() != Config::MUS)
    return nullptr;

  std::unique_ptr<aura::DisplayInitParams> display_params =
      base::MakeUnique<aura::DisplayInitParams>();
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
        base::MakeUnique<display::Display>(mirrored_display);
  }
  // TODO: wire update is_primary_display correctly.
  display_params->is_primary_display = true;
  aura::WindowTreeHostMusInitParams aura_init_params =
      window_manager_->window_manager_client()->CreateInitParamsForNewDisplay();
  aura_init_params.display_id = init_params.display_id;
  aura_init_params.display_init_params = std::move(display_params);
  aura_init_params.use_classic_ime = !Shell::ShouldUseIMEService();
  return base::MakeUnique<AshWindowTreeHostMus>(std::move(aura_init_params));
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
}

void ShellPortMash::CreatePrimaryHost() {
  if (GetAshConfig() == Config::MASH)
    return;

  DCHECK_EQ(Config::MUS, GetAshConfig());
  Shell::Get()->window_tree_host_manager()->Start();
  AshWindowTreeHostInitParams ash_init_params;
  Shell::Get()->window_tree_host_manager()->CreatePrimaryHost(ash_init_params);
}

void ShellPortMash::InitHosts(const ShellInitParams& init_params) {
  if (GetAshConfig() == Config::MUS) {
    Shell::Get()->window_tree_host_manager()->InitHosts();
  } else {
    window_manager_->CreatePrimaryRootWindowController(
        base::WrapUnique(init_params.primary_window_tree_host));
  }
}

std::unique_ptr<display::NativeDisplayDelegate>
ShellPortMash::CreateNativeDisplayDelegate() {
#if defined(USE_OZONE)
  display::mojom::NativeDisplayDelegatePtr native_display_delegate;
  if (window_manager_->connector()) {
    window_manager_->connector()->BindInterface(ui::mojom::kServiceName,
                                                &native_display_delegate);
  }
  return base::MakeUnique<display::ForwardingDisplayDelegate>(
      std::move(native_display_delegate));
#else
  // The bots compile this config, but it is never run.
  CHECK(false);
  return nullptr;
#endif
}

std::unique_ptr<AcceleratorController>
ShellPortMash::CreateAcceleratorController() {
  if (GetAshConfig() == Config::MUS) {
    DCHECK(!mus_state_->accelerator_controller_delegate);
    mus_state_->accelerator_controller_delegate =
        base::MakeUnique<AcceleratorControllerDelegateAura>();
    return base::MakeUnique<AcceleratorController>(
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
      base::MakeUnique<AcceleratorControllerDelegateMus>(window_manager_);
  mash_state_->accelerator_controller_registrar =
      base ::MakeUnique<AcceleratorControllerRegistrar>(
          window_manager_, accelerator_namespace_id);
  return base::MakeUnique<AcceleratorController>(
      mash_state_->accelerator_controller_delegate.get(),
      mash_state_->accelerator_controller_registrar.get());
}

void ShellPortMash::OnDisplayConfigurationChanging() {
  for (auto& observer : display_observers_)
    observer.OnDisplayConfigurationChanging();
}

void ShellPortMash::OnDisplayConfigurationChanged() {
  for (auto& observer : display_observers_)
    observer.OnDisplayConfigurationChanged();
}

}  // namespace mus
}  // namespace ash
