// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ROOT_WINDOW_CONTROLLER_H_
#define ASH_ROOT_WINDOW_CONTROLLER_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/common/shell_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace aura {
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
class WindowTreeHost;
}

namespace views {
class Widget;
}

namespace wm {
class ScopedCaptureClient;
}

namespace ash {
class AshTouchExplorationManager;
class AshWindowTreeHost;
class BootSplashScreen;
class DockedWindowLayoutManager;
enum class LoginStatus;
class PanelLayoutManager;
class ShelfLayoutManager;
class StackingController;
class StatusAreaWidget;
class SystemTray;
class SystemWallpaperController;
class TouchHudDebug;
class TouchHudProjection;
class WmRootWindowController;
class WmShelf;
class WorkspaceController;

namespace mus {
class RootWindowController;
}

// This class maintains the per root window state for ash. This class
// owns the root window and other dependent objects that should be
// deleted upon the deletion of the root window. This object is
// indirectly owned and deleted by |WindowTreeHostManager|.
// The RootWindowController for particular root window is stored in
// its property (RootWindowSettings) and can be obtained using
// |GetRootWindowController(aura::WindowEventDispatcher*)| function.
class ASH_EXPORT RootWindowController : public ShellObserver {
 public:
  // Enumerates the type of display. If there is only a single display then
  // it is primary. In a multi-display environment one monitor is deemed the
  // PRIMARY and all others SECONDARY.
  enum class RootWindowType { PRIMARY, SECONDARY };

  ~RootWindowController() override;

  // Creates and Initialize the RootWindowController for primary display.
  static void CreateForPrimaryDisplay(AshWindowTreeHost* host);

  // Creates and Initialize the RootWindowController for secondary displays.
  static void CreateForSecondaryDisplay(AshWindowTreeHost* host);

  // Returns a RootWindowController of the window's root window.
  static RootWindowController* ForWindow(const aura::Window* window);

  // Returns the RootWindowController of the target root window.
  static RootWindowController* ForTargetRootWindow();

  // TODO(sky): move these to a separate class or use AshWindowTreeHost in
  // mash. http://crbug.com/671246.
  AshWindowTreeHost* ash_host() { return ash_host_.get(); }
  const AshWindowTreeHost* ash_host() const { return ash_host_.get(); }

  aura::WindowTreeHost* GetHost();
  const aura::WindowTreeHost* GetHost() const;
  aura::Window* GetRootWindow();
  const aura::Window* GetRootWindow() const;

  WorkspaceController* workspace_controller();

  WmShelf* wm_shelf() const { return wm_shelf_.get(); }

  WmRootWindowController* wm_root_window_controller() {
    return wm_root_window_controller_.get();
  }

  // Get touch HUDs associated with this root window controller.
  TouchHudDebug* touch_hud_debug() const { return touch_hud_debug_; }
  TouchHudProjection* touch_hud_projection() const {
    return touch_hud_projection_;
  }

  // Set touch HUDs for this root window controller. The root window controller
  // will not own the HUDs; their lifetimes are managed by themselves. Whenever
  // the widget showing a HUD is being destroyed (e.g. because of detaching a
  // display), the HUD deletes itself.
  void set_touch_hud_debug(TouchHudDebug* hud) { touch_hud_debug_ = hud; }
  void set_touch_hud_projection(TouchHudProjection* hud) {
    touch_hud_projection_ = hud;
  }

  // Access the shelf layout manager associated with this root
  // window controller, NULL if no such shelf exists.
  ShelfLayoutManager* GetShelfLayoutManager();

  // May return null, for example for a secondary monitor at the login screen.
  StatusAreaWidget* GetStatusAreaWidget();

  // Returns the system tray on this root window. Note that
  // calling this on the root window that doesn't have a shelf will
  // lead to a crash.
  SystemTray* GetSystemTray();

  // True if the window can receive events on this root window.
  bool CanWindowReceiveEvents(aura::Window* window);

