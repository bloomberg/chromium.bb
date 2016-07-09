// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include <queue>
#include <vector>

#include "ash/aura/aura_layout_manager_adapter.h"
#include "ash/aura/wm_shelf_aura.h"
#include "ash/aura/wm_window_aura.h"
#include "ash/common/ash_constants.h"
#include "ash/common/ash_switches.h"
#include "ash/common/focus_cycler.h"
#include "ash/common/login_status.h"
#include "ash/common/root_window_controller_common.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/shelf_types.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/wm/always_on_top_controller.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/dock/docked_window_layout_manager.h"
#include "ash/common/wm/fullscreen_window_finder.h"
#include "ash/common/wm/panels/panel_layout_manager.h"
#include "ash/common/wm/root_window_layout_manager.h"
#include "ash/common/wm/switchable_windows.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/workspace/workspace_layout_manager.h"
#include "ash/common/wm/workspace/workspace_layout_manager_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_manager.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/root_window_settings.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_factory.h"
#include "ash/system/status_area_widget.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/touch/touch_hud_projection.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/wm/lock_layout_manager.h"
#include "ash/wm/panels/attached_panel_window_targeter.h"
#include "ash/wm/panels/panel_window_event_handler.h"
#include "ash/wm/stacking_controller.h"
#include "ash/wm/status_area_layout_manager.h"
#include "ash/wm/system_background_controller.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/hit_test.h"
#include "ui/base/models/menu_model.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/drag_drop_client.h"
#include "ui/wm/public/tooltip_client.h"
#include "ui/wm/public/window_types.h"

#if defined(OS_CHROMEOS)
#include "ash/ash_touch_exploration_manager_chromeos.h"
#include "ash/wm/boot_splash_screen_chromeos.h"
#include "ui/chromeos/touch_exploration_controller.h"
#endif

