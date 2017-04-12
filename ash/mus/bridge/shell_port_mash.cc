// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/shell_port_mash.h"

#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_controller_delegate_aura.h"
#include "ash/aura/key_event_watcher_aura.h"
#include "ash/aura/pointer_watcher_adapter.h"
#include "ash/key_event_watcher.h"
#include "ash/laser/laser_pointer_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/mus/accelerators/accelerator_controller_delegate_mus.h"
#include "ash/mus/accelerators/accelerator_controller_registrar.h"
#include "ash/mus/bridge/immersive_handler_factory_mus.h"
#include "ash/mus/bridge/workspace_event_handler_mus.h"
#include "ash/mus/drag_window_resizer.h"
#include "ash/mus/keyboard_ui_mus.h"
#include "ash/mus/screen_mus.h"
#include "ash/mus/window_manager.h"
#include "ash/public/cpp/config.h"
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
#include "ash/wm_window.h"
#include "base/memory/ptr_util.h"
#include "components/user_manager/user_info_impl.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/views/mus/pointer_watcher_event_router.h"

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
  DCHECK(primary_root_window_);

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
  if (mus_state_)
    mus_state_->pointer_watcher_adapter.reset();

  ShellPort::Shutdown();

  window_manager_->DeleteAllRootWindowControllers();
}

Config ShellPortMash::GetAshConfig() const {
  return window_manager_->config();
}

WmWindow* ShellPortMash::GetPrimaryRootWindow() {
  // NOTE: This is called before the RootWindowController has been created, so
  // it can't call through to RootWindowController to get all windows.
  return primary_root_window_;
}

WmWindow* ShellPortMash::GetRootWindowForDisplayId(int64_t display_id) {
  RootWindowController* root_window_controller =
      GetRootWindowControllerWithDisplayId(display_id);
  return root_window_controller
             ? WmWindow::Get(root_window_controller->GetRootWindow())
             : nullptr;
}

const display::ManagedDisplayInfo& ShellPortMash::GetDisplayInfo(
    int64_t display_id) const {
  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  static display::ManagedDisplayInfo fake_info;
  return fake_info;
}

bool ShellPortMash::IsActiveDisplayId(int64_t display_id) const {
  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return true;
}

display::Display ShellPortMash::GetFirstDisplay() const {
  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return display::Screen::GetScreen()->GetPrimaryDisplay();
}

bool ShellPortMash::IsInUnifiedMode() const {
  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return false;
}

bool ShellPortMash::IsInUnifiedModeIgnoreMirroring() const {
  // TODO(mash): implement http://crbug.com/622480.
  NOTIMPLEMENTED();
  return false;
}

void ShellPortMash::SetDisplayWorkAreaInsets(WmWindow* window,
                                             const gfx::Insets& insets) {
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
  // TODO: need WmDisplayObserver support for mus. http://crbug.com/705831.
  NOTIMPLEMENTED();
}

void ShellPortMash::RemoveDisplayObserver(WmDisplayObserver* observer) {
  // TODO: need WmDisplayObserver support for mus. http://crbug.com/705831.
  NOTIMPLEMENTED();
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

void ShellPortMash::CreatePrimaryHost() {}

void ShellPortMash::InitHosts(const ShellInitParams& init_params) {
  window_manager_->CreatePrimaryRootWindowController(
      base::WrapUnique(init_params.primary_window_tree_host));
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

}  // namespace mus
}  // namespace ash
