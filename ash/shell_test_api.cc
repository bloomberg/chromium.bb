// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_test_api.h"

#include <memory>
#include <utility>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/keyboard/ash_keyboard_controller.h"
#include "ash/public/interfaces/app_list_view.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/workspace_controller.h"
#include "ash/ws/window_service_owner.h"
#include "base/run_loop.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ws/window_service.h"
#include "services/ws/window_tree.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace ash {
namespace {

// Wait for a WindowTreeHost to no longer be holding pointer events.
class PointerMoveLoopWaiter : public ui::CompositorObserver {
 public:
  explicit PointerMoveLoopWaiter(aura::WindowTreeHost* window_tree_host)
      : window_tree_host_(window_tree_host) {
    window_tree_host_->compositor()->AddObserver(this);
  }

  ~PointerMoveLoopWaiter() override {
    window_tree_host_->compositor()->RemoveObserver(this);
  }

  void Wait() {
    // Use a while loop as it's possible for releasing the lock to trigger
    // processing events, which again grabs the lock.
    while (window_tree_host_->holding_pointer_moves()) {
      run_loop_ = std::make_unique<base::RunLoop>(
          base::RunLoop::Type::kNestableTasksAllowed);
      run_loop_->Run();
      run_loop_.reset();
    }
  }

  // ui::CompositorObserver:
  void OnCompositingEnded(ui::Compositor* compositor) override {
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  aura::WindowTreeHost* window_tree_host_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(PointerMoveLoopWaiter);
};

// Wait until an overview animation completes. This self destruct
// after executing the callback.
class OverviewAnimationStateWaiter : public OverviewObserver {
 public:
  OverviewAnimationStateWaiter(
      mojom::OverviewAnimationState state,
      ShellTestApi::WaitForOverviewAnimationStateCallback callback)
      : state_(state), callback_(std::move(callback)) {
    Shell::Get()->overview_controller()->AddObserver(this);
  }
  ~OverviewAnimationStateWaiter() override {
    Shell::Get()->overview_controller()->RemoveObserver(this);
  }

  // OverviewObserver:
  void OnOverviewModeStartingAnimationComplete(bool canceled) override {
    if (state_ == mojom::OverviewAnimationState::kEnterAnimationComplete) {
      std::move(callback_).Run();
      delete this;
    }
  }
  void OnOverviewModeEndingAnimationComplete(bool canceled) override {
    if (state_ == mojom::OverviewAnimationState::kExitAnimationComplete) {
      std::move(callback_).Run();
      delete this;
    }
  }

 private:
  mojom::OverviewAnimationState state_;
  ShellTestApi::WaitForOverviewAnimationStateCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(OverviewAnimationStateWaiter);
};

// A waiter that waits until the animation ended with the target state, and
// execute the callback.  This self destruction upon completion.
class LauncherStateWaiter {
 public:
  LauncherStateWaiter(
      ash::mojom::AppListViewState state,
      ShellTestApi::WaitForLauncherAnimationStateCallback callback)
      : target_state_(state), callback_(std::move(callback)) {
    Shell::Get()->app_list_controller()->SetStateTransitionAnimationCallback(
        base::BindRepeating(&LauncherStateWaiter::OnStateChanged,
                            base::Unretained(this)));
  }
  ~LauncherStateWaiter() {
    Shell::Get()->app_list_controller()->SetStateTransitionAnimationCallback(
        base::NullCallback());
  }

  void OnStateChanged(ash::mojom::AppListViewState state) {
    if (target_state_ == state) {
      std::move(callback_).Run();
      delete this;
    }
  }