namespace ash {
namespace {

#if defined(OS_CHROMEOS)
// Duration for the animation that hides the boot splash screen, in
// milliseconds.  This should be short enough in relation to
// wm/window_animation.cc's brightness/grayscale fade animation that the login
// background image animation isn't hidden by the splash screen animation.
const int kBootSplashScreenHideDurationMs = 500;
#endif

float ToRelativeValue(int value, int src, int dst) {
  return static_cast<float>(value) / static_cast<float>(src) * dst;
}

void MoveOriginRelativeToSize(const gfx::Size& src_size,
                              const gfx::Size& dst_size,
                              gfx::Rect* bounds_in_out) {
  gfx::Point origin = bounds_in_out->origin();
  bounds_in_out->set_origin(gfx::Point(
      ToRelativeValue(origin.x(), src_size.width(), dst_size.width()),
      ToRelativeValue(origin.y(), src_size.height(), dst_size.height())));
}

// Reparents |window| to |new_parent|.
void ReparentWindow(aura::Window* window, aura::Window* new_parent) {
  const gfx::Size src_size = window->parent()->bounds().size();
  const gfx::Size dst_size = new_parent->bounds().size();
  // Update the restore bounds to make it relative to the display.
  wm::WindowState* state = wm::GetWindowState(window);
  gfx::Rect restore_bounds;
  bool has_restore_bounds = state->HasRestoreBounds();

  bool update_bounds = (state->IsNormalOrSnapped() || state->IsMinimized()) &&
                       new_parent->id() != kShellWindowId_DockedContainer;
  gfx::Rect local_bounds;
  if (update_bounds) {
    local_bounds = WmWindowAura::GetAuraWindow(state->window())->bounds();
    MoveOriginRelativeToSize(src_size, dst_size, &local_bounds);
  }

  if (has_restore_bounds) {
    restore_bounds = state->GetRestoreBoundsInParent();
    MoveOriginRelativeToSize(src_size, dst_size, &restore_bounds);
  }

  new_parent->AddChild(window);

  // Docked windows have bounds handled by the layout manager in AddChild().
  if (update_bounds)
    window->SetBounds(local_bounds);

  if (has_restore_bounds)
    state->SetRestoreBoundsInParent(restore_bounds);
}

// Reparents the appropriate set of windows from |src| to |dst|.
void ReparentAllWindows(aura::Window* src, aura::Window* dst) {
  // Set of windows to move.
  const int kContainerIdsToMove[] = {
      kShellWindowId_DefaultContainer,
      kShellWindowId_DockedContainer,
      kShellWindowId_PanelContainer,
      kShellWindowId_AlwaysOnTopContainer,
      kShellWindowId_SystemModalContainer,
      kShellWindowId_LockSystemModalContainer,
      kShellWindowId_UnparentedControlContainer,
      kShellWindowId_OverlayContainer,
  };
  const int kExtraContainerIdsToMoveInUnifiedMode[] = {
      kShellWindowId_LockScreenContainer,
      kShellWindowId_LockScreenBackgroundContainer,
  };
  std::vector<int> container_ids(
      kContainerIdsToMove,
      kContainerIdsToMove + arraysize(kContainerIdsToMove));
  // Check the default_multi_display_mode because this is also necessary
  // in trasition between mirror <-> unified mode.
  if (Shell::GetInstance()
          ->display_manager()
          ->current_default_multi_display_mode() == DisplayManager::UNIFIED) {
    for (int id : kExtraContainerIdsToMoveInUnifiedMode)
      container_ids.push_back(id);
  }

  for (int id : container_ids) {
    aura::Window* src_container = Shell::GetContainer(src, id);
    aura::Window* dst_container = Shell::GetContainer(dst, id);
    while (!src_container->children().empty()) {
      // Restart iteration from the source container windows each time as they
      // may change as a result of moving other windows.
      aura::Window::Windows::const_iterator iter =
          src_container->children().begin();
      while (iter != src_container->children().end() &&
             SystemModalContainerLayoutManager::IsModalBackground(*iter)) {
        ++iter;
      }
      // If the entire window list is modal background windows then stop.
      if (iter == src_container->children().end())
        break;
      ReparentWindow(*iter, dst_container);
    }
  }
}

bool IsWindowAboveContainer(aura::Window* window,
                            aura::Window* blocking_container) {
  std::vector<aura::Window*> target_path;
  std::vector<aura::Window*> blocking_path;

  while (window) {
    target_path.push_back(window);
    window = window->parent();
  }

  while (blocking_container) {
    blocking_path.push_back(blocking_container);
    blocking_container = blocking_container->parent();
  }

  // The root window is put at the end so that we compare windows at
  // the same depth.
  while (!blocking_path.empty()) {
    if (target_path.empty())
      return false;

    aura::Window* target = target_path.back();
    target_path.pop_back();
    aura::Window* blocking = blocking_path.back();
    blocking_path.pop_back();

    // Still on the same path, continue.
    if (target == blocking)
      continue;

    // This can happen only if unparented window is passed because
    // first element must be the same root.
    if (!target->parent() || !blocking->parent())
      return false;

    aura::Window* common_parent = target->parent();
    DCHECK_EQ(common_parent, blocking->parent());
    aura::Window::Windows windows = common_parent->children();
    auto blocking_iter = std::find(windows.begin(), windows.end(), blocking);
    // If the target window is above blocking window, the window can handle
    // events.
    return std::find(blocking_iter, windows.end(), target) != windows.end();
  }

  return true;
}

// A window delegate which does nothing. Used to create a window that
// is a event target, but do nothing.
class EmptyWindowDelegate : public aura::WindowDelegate {
 public:
  EmptyWindowDelegate() {}
  ~EmptyWindowDelegate() override {}

  // aura::WindowDelegate overrides:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::kNullCursor;
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTNOWHERE;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return false;
  }
  bool CanFocus() override { return false; }
  void OnCaptureLost() override {}
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override { delete this; }
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return false; }
  void GetHitTestMask(gfx::Path* mask) const override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyWindowDelegate);
};

class WorkspaceLayoutManagerDelegateImpl
    : public wm::WorkspaceLayoutManagerDelegate {
 public:
  explicit WorkspaceLayoutManagerDelegateImpl(aura::Window* root_window)
      : root_window_(root_window) {}
  ~WorkspaceLayoutManagerDelegateImpl() override = default;

  void set_shelf(ShelfLayoutManager* shelf) { shelf_ = shelf; }

  // WorkspaceLayoutManagerDelegate:
  void UpdateShelfVisibility() override {
    if (shelf_)
      shelf_->UpdateVisibilityState();
  }
  void OnFullscreenStateChanged(bool is_fullscreen) override {
    if (shelf_) {
      Shell::GetInstance()->NotifyFullscreenStateChange(
          is_fullscreen, WmWindowAura::Get(root_window_));
    }
  }

 private:
  aura::Window* root_window_;
  ShelfLayoutManager* shelf_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManagerDelegateImpl);
};

}  // namespace