  aura::Window* GetContainer(int container_id);
  const aura::Window* GetContainer(int container_id) const;

  // Called when the brightness/grayscale animation from white to the login
  // wallpaper image has started.  Starts |boot_splash_screen_|'s hiding
  // animation (if the screen is non-NULL).
  void OnInitialWallpaperAnimationStarted();

  // Called when the wallpaper animation is finished. Updates
  // |system_wallpaper_| to be black and drops |boot_splash_screen_| and moves
  // the wallpaper controller into the root window controller. |widget| holds
  // the wallpaper image, or NULL if the wallpaper is a solid color.
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

  // If touch exploration is enabled, update the touch exploration
  // controller so that synthesized touch events are anchored at this point.
  void SetTouchAccessibilityAnchorPoint(const gfx::Point& anchor_point);

 private:
  // TODO(sky): remove this. Temporary during ash-mus unification.
  // http://crbug.com/671246.
  friend class mus::RootWindowController;

  // Creates a new RootWindowController with the specified host. Only one of
  // |ash_host| or |window_tree_host| should be specified. This takes ownership
  // of the supplied arguments.
  // TODO(sky): mash should create AshWindowTreeHost, http://crbug.com/671246.
  RootWindowController(AshWindowTreeHost* ash_host,
                       aura::WindowTreeHost* window_tree_host);

  // Initializes the RootWindowController based on |root_window_type|.
  void Init(RootWindowType root_window_type);

  void InitLayoutManagers();

  // Initializes |system_wallpaper_| and possibly also |boot_splash_screen_|.
  // The initial color is determined by the |root_window_type| and whether or
  // not this is the first boot.
  void CreateSystemWallpaper(RootWindowType root_window_type);

  // Enables projection touch HUD.
  void EnableTouchHudProjection();

  // Disables projection touch HUD.
  void DisableTouchHudProjection();

  DockedWindowLayoutManager* docked_window_layout_manager();
  PanelLayoutManager* panel_layout_manager();

  // Overridden from ShellObserver.
  void OnLoginStateChanged(LoginStatus status) override;
  void OnTouchHudProjectionToggled(bool enabled) override;

  std::unique_ptr<AshWindowTreeHost> ash_host_;
  std::unique_ptr<aura::WindowTreeHost> mus_window_tree_host_;
  // This comes from |ash_host_| or |mus_window_tree_host_|.
  aura::WindowTreeHost* window_tree_host_;

  std::unique_ptr<WmRootWindowController> wm_root_window_controller_;

  std::unique_ptr<StackingController> stacking_controller_;

  // The shelf controller for this root window. Exists for the entire lifetime
  // of the RootWindowController so that it is safe for observers to be added
  // to it during construction of the shelf widget and status tray.
  std::unique_ptr<WmShelf> wm_shelf_;

  std::unique_ptr<SystemWallpaperController> system_wallpaper_;

  std::unique_ptr<BootSplashScreen> boot_splash_screen_;
  // Responsible for initializing TouchExplorationController when spoken
  // feedback is on.
  std::unique_ptr<AshTouchExplorationManager> touch_exploration_manager_;

  // Heads-up displays for touch events. These HUDs are not owned by the root
  // window controller and manage their own lifetimes.
  TouchHudDebug* touch_hud_debug_;
  TouchHudProjection* touch_hud_projection_;

  // Handles double clicks on the panel window header.
  std::unique_ptr<ui::EventHandler> panel_container_handler_;

  std::unique_ptr<::wm::ScopedCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowController);
};

// On classic ash, returns the RootWindowController for the given |root_window|.
// On mus ash, returns the RootWindowController for the primary display.
// See RootWindowController class comment above.
ASH_EXPORT RootWindowController* GetRootWindowController(
    const aura::Window* root_window);

}  // namespace ash

#endif  // ASH_ROOT_WINDOW_CONTROLLER_H_
