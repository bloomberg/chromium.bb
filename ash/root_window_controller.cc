// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/root_window_controller.h"

#include <queue>
#include <vector>

#include "ash/ash_constants.h"
#include "ash/ash_switches.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_widget_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_manager.h"
#include "ash/focus_cycler.h"
#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/root_window_settings.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/switchable_windows.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/touch/touch_hud_projection.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/aura/aura_layout_manager_adapter.h"
#include "ash/wm/aura/wm_shelf_aura.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/dock/docked_window_layout_manager.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/common/workspace/workspace_layout_manager_delegate.h"
#include "ash/wm/lock_layout_manager.h"
#include "ash/wm/panels/attached_panel_window_targeter.h"
#include "ash/wm/panels/panel_layout_manager.h"
#include "ash/wm/panels/panel_window_event_handler.h"
#include "ash/wm/root_window_layout_manager.h"
#include "ash/wm/stacking_controller.h"
#include "ash/wm/status_area_layout_manager.h"
#include "ash/wm/system_background_controller.h"
#include "ash/wm/system_modal_container_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
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
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view_model.h"
#include "ui/views/view_model_utils.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/easy_resize_window_targeter.h"
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

// Creates a new window for use as a container.
aura::Window* CreateContainer(int window_id,
                              const char* name,
                              aura::Window* parent) {
  aura::Window* container = new aura::Window(NULL);
  container->set_id(window_id);
  container->SetName(name);
  container->Init(ui::LAYER_NOT_DRAWN);
  parent->AddChild(container);
  if (window_id != kShellWindowId_UnparentedControlContainer)
    container->Show();
  return container;
}

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
    local_bounds = wm::WmWindowAura::GetAuraWindow(state->window())->bounds();
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

// Mark the container window so that a widget added to this container will
// use the virtual screeen coordinates instead of parent.
void SetUsesScreenCoordinates(aura::Window* container) {
  container->SetProperty(kUsesScreenCoordinatesKey, true);
}

// Mark the container window so that a widget added to this container will
// say in the same root window regardless of the bounds specified.
void DescendantShouldStayInSameRootWindow(aura::Window* container) {
  container->SetProperty(kStayInSameRootWindowKey, true);
}

void SetUsesEasyResizeTargeter(aura::Window* container) {
  gfx::Insets mouse_extend(-kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize);
  gfx::Insets touch_extend = mouse_extend.Scale(
      kResizeOutsideBoundsScaleForTouch);
  container->SetEventTargeter(
      std::unique_ptr<ui::EventTargeter>(new ::wm::EasyResizeWindowTargeter(
          container, mouse_extend, touch_extend)));
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
      ash::Shell::GetInstance()->NotifyFullscreenStateChange(is_fullscreen,
                                                             root_window_);
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
                   Shell::GetInstance()->delegate()->IsFirstRunAfterBoot());
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

// static
aura::Window* RootWindowController::GetContainerForWindow(
    aura::Window* window) {
  aura::Window* container = window->parent();
  while (container && container->type() != ui::wm::WINDOW_TYPE_UNKNOWN)
    container = container->parent();
  return container;
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
  Shell* shell = Shell::GetInstance();
  shell->RemoveShellObserver(this);

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
  // Change the target root window before closing child windows. If any child
  // being removed triggers a relayout of the shelf it will try to build a
  // window list adding windows from the target root window's containers which
  // may have already gone away.
  if (Shell::GetTargetRootWindow() == root_window) {
    shell->set_target_root_window(
        Shell::GetPrimaryRootWindow() == root_window
            ? NULL
            : Shell::GetPrimaryRootWindow());
  }

  CloseChildWindows();
  GetRootWindowSettings(root_window)->controller = NULL;
  workspace_controller_.reset();
  // Forget with the display ID so that display lookup
  // ends up with invalid display.
  GetRootWindowSettings(root_window)->display_id =
      gfx::Display::kInvalidDisplayID;
  ash_host_->PrepareForShutdown();

  system_background_.reset();
  aura::client::SetScreenPositionClient(root_window, NULL);
}