void RootWindowController::CreateForPrimaryDisplay(AshWindowTreeHost* host) {
  RootWindowController* controller = new RootWindowController(host);
  controller->Init(RootWindowController::PRIMARY,
                   WmShell::Get()->delegate()->IsFirstRunAfterBoot());
}

void RootWindowController::CreateForSecondaryDisplay(AshWindowTreeHost* host) {
  RootWindowController* controller = new RootWindowController(host);
  controller->Init(RootWindowController::SECONDARY, false /* first run */);
}

// static
RootWindowController* RootWindowController::ForWindow(
    const aura::Window* window) {
  CHECK(Shell::HasInstance());
  return GetRootWindowController(window->GetRootWindow());
}

// static
RootWindowController* RootWindowController::ForTargetRootWindow() {
  CHECK(Shell::HasInstance());
  return GetRootWindowController(Shell::GetTargetRootWindow());
}

RootWindowController::~RootWindowController() {
  Shutdown();
  ash_host_.reset();
  // The CaptureClient needs to be around for as long as the RootWindow is
  // valid.
  capture_client_.reset();
}

aura::WindowTreeHost* RootWindowController::GetHost() {
  return ash_host_->AsWindowTreeHost();
}

const aura::WindowTreeHost* RootWindowController::GetHost() const {
  return ash_host_->AsWindowTreeHost();
}

aura::Window* RootWindowController::GetRootWindow() {
  return GetHost()->window();
}

const aura::Window* RootWindowController::GetRootWindow() const {
  return GetHost()->window();
}

void RootWindowController::SetWallpaperController(
    DesktopBackgroundWidgetController* controller) {
  wallpaper_controller_.reset(controller);
}

void RootWindowController::SetAnimatingWallpaperController(
    AnimatingDesktopController* controller) {
  if (animating_wallpaper_controller_.get())
    animating_wallpaper_controller_->StopAnimating();
  animating_wallpaper_controller_.reset(controller);
}

void RootWindowController::Shutdown() {
  WmShell::Get()->RemoveShellObserver(this);

#if defined(OS_CHROMEOS)
  if (touch_exploration_manager_) {
    touch_exploration_manager_.reset();
  }
#endif

  if (animating_wallpaper_controller_.get())
    animating_wallpaper_controller_->StopAnimating();
  wallpaper_controller_.reset();
  animating_wallpaper_controller_.reset();
  aura::Window* root_window = GetRootWindow();
  Shell* shell = Shell::GetInstance();
  // Change the target root window before closing child windows. If any child
  // being removed triggers a relayout of the shelf it will try to build a
  // window list adding windows from the target root window's containers which
  // may have already gone away.
  if (Shell::GetTargetRootWindow() == root_window) {
    shell->set_target_root_window(Shell::GetPrimaryRootWindow() == root_window
                                      ? NULL
                                      : Shell::GetPrimaryRootWindow());
  }

  CloseChildWindows();
  GetRootWindowSettings(root_window)->controller = NULL;
  workspace_controller_.reset();
  // Forget with the display ID so that display lookup
  // ends up with invalid display.
  GetRootWindowSettings(root_window)->display_id =
      display::Display::kInvalidDisplayID;
  ash_host_->PrepareForShutdown();

  system_background_.reset();
  aura::client::SetScreenPositionClient(root_window, NULL);
}

bool RootWindowController::CanWindowReceiveEvents(aura::Window* window) {
  if (GetRootWindow() != window->GetRootWindow())
    return false;

  // Always allow events to fall through to the virtual keyboard even if
  // displaying a system modal dialog.
  if (IsVirtualKeyboardWindow(window))
    return true;

  aura::Window* blocking_container = nullptr;

  int modal_container_id = 0;
  if (WmShell::Get()->GetSessionStateDelegate()->IsUserSessionBlocked()) {
    blocking_container =
        GetContainer(kShellWindowId_LockScreenContainersContainer);
    modal_container_id = kShellWindowId_LockSystemModalContainer;
  } else {
    modal_container_id = kShellWindowId_SystemModalContainer;
  }
  aura::Window* modal_container = GetContainer(modal_container_id);
  SystemModalContainerLayoutManager* modal_layout_manager = nullptr;
  modal_layout_manager = static_cast<SystemModalContainerLayoutManager*>(
      modal_container->layout_manager());

  if (modal_layout_manager->has_modal_background())
    blocking_container = modal_container;
  else
    modal_container = nullptr;  // Don't check modal dialogs.

  // In normal session.
  if (!blocking_container)
    return true;

  if (!IsWindowAboveContainer(window, blocking_container))
    return false;

  // If the window is in the target modal container, only allow the top most
  // one.
  if (modal_container && modal_container->Contains(window))
    return modal_layout_manager->IsPartOfActiveModalWindow(window);

  return true;
}