 private:
  ash::mojom::AppListViewState target_state_;
  ShellTestApi::WaitForLauncherAnimationStateCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(LauncherStateWaiter);
};

}  // namespace

ShellTestApi::ShellTestApi() : ShellTestApi(Shell::Get()) {}

ShellTestApi::ShellTestApi(Shell* shell) : shell_(shell) {}

// static
void ShellTestApi::BindRequest(mojom::ShellTestApiRequest request) {
  mojo::MakeStrongBinding(std::make_unique<ShellTestApi>(), std::move(request));
}

MessageCenterController* ShellTestApi::message_center_controller() {
  return shell_->message_center_controller_.get();
}

SystemGestureEventFilter* ShellTestApi::system_gesture_event_filter() {
  return shell_->system_gesture_filter_.get();
}

WorkspaceController* ShellTestApi::workspace_controller() {
  // TODO(afakhry): Split this into two, one for root, and one for context.
  return GetActiveWorkspaceController(shell_->GetPrimaryRootWindow());
}

ScreenPositionController* ShellTestApi::screen_position_controller() {
  return shell_->screen_position_controller_.get();
}

NativeCursorManagerAsh* ShellTestApi::native_cursor_manager_ash() {
  return shell_->native_cursor_manager_;
}

DragDropController* ShellTestApi::drag_drop_controller() {
  return shell_->drag_drop_controller_.get();
}

PowerPrefs* ShellTestApi::power_prefs() {
  return shell_->power_prefs_.get();
}

void ShellTestApi::OnLocalStatePrefServiceInitialized(
    std::unique_ptr<PrefService> pref_service) {
  shell_->OnLocalStatePrefServiceInitialized(std::move(pref_service));
}

void ShellTestApi::ResetPowerButtonControllerForTest() {
  shell_->backlights_forced_off_setter_->ResetForTest();
  shell_->power_button_controller_ = std::make_unique<PowerButtonController>(
      shell_->backlights_forced_off_setter_.get());
}

void ShellTestApi::SimulateModalWindowOpenForTest(bool modal_window_open) {
  shell_->simulate_modal_window_open_for_test_ = modal_window_open;
}

void ShellTestApi::IsSystemModalWindowOpen(IsSystemModalWindowOpenCallback cb) {
  std::move(cb).Run(Shell::IsSystemModalWindowOpen());
}

void ShellTestApi::EnableTabletModeWindowManager(bool enable) {
  shell_->tablet_mode_controller()->EnableTabletModeWindowManager(enable);
}

void ShellTestApi::EnableVirtualKeyboard(EnableVirtualKeyboardCallback cb) {
  shell_->ash_keyboard_controller()->SetEnableFlag(
      keyboard::mojom::KeyboardEnableFlag::kCommandLineEnabled);
  std::move(cb).Run();
}

void ShellTestApi::ToggleFullscreen(ToggleFullscreenCallback cb) {
  ash::accelerators::ToggleFullscreen();
  std::move(cb).Run();
}

void ShellTestApi::ToggleOverviewMode(ToggleOverviewModeCallback cb) {
  shell_->overview_controller()->ToggleOverview();
  std::move(cb).Run();
}

void ShellTestApi::IsOverviewSelecting(IsOverviewSelectingCallback callback) {
  std::move(callback).Run(shell_->overview_controller()->InOverviewSession());
}

void ShellTestApi::AddRemoveDisplay() {
  shell_->display_manager()->AddRemoveDisplay();
}

void ShellTestApi::SetMinFlingVelocity(float velocity) {
  ui::GestureConfiguration::GetInstance()->set_min_fling_velocity(velocity);
}

void ShellTestApi::WaitForNoPointerHoldLock(
    WaitForNoPointerHoldLockCallback callback) {
  aura::WindowTreeHost* primary_host =
      Shell::GetPrimaryRootWindowController()->GetHost();
  if (primary_host->holding_pointer_moves())
    PointerMoveLoopWaiter(primary_host).Wait();
  std::move(callback).Run();
}

void ShellTestApi::WaitForNextFrame(WaitForNextFrameCallback callback) {
  Shell::GetPrimaryRootWindowController()
      ->GetHost()
      ->compositor()
      ->RequestPresentationTimeForNextFrame(base::BindOnce(
          [](WaitForNextFrameCallback callback,
             const gfx::PresentationFeedback& feedback) {
            std::move(callback).Run();
          },
          std::move(callback)));
}

void ShellTestApi::WaitForOverviewAnimationState(
    mojom::OverviewAnimationState state,
    WaitForOverviewAnimationStateCallback callback) {
  auto* overview_controller = Shell::Get()->overview_controller();
  if (state == mojom::OverviewAnimationState::kEnterAnimationComplete &&
      overview_controller->InOverviewSession() &&
      !overview_controller->IsInStartAnimation()) {
    // If there is no animation applied, call the callback immediately.
    std::move(callback).Run();
    return;
  }
  if (state == mojom::OverviewAnimationState::kExitAnimationComplete &&
      !overview_controller->InOverviewSession() &&
      !overview_controller->IsCompletingShutdownAnimations()) {
    // If there is no animation applied, call the callback immediately.
    std::move(callback).Run();
    return;
  }
  new OverviewAnimationStateWaiter(state, std::move(callback));
}

void ShellTestApi::WaitForLauncherAnimationState(
    ash::mojom::AppListViewState target_state,
    WaitForLauncherAnimationStateCallback callback) {
  new LauncherStateWaiter(target_state, std::move(callback));
}

}  // namespace ash