SystemModalContainerLayoutManager*
RootWindowController::GetSystemModalLayoutManager(aura::Window* window) {
  aura::Window* modal_container = NULL;
  if (window) {
    aura::Window* window_container = GetContainerForWindow(window);
    if (window_container &&
        window_container->id() >= kShellWindowId_LockScreenContainer) {
      modal_container = GetContainer(kShellWindowId_LockSystemModalContainer);
    } else {
      modal_container = GetContainer(kShellWindowId_SystemModalContainer);
    }
  } else {
    int modal_window_id = Shell::GetInstance()->session_state_delegate()
        ->IsUserSessionBlocked() ? kShellWindowId_LockSystemModalContainer :
                                   kShellWindowId_SystemModalContainer;
    modal_container = GetContainer(modal_window_id);
  }
  return modal_container ? static_cast<SystemModalContainerLayoutManager*>(
      modal_container->layout_manager()) : NULL;
}

aura::Window* RootWindowController::GetContainer(int container_id) {
  return GetRootWindow()->GetChildById(container_id);
}

const aura::Window* RootWindowController::GetContainer(int container_id) const {
  return ash_host_->AsWindowTreeHost()->window()->GetChildById(container_id);
}

void RootWindowController::ShowShelf() {
  if (!shelf_->shelf())
    return;
  shelf_->shelf()->SetVisible(true);
  shelf_->status_area_widget()->Show();
}

void RootWindowController::OnShelfCreated() {
  if (panel_layout_manager_)
    panel_layout_manager_->SetShelf(shelf_->shelf());
  if (docked_layout_manager_) {
    docked_layout_manager_->SetShelf(shelf_->shelf()->wm_shelf());
    if (shelf_->shelf_layout_manager())
      docked_layout_manager_->AddObserver(shelf_->shelf_layout_manager());
  }

  // Notify shell observers that the shelf has been created.
  Shell::GetInstance()->OnShelfCreatedForRootWindow(GetRootWindow());
}

void RootWindowController::UpdateAfterLoginStatusChange(
    user::LoginStatus status) {
  if (status != user::LOGGED_IN_NONE)
    mouse_event_target_.reset();
  if (shelf_->status_area_widget())
    shelf_->status_area_widget()->UpdateAfterLoginStatusChange(status);
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

  Shell::GetInstance()->user_wallpaper_delegate()->
      OnWallpaperAnimationFinished();
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
  if (docked_layout_manager_ && shelf_ && shelf_->shelf_layout_manager())
    docked_layout_manager_->RemoveObserver(shelf_->shelf_layout_manager());

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

  // TODO(harrym): Remove when Status Area Widget is a child view.
  if (shelf_) {
    shelf_->ShutdownStatusAreaWidget();

    if (shelf_->shelf_layout_manager())
      shelf_->shelf_layout_manager()->PrepareForShutdown();
  }

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

  shelf_.reset();
}

void RootWindowController::MoveWindowsTo(aura::Window* dst) {
  // Forget the shelf early so that shelf don't update itself using wrong
  // display info.
  workspace_controller_->SetShelf(nullptr);
  workspace_controller_->layout_manager()->DeleteDelegate();
  ReparentAllWindows(GetRootWindow(), dst);
}

ShelfLayoutManager* RootWindowController::GetShelfLayoutManager() {
  return shelf_->shelf_layout_manager();
}

SystemTray* RootWindowController::GetSystemTray() {
  // We assume in throughout the code that this will not return NULL. If code
  // triggers this for valid reasons, it should test status_area_widget first.
  CHECK(shelf_->status_area_widget());
  return shelf_->status_area_widget()->system_tray();
}

void RootWindowController::ShowContextMenu(const gfx::Point& location_in_screen,
                                           ui::MenuSourceType source_type) {
  ShellDelegate* delegate = Shell::GetInstance()->delegate();
  DCHECK(delegate);
  std::unique_ptr<ui::MenuModel> menu_model(
      delegate->CreateContextMenu(shelf_->shelf(), nullptr));
  if (!menu_model)
    return;

  // Background controller may not be set yet if user clicked on status are
  // before initial animation completion. See crbug.com/222218
  if (!wallpaper_controller_.get())
    return;

  views::MenuRunner menu_runner(menu_model.get(),
                                views::MenuRunner::CONTEXT_MENU);
  if (menu_runner.RunMenuAt(wallpaper_controller_->widget(),
                            NULL,
                            gfx::Rect(location_in_screen, gfx::Size()),
                            views::MENU_ANCHOR_TOPLEFT,
                            source_type) == views::MenuRunner::MENU_DELETED) {
    return;
  }

  Shell::GetInstance()->UpdateShelfVisibility();
}