SystemModalContainerLayoutManager*
RootWindowController::GetSystemModalLayoutManager(aura::Window* window) {
  aura::Window* modal_container = NULL;
  if (window) {
    aura::Window* window_container = WmWindowAura::GetAuraWindow(
        wm::GetContainerForWindow(WmWindowAura::Get(window)));
    if (window_container &&
        window_container->id() >= kShellWindowId_LockScreenContainer) {
      modal_container = GetContainer(kShellWindowId_LockSystemModalContainer);
    } else {
      modal_container = GetContainer(kShellWindowId_SystemModalContainer);
    }
  } else {
    int modal_window_id =
        WmShell::Get()->GetSessionStateDelegate()->IsUserSessionBlocked()
            ? kShellWindowId_LockSystemModalContainer
            : kShellWindowId_SystemModalContainer;
    modal_container = GetContainer(modal_window_id);
  }
  return modal_container ? static_cast<SystemModalContainerLayoutManager*>(
                               modal_container->layout_manager())
                         : NULL;
}

aura::Window* RootWindowController::GetContainer(int container_id) {
  return GetRootWindow()->GetChildById(container_id);
}

const aura::Window* RootWindowController::GetContainer(int container_id) const {
  return ash_host_->AsWindowTreeHost()->window()->GetChildById(container_id);
}

void RootWindowController::ShowShelf() {
  if (!shelf_widget_->shelf())
    return;
  shelf_widget_->shelf()->SetVisible(true);
  shelf_widget_->status_area_widget()->Show();
}

void RootWindowController::CreateShelf() {
  if (shelf_widget_->shelf())
    return;
  shelf_widget_->CreateShelf(wm_shelf_aura_.get());

  if (panel_layout_manager_)
    panel_layout_manager_->SetShelf(wm_shelf_aura_.get());
  if (docked_layout_manager_) {
    docked_layout_manager_->SetShelf(wm_shelf_aura_.get());
    if (shelf_widget_->shelf_layout_manager())
      docked_layout_manager_->AddObserver(
          shelf_widget_->shelf_layout_manager());
  }

  // Notify shell observers that the shelf has been created.
  Shell::GetInstance()->OnShelfCreatedForRootWindow(
      WmWindowAura::Get(GetRootWindow()));

  shelf_widget_->PostCreateShelf();
}

Shelf* RootWindowController::GetShelf() const {
  // TODO(jamescook): Shelf should be owned by this class, not by ShelfWidget.
  return shelf_widget_->shelf();
}

void RootWindowController::UpdateAfterLoginStatusChange(LoginStatus status) {
  if (status != LoginStatus::NOT_LOGGED_IN)
    mouse_event_target_.reset();
  if (shelf_widget_->status_area_widget())
    shelf_widget_->status_area_widget()->UpdateAfterLoginStatusChange(status);
}

void RootWindowController::HandleInitialDesktopBackgroundAnimationStarted() {
#if defined(OS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshAnimateFromBootSplashScreen) &&
      boot_splash_screen_.get()) {
    // Make the splash screen fade out so it doesn't obscure the desktop
    // wallpaper's brightness/grayscale animation.
    boot_splash_screen_->StartHideAnimation(
        base::TimeDelta::FromMilliseconds(kBootSplashScreenHideDurationMs));
  }
#endif
}

