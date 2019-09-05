// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"

#include <memory>
#include <utility>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/presenter/app_list_presenter_impl.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_launcher_gesture_handler_observer.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/power/backlights_forced_off_setter.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/wm/overview/overview_animation_state_waiter.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/workspace_controller.h"
#include "base/run_loop.h"
#include "components/prefs/testing_pref_service.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
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

class HomeLauncherStateWaiter : public HomeLauncherGestureHandlerObserver {
 public:
  HomeLauncherStateWaiter(bool target_shown, base::OnceClosure closure)
      : target_shown_(target_shown), closure_(std::move(closure)) {
    Shell::Get()
        ->home_screen_controller()
        ->home_launcher_gesture_handler()
        ->AddObserver(this);
  }
  ~HomeLauncherStateWaiter() override {
    Shell::Get()
        ->home_screen_controller()
        ->home_launcher_gesture_handler()
        ->RemoveObserver(this);
  }

 private:
  // HomeLauncherGestureHandlerObserver:
  void OnHomeLauncherAnimationComplete(bool shown,
                                       int64_t display_id) override {
    if (shown == target_shown_) {
      std::move(closure_).Run();
      delete this;
    }
  }

  bool target_shown_;
  base::OnceClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(HomeLauncherStateWaiter);
};

// A waiter that waits until the animation ended with the target state, and
// execute the callback.  This self destruction upon completion.
class LauncherStateWaiter {
 public:
  LauncherStateWaiter(ash::AppListViewState state, base::OnceClosure closure)
      : target_state_(state), closure_(std::move(closure)) {
    Shell::Get()->app_list_controller()->SetStateTransitionAnimationCallback(
        base::BindRepeating(&LauncherStateWaiter::OnStateChanged,
                            base::Unretained(this)));
  }
  ~LauncherStateWaiter() {
    Shell::Get()->app_list_controller()->SetStateTransitionAnimationCallback(
        base::NullCallback());
  }

  void OnStateChanged(ash::AppListViewState state) {
    if (target_state_ == state) {
      std::move(closure_).Run();
      delete this;
    }
  }

 private:
  ash::AppListViewState target_state_;
  base::OnceClosure closure_;

  DISALLOW_COPY_AND_ASSIGN(LauncherStateWaiter);
};

}  // namespace

ShellTestApi::ShellTestApi() : shell_(Shell::Get()) {}
ShellTestApi::~ShellTestApi() = default;

// static
void ShellTestApi::SetTabletControllerUseScreenshotForTest(
    bool use_screenshot) {
  TabletModeController::SetUseScreenshotForTest(use_screenshot);
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

void ShellTestApi::ResetPowerButtonControllerForTest() {
  shell_->backlights_forced_off_setter_->ResetForTest();
  shell_->power_button_controller_ = std::make_unique<PowerButtonController>(
      shell_->backlights_forced_off_setter_.get());
}

void ShellTestApi::SimulateModalWindowOpenForTest(bool modal_window_open) {
  shell_->simulate_modal_window_open_for_test_ = modal_window_open;
}

bool ShellTestApi::IsSystemModalWindowOpen() {
  return Shell::IsSystemModalWindowOpen();
}

void ShellTestApi::SetTabletModeEnabledForTest(bool enable,
                                               bool wait_for_completion) {
  TabletMode::Waiter waiter(enable);
  shell_->tablet_mode_controller()->SetEnabledForTest(enable);
  waiter.Wait();
}

void ShellTestApi::EnableVirtualKeyboard() {
  shell_->keyboard_controller()->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kCommandLineEnabled);
}

void ShellTestApi::ToggleFullscreen() {
  ash::accelerators::ToggleFullscreen();
}

bool ShellTestApi::IsOverviewSelecting() {
  return shell_->overview_controller()->InOverviewSession();
}

void ShellTestApi::AddRemoveDisplay() {
  shell_->display_manager()->AddRemoveDisplay();
}

void ShellTestApi::WaitForNoPointerHoldLock() {
  aura::WindowTreeHost* primary_host =
      Shell::GetPrimaryRootWindowController()->GetHost();
  if (primary_host->holding_pointer_moves())
    PointerMoveLoopWaiter(primary_host).Wait();
}

void ShellTestApi::WaitForNextFrame(base::OnceClosure closure) {
  Shell::GetPrimaryRootWindowController()
      ->GetHost()
      ->compositor()
      ->RequestPresentationTimeForNextFrame(base::BindOnce(
          [](base::OnceClosure closure,
             const gfx::PresentationFeedback& feedback) {
            std::move(closure).Run();
          },
          std::move(closure)));
}

void ShellTestApi::WaitForOverviewAnimationState(OverviewAnimationState state) {
  auto* overview_controller = shell_->overview_controller();
  if (state == OverviewAnimationState::kEnterAnimationComplete &&
      overview_controller->InOverviewSession() &&
      !overview_controller->IsInStartAnimation()) {
    // If there is no animation applied, call the callback immediately.
    return;
  }
  if (state == OverviewAnimationState::kExitAnimationComplete &&
      !overview_controller->InOverviewSession() &&
      !overview_controller->IsCompletingShutdownAnimations()) {
    // If there is no animation applied, call the callback immediately.
    return;
  }
  base::RunLoop run_loop;
  new OverviewAnimationStateWaiter(
      state, base::Bind([](base::RunLoop* run_loop,
                           bool finished) { run_loop->QuitWhenIdle(); },
                        base::Unretained(&run_loop)));
  run_loop.Run();
}

void ShellTestApi::WaitForLauncherAnimationState(
    ash::AppListViewState target_state) {
  base::RunLoop run_loop;

  // In the tablet mode, some of the app-list state switching is handled
  // differently. For open and close, HomeLauncherGestureHandler handles the
  // gestures and animation. HomeLauncherStateWaiter can wait for such
  // animation. For switching between the search and apps-grid,
  // LauncherStateWaiter can wait for the animation.
  bool should_wait_for_home_launcher = false;
  if (Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      target_state != AppListViewState::kFullscreenSearch) {
    // App-list can't enter into kPeeking or kHalf state. Thus |target_state|
    // should be either kClosed or kFullscreenAllApps.
    DCHECK(target_state == AppListViewState::kClosed ||
           target_state == AppListViewState::kFullscreenAllApps);
    const AppListViewState current_state =
        Shell::Get()->app_list_controller()->GetAppListViewState();
    should_wait_for_home_launcher =
        (target_state == AppListViewState::kClosed) ||
        (current_state != AppListViewState::kFullscreenSearch);
  }
  if (should_wait_for_home_launcher) {
    new HomeLauncherStateWaiter(target_state != ash::AppListViewState::kClosed,
                                run_loop.QuitWhenIdleClosure());
  } else {
    new LauncherStateWaiter(target_state, run_loop.QuitWhenIdleClosure());
  }
  run_loop.Run();
}

PaginationModel* ShellTestApi::GetAppListPaginationModel() {
  app_list::AppListView* view =
      Shell::Get()->app_list_controller()->presenter()->GetView();
  if (!view)
    return nullptr;
  return view->GetAppsPaginationModel();
}

std::vector<aura::Window*> ShellTestApi::GetItemWindowListInOverviewGrids() {
  return ash::Shell::Get()
      ->overview_controller()
      ->GetItemWindowListInOverviewGridsForTest();
}

}  // namespace ash