void RootWindowController::UpdateShelfVisibility() {
  shelf_->shelf_layout_manager()->UpdateVisibilityState();
}

aura::Window* RootWindowController::GetWindowForFullscreenMode() {
  aura::Window* topmost_window = NULL;
  aura::Window* active_window = wm::GetActiveWindow();
  if (active_window && active_window->GetRootWindow() == GetRootWindow() &&
      IsSwitchableContainer(active_window->parent())) {
    // Use the active window when it is on the current root window to determine
    // the fullscreen state to allow temporarily using a panel or docked window
    // (which are always above the default container) while a fullscreen
    // window is open. We only use the active window when in a switchable
    // container as the launcher should not exit fullscreen mode.
    topmost_window = active_window;
  } else {
    // Otherwise, use the topmost window on the root window's default container
    // when there is no active window on this root window.
    const aura::Window::Windows& windows =
        GetContainer(kShellWindowId_DefaultContainer)->children();
    for (aura::Window::Windows::const_reverse_iterator iter = windows.rbegin();
         iter != windows.rend(); ++iter) {
      if (wm::IsWindowUserPositionable(*iter) &&
          (*iter)->layer()->GetTargetVisibility()) {
        topmost_window = *iter;
        break;
      }
    }
  }
  while (topmost_window) {
    if (wm::GetWindowState(topmost_window)->IsFullscreen())
      return topmost_window;
    topmost_window = ::wm::GetTransientParent(topmost_window);
  }
  return NULL;
}

void RootWindowController::ActivateKeyboard(
    keyboard::KeyboardController* keyboard_controller) {
  if (!keyboard::IsKeyboardEnabled() ||
      GetContainer(kShellWindowId_VirtualKeyboardContainer)) {
    return;
  }
  DCHECK(keyboard_controller);
  keyboard_controller->AddObserver(shelf()->shelf_layout_manager());
  keyboard_controller->AddObserver(panel_layout_manager_);
  keyboard_controller->AddObserver(docked_layout_manager_);
  keyboard_controller->AddObserver(workspace_controller_->layout_manager());
  keyboard_controller->AddObserver(
      always_on_top_controller_->GetLayoutManager());
  Shell::GetInstance()->delegate()->VirtualKeyboardActivated(true);
  aura::Window* parent = GetContainer(kShellWindowId_ImeWindowParentContainer);
  DCHECK(parent);
  aura::Window* keyboard_container =
      keyboard_controller->GetContainerWindow();
  keyboard_container->set_id(kShellWindowId_VirtualKeyboardContainer);
  parent->AddChild(keyboard_container);
}

void RootWindowController::DeactivateKeyboard(
    keyboard::KeyboardController* keyboard_controller) {
  if (!keyboard_controller ||
      !keyboard_controller->keyboard_container_initialized()) {
    return;
  }
  aura::Window* keyboard_container =
      keyboard_controller->GetContainerWindow();
  if (keyboard_container->GetRootWindow() == GetRootWindow()) {
    aura::Window* parent =
        GetContainer(kShellWindowId_ImeWindowParentContainer);
    DCHECK(parent);
    parent->RemoveChild(keyboard_container);
    // Virtual keyboard may be deactivated while still showing, notify all
    // observers that keyboard bounds changed to 0 before remove them.
    keyboard_controller->NotifyKeyboardBoundsChanging(gfx::Rect());
    keyboard_controller->RemoveObserver(shelf()->shelf_layout_manager());
    keyboard_controller->RemoveObserver(panel_layout_manager_);
    keyboard_controller->RemoveObserver(docked_layout_manager_);
    keyboard_controller->RemoveObserver(
        workspace_controller_->layout_manager());
    keyboard_controller->RemoveObserver(
        always_on_top_controller_->GetLayoutManager());
    Shell::GetInstance()->delegate()->VirtualKeyboardActivated(false);
  }
}