void RootWindowController::OnWallpaperAnimationFinished(views::Widget* widget) {
  // Make sure the wallpaper is visible.
  system_background_->SetColor(SK_ColorBLACK);
#if defined(OS_CHROMEOS)
  boot_splash_screen_.reset();
#endif

  Shell::GetInstance()
      ->user_wallpaper_delegate()
      ->OnWallpaperAnimationFinished();
  // Only removes old component when wallpaper animation finished. If we
  // remove the old one before the new wallpaper is done fading in there will
  // be a white flash during the animation.
  if (animating_wallpaper_controller()) {
    DesktopBackgroundWidgetController* controller =
        animating_wallpaper_controller()->GetController(true);
    // |desktop_widget_| should be the same animating widget we try to move
    // to |kDesktopController|. Otherwise, we may close |desktop_widget_|
    // before move it to |kDesktopController|.
    DCHECK_EQ(controller->widget(), widget);
    // Release the old controller and close its background widget.
    SetWallpaperController(controller);
  }
}

void RootWindowController::CloseChildWindows() {
  mouse_event_target_.reset();

  // Remove observer as deactivating keyboard causes |docked_layout_manager_|
  // to fire notifications.
  if (docked_layout_manager_ && shelf_widget_ &&
      shelf_widget_->shelf_layout_manager())
    docked_layout_manager_->RemoveObserver(
        shelf_widget_->shelf_layout_manager());

  // Deactivate keyboard container before closing child windows and shutting
  // down associated layout managers.
  DeactivateKeyboard(keyboard::KeyboardController::GetInstance());

  // panel_layout_manager_ needs to be shut down before windows are destroyed.
  if (panel_layout_manager_) {
    panel_layout_manager_->Shutdown();
    panel_layout_manager_ = NULL;
  }
  // docked_layout_manager_ needs to be shut down before windows are destroyed.
  if (docked_layout_manager_) {
    docked_layout_manager_->Shutdown();
    docked_layout_manager_ = NULL;
  }
  aura::Window* root_window = GetRootWindow();
  aura::client::SetDragDropClient(root_window, NULL);

  if (shelf_widget_)
    shelf_widget_->Shutdown();

  wm_shelf_aura_->Shutdown();

  // Close background widget first as it depends on tooltip.
  wallpaper_controller_.reset();
  animating_wallpaper_controller_.reset();

  workspace_controller_.reset();
  aura::client::SetTooltipClient(root_window, NULL);

  // Explicitly destroy top level windows. We do this as during part of
  // destruction such windows may query the RootWindow for state.
  aura::WindowTracker non_toplevel_windows;
  non_toplevel_windows.Add(root_window);
  while (!non_toplevel_windows.windows().empty()) {
    const aura::Window* non_toplevel_window =
        *non_toplevel_windows.windows().begin();
    non_toplevel_windows.Remove(const_cast<aura::Window*>(non_toplevel_window));
    aura::WindowTracker toplevel_windows;
    for (size_t i = 0; i < non_toplevel_window->children().size(); ++i) {
      aura::Window* child = non_toplevel_window->children()[i];
      if (!child->owned_by_parent())
        continue;
      if (child->delegate())
        toplevel_windows.Add(child);
      else
        non_toplevel_windows.Add(child);
    }
    while (!toplevel_windows.windows().empty())
      delete *toplevel_windows.windows().begin();
  }
  // And then remove the containers.
  while (!root_window->children().empty()) {
    aura::Window* window = root_window->children()[0];
    if (window->owned_by_parent()) {
      delete window;
    } else {
      root_window->RemoveChild(window);
    }
  }

  shelf_widget_.reset();
}

void RootWindowController::MoveWindowsTo(aura::Window* dst) {
  // Forget the shelf early so that shelf don't update itself using wrong
  // display info.
  workspace_controller_->SetShelf(nullptr);
  workspace_controller_->layout_manager()->DeleteDelegate();
  ReparentAllWindows(GetRootWindow(), dst);
}

ShelfLayoutManager* RootWindowController::GetShelfLayoutManager() {
  return shelf_widget_->shelf_layout_manager();
}

SystemTray* RootWindowController::GetSystemTray() {
  // We assume in throughout the code that this will not return NULL. If code
  // triggers this for valid reasons, it should test status_area_widget first.
  CHECK(shelf_widget_->status_area_widget());
  return shelf_widget_->status_area_widget()->system_tray();
}

