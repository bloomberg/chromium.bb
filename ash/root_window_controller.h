// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROOT_WINDOW_CONTROLLER_H_
#define ASH_ROOT_WINDOW_CONTROLLER_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell_observer.h"
#include "ash/system/user/login_status.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_types.h"

class SkBitmap;

namespace aura {
class EventFilter;
class Window;
}

namespace gfx {
class Point;
}

namespace keyboard {
class KeyboardController;
}

namespace ui {
class EventHandler;
}

namespace views {
class Widget;
}

namespace wm {
class InputMethodEventFilter;
class RootWindowEventFilter;
class ScopedCaptureClient;
}

namespace ash {
class AshWindowTreeHost;
class AlwaysOnTopController;
class AnimatingDesktopController;
class DesktopBackgroundWidgetController;
class DockedWindowLayoutManager;
class PanelLayoutManager;
class RootWindowLayoutManager;
class ShelfLayoutManager;
class ShelfWidget;
class StackingController;
class StatusAreaWidget;
class SystemBackgroundController;
class SystemModalContainerLayoutManager;
class SystemTray;
class TouchHudDebug;
class TouchHudProjection;
class WorkspaceController;

#if defined(OS_CHROMEOS)
class BootSplashScreen;
class AshTouchExplorationManager;
#endif

// This class maintains the per root window state for ash. This class
// owns the root window and other dependent objects that should be
// deleted upon the deletion of the root window. This object is
// indirectly owned and deleted by |WindowTreeHostManager|.
// The RootWindowController for particular root window is stored in
// its property (RootWindowSettings) and can be obtained using
// |GetRootWindowController(aura::WindowEventDispatcher*)| function.
//
// NOTE: In classic ash there is one RootWindow per display, so every RootWindow
// has a RootWindowController. In mus/mash there is one RootWindow per top-level
// Widget, so not all RootWindows have a RootWindowController.
class ASH_EXPORT RootWindowController : public ShellObserver {
 public:
  // Creates and Initialize the RootWindowController for primary display.
  static void CreateForPrimaryDisplay(AshWindowTreeHost* host);

  // Creates and Initialize the RootWindowController for secondary displays.
  static void CreateForSecondaryDisplay(AshWindowTreeHost* host);

  // Returns a RootWindowController of the window's root window.
  static RootWindowController* ForWindow(const aura::Window* window);

  // Returns the RootWindowController of the target root window.
  static RootWindowController* ForTargetRootWindow();

  ~RootWindowController() override;

  AshWindowTreeHost* ash_host() { return ash_host_.get(); }
  const AshWindowTreeHost* ash_host() const { return ash_host_.get(); }

  aura::WindowTreeHost* GetHost();
  const aura::WindowTreeHost* GetHost() const;
  aura::Window* GetRootWindow();
  const aura::Window* GetRootWindow() const;

  RootWindowLayoutManager* root_window_layout() { return root_window_layout_; }

  WorkspaceController* workspace_controller() {
    return workspace_controller_.get();
  }

  AlwaysOnTopController* always_on_top_controller() {
    return always_on_top_controller_.get();
  }

  // Access the shelf associated with this root window controller,
  // NULL if no such shelf exists.
  ShelfWidget* shelf() { return shelf_.get(); }

  // Get touch HUDs associated with this root window controller.
  TouchHudDebug* touch_hud_debug() const {
    return touch_hud_debug_;
  }
  TouchHudProjection* touch_hud_projection() const {
    return touch_hud_projection_;
  }

  // Set touch HUDs for this root window controller. The root window controller
  // will not own the HUDs; their lifetimes are managed by themselves. Whenever
  // the widget showing a HUD is being destroyed (e.g. because of detaching a
  // display), the HUD deletes itself.
  void set_touch_hud_debug(TouchHudDebug* hud) {
    touch_hud_debug_ = hud;
  }
  void set_touch_hud_projection(TouchHudProjection* hud) {
    touch_hud_projection_ = hud;
  }

  DesktopBackgroundWidgetController* wallpaper_controller() {
    return wallpaper_controller_.get();
  }
  void SetWallpaperController(DesktopBackgroundWidgetController* controller);
  AnimatingDesktopController* animating_wallpaper_controller() {
    return animating_wallpaper_controller_.get();
  }
  void SetAnimatingWallpaperController(AnimatingDesktopController* controller);

  // Access the shelf layout manager associated with this root
  // window controller, NULL if no such shelf exists.
  ShelfLayoutManager* GetShelfLayoutManager();

  // Returns the system tray on this root window. Note that
  // calling this on the root window that doesn't have a shelf will
  // lead to a crash.
  SystemTray* GetSystemTray();

  // Shows context menu at the |location_in_screen|. This uses
  // |ShellDelegate::CreateContextMenu| to define the content of the menu.
  void ShowContextMenu(const gfx::Point& location_in_screen,
                       ui::MenuSourceType source_type);

  // Returns the layout-manager for the appropriate modal-container. If the
  // window is inside the lockscreen modal container, then the layout manager
  // for that is returned. Otherwise the layout manager for the default modal
  // container is returned.
  // If no window is specified (i.e. |window| is NULL), then the lockscreen
  // modal container is used if the screen is currently locked. Otherwise, the
  // default modal container is used.
  SystemModalContainerLayoutManager* GetSystemModalLayoutManager(
      aura::Window* window);