bool RootWindowController::IsVirtualKeyboardWindow(aura::Window* window) {
  aura::Window* parent = GetContainer(kShellWindowId_ImeWindowParentContainer);
  return parent ? parent->Contains(window) : false;
}

////////////////////////////////////////////////////////////////////////////////
// RootWindowController, private:

RootWindowController::RootWindowController(AshWindowTreeHost* ash_host)
    : ash_host_(ash_host),
      root_window_layout_(NULL),
      docked_layout_manager_(NULL),
      panel_layout_manager_(NULL),
      touch_hud_debug_(NULL),
      touch_hud_projection_(NULL) {
  aura::Window* root_window = GetRootWindow();
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

  CreateContainersInRootWindow(root_window);

  CreateSystemBackground(first_run_after_boot);

  InitLayoutManagers();
  InitTouchHuds();

  if (Shell::GetPrimaryRootWindowController()->
      GetSystemModalLayoutManager(NULL)->has_modal_background()) {
    GetSystemModalLayoutManager(NULL)->CreateModalBackground();
  }

  shell->AddShellObserver(this);

  if (root_window_type == PRIMARY) {
    root_window_layout()->OnWindowResized();
    shell->InitKeyboard();
  } else {
    root_window_layout()->OnWindowResized();
    ash_host_->AsWindowTreeHost()->Show();

    // Create a shelf if a user is already logged in.
    if (shell->session_state_delegate()->NumberOfLoggedInUsers())
      shelf()->CreateShelf();

    // Notify shell observers about new root window.
    shell->OnRootWindowAdded(root_window);
  }

#if defined(OS_CHROMEOS)
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshDisableTouchExplorationMode)) {
    touch_exploration_manager_.reset(new AshTouchExplorationManager(this));
  }
#endif
}