void RootWindowController::ShowContextMenu(const gfx::Point& location_in_screen,
                                           ui::MenuSourceType source_type) {
  ShellDelegate* delegate = WmShell::Get()->delegate();
  DCHECK(delegate);
  menu_model_.reset(delegate->CreateContextMenu(wm_shelf_aura_.get(), nullptr));
  if (!menu_model_)
    return;

  menu_model_adapter_.reset(new views::MenuModelAdapter(
      menu_model_.get(),
      base::Bind(&RootWindowController::OnMenuClosed, base::Unretained(this))));

  // Background controller may not be set yet if user clicked on status are
  // before initial animation completion. See crbug.com/222218
  if (!wallpaper_controller_.get())
    return;

  menu_runner_.reset(new views::MenuRunner(
      menu_model_adapter_->CreateMenu(),
      views::MenuRunner::CONTEXT_MENU | views::MenuRunner::ASYNC));
  ignore_result(
      menu_runner_->RunMenuAt(wallpaper_controller_->widget(), NULL,
                              gfx::Rect(location_in_screen, gfx::Size()),
                              views::MENU_ANCHOR_TOPLEFT, source_type));
}

void RootWindowController::UpdateShelfVisibility() {
  shelf_widget_->shelf_layout_manager()->UpdateVisibilityState();
}

aura::Window* RootWindowController::GetWindowForFullscreenMode() {
  return WmWindowAura::GetAuraWindow(
      wm::GetWindowForFullscreenMode(WmWindowAura::Get(GetRootWindow())));
}

void RootWindowController::ActivateKeyboard(
    keyboard::KeyboardController* keyboard_controller) {
  if (!keyboard::IsKeyboardEnabled() ||
      GetContainer(kShellWindowId_VirtualKeyboardContainer)) {
    return;
  }
  DCHECK(keyboard_controller);
  keyboard_controller->AddObserver(shelf_widget()->shelf_layout_manager());
  keyboard_controller->AddObserver(panel_layout_manager_);
  keyboard_controller->AddObserver(docked_layout_manager_);
  keyboard_controller->AddObserver(workspace_controller_->layout_manager());
  keyboard_controller->AddObserver(
      always_on_top_controller_->GetLayoutManager());
  WmShell::Get()->OnVirtualKeyboardActivated(true);
  aura::Window* parent = GetContainer(kShellWindowId_ImeWindowParentContainer);
  DCHECK(parent);
  aura::Window* keyboard_container = keyboard_controller->GetContainerWindow();
  keyboard_container->set_id(kShellWindowId_VirtualKeyboardContainer);
  parent->AddChild(keyboard_container);
}

void RootWindowController::DeactivateKeyboard(
    keyboard::KeyboardController* keyboard_controller) {
  if (!keyboard_controller ||
      !keyboard_controller->keyboard_container_initialized()) {
    return;
  }
  aura::Window* keyboard_container = keyboard_controller->GetContainerWindow();
  if (keyboard_container->GetRootWindow() == GetRootWindow()) {
    aura::Window* parent =
        GetContainer(kShellWindowId_ImeWindowParentContainer);
    DCHECK(parent);
    parent->RemoveChild(keyboard_container);
    // Virtual keyboard may be deactivated while still showing, notify all
    // observers that keyboard bounds changed to 0 before remove them.
    keyboard_controller->NotifyKeyboardBoundsChanging(gfx::Rect());
    keyboard_controller->RemoveObserver(shelf_widget()->shelf_layout_manager());
    keyboard_controller->RemoveObserver(panel_layout_manager_);
    keyboard_controller->RemoveObserver(docked_layout_manager_);
    keyboard_controller->RemoveObserver(
        workspace_controller_->layout_manager());
    keyboard_controller->RemoveObserver(
        always_on_top_controller_->GetLayoutManager());
    WmShell::Get()->OnVirtualKeyboardActivated(false);
  }
}

bool RootWindowController::IsVirtualKeyboardWindow(aura::Window* window) {
  aura::Window* parent = GetContainer(kShellWindowId_ImeWindowParentContainer);
  return parent ? parent->Contains(window) : false;
}