  aura::Window* GetContainer(int container_id);
  const aura::Window* GetContainer(int container_id) const;

  // Show shelf view if it was created hidden (before session has started).
  void ShowShelf();

  // Called when the shelf associated with this root window is created.
  void OnShelfCreated();

  // Called when the login status changes after login (such as lock/unlock).
  // TODO(oshima): Investigate if we can merge this and |OnLoginStateChanged|.
  void UpdateAfterLoginStatusChange(user::LoginStatus status);

  // Called when the brightness/grayscale animation from white to the login
  // desktop background image has started.  Starts |boot_splash_screen_|'s
  // hiding animation (if the screen is non-NULL).
  void HandleInitialDesktopBackgroundAnimationStarted();

  // Called when the wallpaper ainmation is finished. Updates |background_|
  // to be black and drops |boot_splash_screen_| and moves the wallpaper
  // controller into the root window controller. |widget| holds the wallpaper
  // image, or NULL if the background is a solid color.
  void OnWallpaperAnimationFinished(views::Widget* widget);

  // Deletes associated objects and clears the state, but doesn't delete
  // the root window yet. This is used to delete a secondary displays'
  // root window safely when the display disconnect signal is received,
  // which may come while we're in the nested message loop.
  void Shutdown();

  // Deletes all child windows and performs necessary cleanup.
  void CloseChildWindows();

  // Moves child windows to |dest|.
  void MoveWindowsTo(aura::Window* dest);

  // Force the shelf to query for it's current visibility state.
  void UpdateShelfVisibility();

  // Initialize touch HUDs if necessary.
  void InitTouchHuds();

  // Returns the topmost window or one of its transient parents, if any of them
  // are in fullscreen mode.
  aura::Window* GetWindowForFullscreenMode();

  // Activate virtual keyboard on current root window controller.
  void ActivateKeyboard(keyboard::KeyboardController* keyboard_controller);

  // Deactivate virtual keyboard on current root window controller.
  void DeactivateKeyboard(keyboard::KeyboardController* keyboard_controller);

  // Tests if a window is associated with the virtual keyboard.
  bool IsVirtualKeyboardWindow(aura::Window* window);

 private:
  explicit RootWindowController(AshWindowTreeHost* host);
  enum RootWindowType {
    PRIMARY,
    SECONDARY
  };

  // Initializes the RootWindowController.  |is_primary| is true if
  // the controller is for primary display.  |first_run_after_boot| is
  // set to true only for primary root window after boot.
  void Init(RootWindowType root_window_type, bool first_run_after_boot);

  void InitLayoutManagers();

  // Initializes |system_background_| and possibly also |boot_splash_screen_|.
  // |is_first_run_after_boot| determines the background's initial color.
  void CreateSystemBackground(bool is_first_run_after_boot);

  // Creates each of the special window containers that holds windows of various
  // types in the shell UI.
  void CreateContainersInRootWindow(aura::Window* root_window);

  // Enables projection touch HUD.
  void EnableTouchHudProjection();

  // Disables projection touch HUD.
  void DisableTouchHudProjection();

  // Overridden from ShellObserver.
  void OnLoginStateChanged(user::LoginStatus status) override;
  void OnTouchHudProjectionToggled(bool enabled) override;

  std::unique_ptr<AshWindowTreeHost> ash_host_;
  RootWindowLayoutManager* root_window_layout_;

  std::unique_ptr<StackingController> stacking_controller_;

  // The shelf for managing the shelf and the status widget.
  std::unique_ptr<ShelfWidget> shelf_;

  // An invisible/empty window used as a event target for
  // |MouseCursorEventFilter| before a user logs in.
  // (crbug.com/266987)
  // Its container is |LockScreenBackgroundContainer| and
  // this must be deleted before the container is deleted.
  std::unique_ptr<aura::Window> mouse_event_target_;

  // Manages layout of docked windows. Owned by DockedContainer.
  DockedWindowLayoutManager* docked_layout_manager_;

  // Manages layout of panels. Owned by PanelContainer.
  PanelLayoutManager* panel_layout_manager_;

  std::unique_ptr<SystemBackgroundController> system_background_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<BootSplashScreen> boot_splash_screen_;
  // Responsible for initializing TouchExplorationController when spoken
  // feedback is on.
  std::unique_ptr<AshTouchExplorationManager> touch_exploration_manager_;
#endif

  std::unique_ptr<WorkspaceController> workspace_controller_;
  std::unique_ptr<AlwaysOnTopController> always_on_top_controller_;

  // Heads-up displays for touch events. These HUDs are not owned by the root
  // window controller and manage their own lifetimes.
  TouchHudDebug* touch_hud_debug_;
  TouchHudProjection* touch_hud_projection_;

  // Handles double clicks on the panel window header.
  std::unique_ptr<ui::EventHandler> panel_container_handler_;

  std::unique_ptr<DesktopBackgroundWidgetController> wallpaper_controller_;
  std::unique_ptr<AnimatingDesktopController> animating_wallpaper_controller_;
  std::unique_ptr<::wm::ScopedCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowController);
};


// Gets the RootWindowController for |root_window|.
ASH_EXPORT RootWindowController* GetRootWindowController(
    const aura::Window* root_window);

}  // namespace ash

#endif  // ASH_ROOT_WINDOW_CONTROLLER_H_