void RootWindowController::InitLayoutManagers() {
  aura::Window* root_window = GetRootWindow();
  root_window_layout_ = new RootWindowLayoutManager(root_window);
  root_window->SetLayoutManager(root_window_layout_);

  aura::Window* default_container =
      GetContainer(kShellWindowId_DefaultContainer);
  // Workspace manager has its own layout managers.

  WorkspaceLayoutManagerDelegateImpl* workspace_layout_manager_delegate =
      new WorkspaceLayoutManagerDelegateImpl(root_window);
  workspace_controller_.reset(new WorkspaceController(
      default_container, base::WrapUnique(workspace_layout_manager_delegate)));

  aura::Window* always_on_top_container =
      GetContainer(kShellWindowId_AlwaysOnTopContainer);
  always_on_top_controller_.reset(
      new AlwaysOnTopController(always_on_top_container));

  DCHECK(!shelf_.get());
  aura::Window* shelf_container = GetContainer(kShellWindowId_ShelfContainer);
  // TODO(harrym): Remove when status area is view.
  aura::Window* status_container = GetContainer(kShellWindowId_StatusContainer);
  shelf_.reset(new ShelfWidget(
      shelf_container, status_container, workspace_controller()));
  workspace_layout_manager_delegate->set_shelf(shelf_->shelf_layout_manager());

  if (!Shell::GetInstance()->session_state_delegate()->
      IsActiveUserSessionStarted()) {
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
  wm::WmWindow* docked_container =
      wm::WmWindowAura::Get(GetContainer(kShellWindowId_DockedContainer));
  docked_layout_manager_ = new DockedWindowLayoutManager(docked_container);
  docked_container->SetLayoutManager(base::WrapUnique(docked_layout_manager_));

  // Installs SnapLayoutManager to containers who set the
  // |kSnapsChildrenToPhysicalPixelBoundary| property.
  wm::InstallSnapLayoutManagerToContainers(root_window);

  // Create Panel layout manager
  aura::Window* panel_container = GetContainer(kShellWindowId_PanelContainer);
  wm::WmWindow* wm_panel_container = wm::WmWindowAura::Get(panel_container);
  panel_layout_manager_ = new PanelLayoutManager(wm_panel_container);
  wm_panel_container->SetLayoutManager(base::WrapUnique(panel_layout_manager_));
  panel_container_handler_.reset(new PanelWindowEventHandler);
  panel_container->AddPreTargetHandler(panel_container_handler_.get());

  // Install an AttachedPanelWindowTargeter on the panel container to make it
  // easier to correctly target shelf buttons with touch.
  gfx::Insets mouse_extend(-kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize);
  gfx::Insets touch_extend = mouse_extend.Scale(
      kResizeOutsideBoundsScaleForTouch);
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

void RootWindowController::CreateContainersInRootWindow(
    aura::Window* root_window) {
  // These containers are just used by PowerButtonController to animate groups
  // of containers simultaneously without messing up the current transformations
  // on those containers. These are direct children of the root window; all of
  // the other containers are their children.

  // The desktop background container is not part of the lock animation, so it
  // is not included in those animate groups.
  // When screen is locked desktop background is moved to lock screen background
  // container (moved back on unlock). We want to make sure that there's an
  // opaque layer occluding the non-lock-screen layers.
  aura::Window* desktop_background_container = CreateContainer(
      kShellWindowId_DesktopBackgroundContainer,
      "DesktopBackgroundContainer",
      root_window);
  ::wm::SetChildWindowVisibilityChangesAnimated(desktop_background_container);

  aura::Window* non_lock_screen_containers = CreateContainer(
      kShellWindowId_NonLockScreenContainersContainer,
      "NonLockScreenContainersContainer",
      root_window);
  // Clip all windows inside this container, as half pixel of the window's
  // texture may become visible when the screen is scaled. crbug.com/368591.
  non_lock_screen_containers->layer()->SetMasksToBounds(true);

  aura::Window* lock_background_containers = CreateContainer(
      kShellWindowId_LockScreenBackgroundContainer,
      "LockScreenBackgroundContainer",
      root_window);
  ::wm::SetChildWindowVisibilityChangesAnimated(lock_background_containers);

  aura::Window* lock_screen_containers = CreateContainer(
      kShellWindowId_LockScreenContainersContainer,
      "LockScreenContainersContainer",
      root_window);
  aura::Window* lock_screen_related_containers = CreateContainer(
      kShellWindowId_LockScreenRelatedContainersContainer,
      "LockScreenRelatedContainersContainer",
      root_window);

  CreateContainer(kShellWindowId_UnparentedControlContainer,
                  "UnparentedControlContainer",
                  non_lock_screen_containers);

  aura::Window* default_container = CreateContainer(
      kShellWindowId_DefaultContainer,
      "DefaultContainer",
      non_lock_screen_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(default_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(default_container);
  SetUsesScreenCoordinates(default_container);
  SetUsesEasyResizeTargeter(default_container);

  aura::Window* always_on_top_container = CreateContainer(
      kShellWindowId_AlwaysOnTopContainer,
      "AlwaysOnTopContainer",
      non_lock_screen_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(always_on_top_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(always_on_top_container);
  SetUsesScreenCoordinates(always_on_top_container);

  aura::Window* docked_container = CreateContainer(
      kShellWindowId_DockedContainer,
      "DockedContainer",
      non_lock_screen_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(docked_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(docked_container);
  SetUsesScreenCoordinates(docked_container);
  SetUsesEasyResizeTargeter(docked_container);

  aura::Window* shelf_container =
      CreateContainer(kShellWindowId_ShelfContainer,
                      "ShelfContainer",
                      non_lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(shelf_container);
  SetUsesScreenCoordinates(shelf_container);
  DescendantShouldStayInSameRootWindow(shelf_container);

  aura::Window* panel_container = CreateContainer(
      kShellWindowId_PanelContainer,
      "PanelContainer",
      non_lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(panel_container);
  SetUsesScreenCoordinates(panel_container);

  aura::Window* shelf_bubble_container =
      CreateContainer(kShellWindowId_ShelfBubbleContainer,
                      "ShelfBubbleContainer",
                      non_lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(shelf_bubble_container);
  SetUsesScreenCoordinates(shelf_bubble_container);
  DescendantShouldStayInSameRootWindow(shelf_bubble_container);

  aura::Window* app_list_container =
      CreateContainer(kShellWindowId_AppListContainer,
                      "AppListContainer",
                      non_lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(app_list_container);
  SetUsesScreenCoordinates(app_list_container);

  aura::Window* modal_container = CreateContainer(
      kShellWindowId_SystemModalContainer,
      "SystemModalContainer",
      non_lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(modal_container);
  modal_container->SetLayoutManager(
      new SystemModalContainerLayoutManager(modal_container));
  ::wm::SetChildWindowVisibilityChangesAnimated(modal_container);
  SetUsesScreenCoordinates(modal_container);
  SetUsesEasyResizeTargeter(modal_container);

  // TODO(beng): Figure out if we can make this use
  // SystemModalContainerEventFilter instead of stops_event_propagation.
  aura::Window* lock_container = CreateContainer(
      kShellWindowId_LockScreenContainer,
      "LockScreenContainer",
      lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(lock_container);
  lock_container->SetLayoutManager(new LockLayoutManager(lock_container));
  SetUsesScreenCoordinates(lock_container);
  // TODO(beng): stopsevents

  aura::Window* lock_modal_container = CreateContainer(
      kShellWindowId_LockSystemModalContainer,
      "LockSystemModalContainer",
      lock_screen_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(lock_modal_container);
  lock_modal_container->SetLayoutManager(
      new SystemModalContainerLayoutManager(lock_modal_container));
  ::wm::SetChildWindowVisibilityChangesAnimated(lock_modal_container);
  SetUsesScreenCoordinates(lock_modal_container);
  SetUsesEasyResizeTargeter(lock_modal_container);

  aura::Window* status_container =
      CreateContainer(kShellWindowId_StatusContainer,
                      "StatusContainer",
                      lock_screen_related_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(status_container);
  SetUsesScreenCoordinates(status_container);
  DescendantShouldStayInSameRootWindow(status_container);

  aura::Window* settings_bubble_container = CreateContainer(
      kShellWindowId_SettingBubbleContainer,
      "SettingBubbleContainer",
      lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(settings_bubble_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(settings_bubble_container);
  SetUsesScreenCoordinates(settings_bubble_container);
  DescendantShouldStayInSameRootWindow(settings_bubble_container);

  aura::Window* virtual_keyboard_parent_container =
      CreateContainer(kShellWindowId_ImeWindowParentContainer,
                      "VirtualKeyboardParentContainer",
                      lock_screen_related_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(
      virtual_keyboard_parent_container);
  SetUsesScreenCoordinates(virtual_keyboard_parent_container);

  aura::Window* menu_container = CreateContainer(
      kShellWindowId_MenuContainer,
      "MenuContainer",
      lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(menu_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(menu_container);
  SetUsesScreenCoordinates(menu_container);

  aura::Window* drag_drop_container = CreateContainer(
      kShellWindowId_DragImageAndTooltipContainer,
      "DragImageAndTooltipContainer",
      lock_screen_related_containers);
  ::wm::SetChildWindowVisibilityChangesAnimated(drag_drop_container);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(drag_drop_container);
  SetUsesScreenCoordinates(drag_drop_container);

  aura::Window* overlay_container = CreateContainer(
      kShellWindowId_OverlayContainer,
      "OverlayContainer",
      lock_screen_related_containers);
  wm::SetSnapsChildrenToPhysicalPixelBoundary(overlay_container);
  SetUsesScreenCoordinates(overlay_container);

#if defined(OS_CHROMEOS)
  aura::Window* mouse_cursor_container = CreateContainer(
      kShellWindowId_MouseCursorContainer,
      "MouseCursorContainer",
      root_window);
  SetUsesScreenCoordinates(mouse_cursor_container);
#endif

  CreateContainer(kShellWindowId_PowerButtonAnimationContainer,
                  "PowerButtonAnimationContainer", root_window);
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

void RootWindowController::OnLoginStateChanged(user::LoginStatus status) {
  shelf_->shelf_layout_manager()->UpdateVisibilityState();
}

void RootWindowController::OnTouchHudProjectionToggled(bool enabled) {
  if (enabled)
    EnableTouchHudProjection();
  else
    DisableTouchHudProjection();
}

RootWindowController* GetRootWindowController(
    const aura::Window* root_window) {
  return root_window ? GetRootWindowSettings(root_window)->controller : NULL;
}

}  // namespace ash