void RootWindowController::SetTouchAccessibilityAnchorPoint(
    const gfx::Point& anchor_point) {
#if defined(OS_CHROMEOS)
  if (touch_exploration_manager_)
    touch_exploration_manager_->SetTouchAccessibilityAnchorPoint(anchor_point);
#endif  // defined(OS_CHROMEOS)
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowController, private:

RootWindowController::RootWindowController(AshWindowTreeHost* ash_host)
    : ash_host_(ash_host),
      wm_shelf_aura_(new WmShelfAura),
      docked_layout_manager_(NULL),
      panel_layout_manager_(NULL),
      touch_hud_debug_(NULL),
      touch_hud_projection_(NULL) {
  aura::Window* root_window = GetRootWindow();
  root_window_controller_common_.reset(
      new RootWindowControllerCommon(WmWindowAura::Get(root_window)));
  GetRootWindowSettings(root_window)->controller = this;

  stacking_controller_.reset(new StackingController);
  aura::client::SetWindowTreeClient(root_window, stacking_controller_.get());
  capture_client_.reset(new ::wm::ScopedCaptureClient(root_window));
}

void RootWindowController::Init(RootWindowType root_window_type,
                                bool first_run_after_boot) {
  aura::Window* root_window = GetRootWindow();
  Shell* shell = Shell::GetInstance();
  shell->InitRootWindow(root_window);

  root_window_controller_common_->CreateContainers();

  CreateSystemBackground(first_run_after_boot);

  InitLayoutManagers();
  InitTouchHuds();

  if (Shell::GetPrimaryRootWindowController()
          ->GetSystemModalLayoutManager(NULL)
          ->has_modal_background()) {
    GetSystemModalLayoutManager(NULL)->CreateModalBackground();
  }

  WmShell::Get()->AddShellObserver(this);

  root_window_controller_common_->root_window_layout()->OnWindowResized();
  if (root_window_type == PRIMARY) {
    shell->InitKeyboard();
  } else {
    ash_host_->AsWindowTreeHost()->Show();

    // Create a shelf if a user is already logged in.
    if (WmShell::Get()->GetSessionStateDelegate()->NumberOfLoggedInUsers())
      CreateShelf();

    // Notify shell observers about new root window.
    shell->OnRootWindowAdded(WmWindowAura::Get(root_window));
  }

#if defined(OS_CHROMEOS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshDisableTouchExplorationMode)) {
    touch_exploration_manager_.reset(new AshTouchExplorationManager(this));
  }
#endif
}

void RootWindowController::InitLayoutManagers() {
  root_window_controller_common_->CreateLayoutManagers();

  aura::Window* root_window = GetRootWindow();

  aura::Window* default_container =
      GetContainer(kShellWindowId_DefaultContainer);
  // Workspace manager has its own layout managers.

  aura::Window* modal_container =
      root_window->GetChildById(kShellWindowId_SystemModalContainer);
  DCHECK(modal_container);
  modal_container->SetLayoutManager(
      new SystemModalContainerLayoutManager(modal_container));

  aura::Window* lock_container =
      root_window->GetChildById(kShellWindowId_LockScreenContainer);
  DCHECK(lock_container);
  lock_container->SetLayoutManager(new LockLayoutManager(lock_container));

  aura::Window* lock_modal_container =
      root_window->GetChildById(kShellWindowId_LockSystemModalContainer);
  DCHECK(lock_modal_container);
  lock_modal_container->SetLayoutManager(
      new SystemModalContainerLayoutManager(lock_modal_container));

  WorkspaceLayoutManagerDelegateImpl* workspace_layout_manager_delegate =
      new WorkspaceLayoutManagerDelegateImpl(root_window);
  workspace_controller_.reset(new WorkspaceController(
      default_container, base::WrapUnique(workspace_layout_manager_delegate)));

  WmWindow* always_on_top_container =
      WmWindowAura::Get(GetContainer(kShellWindowId_AlwaysOnTopContainer));
  always_on_top_controller_.reset(
      new AlwaysOnTopController(always_on_top_container));

  DCHECK(!shelf_widget_.get());
  WmWindow* shelf_container =
      WmWindowAura::Get(GetContainer(kShellWindowId_ShelfContainer));
  WmWindow* status_container =
      WmWindowAura::Get(GetContainer(kShellWindowId_StatusContainer));
  shelf_widget_.reset(new ShelfWidget(shelf_container, status_container,
                                      wm_shelf_aura_.get(),
                                      workspace_controller()));
  workspace_layout_manager_delegate->set_shelf(
      shelf_widget_->shelf_layout_manager());

  if (!WmShell::Get()
           ->GetSessionStateDelegate()
           ->IsActiveUserSessionStarted()) {
    // This window exists only to be a event target on login screen.
    // It does not have to handle events, nor be visible.
    mouse_event_target_.reset(new aura::Window(new EmptyWindowDelegate));
    mouse_event_target_->Init(ui::LAYER_NOT_DRAWN);

    aura::Window* lock_background_container =
        GetContainer(kShellWindowId_LockScreenBackgroundContainer);
    lock_background_container->AddChild(mouse_event_target_.get());
    mouse_event_target_->Show();
  }

  // Create Docked windows layout manager
  WmWindow* docked_container =
      WmWindowAura::Get(GetContainer(kShellWindowId_DockedContainer));
  docked_layout_manager_ = new DockedWindowLayoutManager(docked_container);
  docked_container->SetLayoutManager(base::WrapUnique(docked_layout_manager_));

  // Installs SnapLayoutManager to containers who set the
  // |kSnapsChildrenToPhysicalPixelBoundary| property.
  wm::InstallSnapLayoutManagerToContainers(root_window);

  // Create Panel layout manager
  aura::Window* panel_container = GetContainer(kShellWindowId_PanelContainer);
  WmWindow* wm_panel_container = WmWindowAura::Get(panel_container);
  panel_layout_manager_ = new PanelLayoutManager(wm_panel_container);
  wm_panel_container->SetLayoutManager(base::WrapUnique(panel_layout_manager_));
  panel_container_handler_.reset(new PanelWindowEventHandler);
  panel_container->AddPreTargetHandler(panel_container_handler_.get());

  // Install an AttachedPanelWindowTargeter on the panel container to make it
  // easier to correctly target shelf buttons with touch.
  gfx::Insets mouse_extend(-kResizeOutsideBoundsSize, -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize);
  gfx::Insets touch_extend =
      mouse_extend.Scale(kResizeOutsideBoundsScaleForTouch);
  panel_container->SetEventTargeter(
      std::unique_ptr<ui::EventTargeter>(new AttachedPanelWindowTargeter(
          panel_container, mouse_extend, touch_extend, panel_layout_manager_)));
}

void RootWindowController::InitTouchHuds() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshTouchHud))
    set_touch_hud_debug(new TouchHudDebug(GetRootWindow()));
  if (Shell::GetInstance()->is_touch_hud_projection_enabled())
    EnableTouchHudProjection();
}

void RootWindowController::CreateSystemBackground(
    bool is_first_run_after_boot) {
  SkColor color = SK_ColorBLACK;
#if defined(OS_CHROMEOS)
  if (is_first_run_after_boot)
    color = kChromeOsBootColor;
#endif
  system_background_.reset(
      new SystemBackgroundController(GetRootWindow(), color));

#if defined(OS_CHROMEOS)
  // Make a copy of the system's boot splash screen so we can composite it
  // onscreen until the desktop background is ready.
  if (is_first_run_after_boot &&
      (base::CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kAshCopyHostBackgroundAtBoot) ||
       base::CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kAshAnimateFromBootSplashScreen)))
    boot_splash_screen_.reset(new BootSplashScreen(GetHost()));
#endif
}

void RootWindowController::EnableTouchHudProjection() {
  if (touch_hud_projection_)
    return;
  set_touch_hud_projection(new TouchHudProjection(GetRootWindow()));
}

void RootWindowController::DisableTouchHudProjection() {
  if (!touch_hud_projection_)
    return;
  touch_hud_projection_->Remove();
}

void RootWindowController::OnMenuClosed() {
  menu_runner_.reset();
  menu_model_adapter_.reset();
  menu_model_.reset();
  Shell::GetInstance()->UpdateShelfVisibility();
}

void RootWindowController::OnLoginStateChanged(LoginStatus status) {
  shelf_widget_->shelf_layout_manager()->UpdateVisibilityState();
}

void RootWindowController::OnTouchHudProjectionToggled(bool enabled) {
  if (enabled)
    EnableTouchHudProjection();
  else
    DisableTouchHudProjection();
}

RootWindowController* GetRootWindowController(const aura::Window* root_window) {
  if (!root_window)
    return nullptr;

  if (Shell::GetInstance()->in_mus()) {
    // On mus, like desktop aura, each top-level widget has its own root window,
    // so |root_window| is not necessarily the display's root. For now, just
    // the use the primary display root.
    // TODO(jamescook): Multi-display support. This depends on how mus windows
    // will be owned by displays.
    aura::Window* primary_root_window = Shell::GetInstance()
                                            ->window_tree_host_manager()
                                            ->GetPrimaryRootWindow();
    return GetRootWindowSettings(primary_root_window)->controller;
  }

  return GetRootWindowSettings(root_window)->controller;
}

}  // namespace ash
