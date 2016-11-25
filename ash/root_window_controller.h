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
}

namespace views {
class Widget;
}

namespace wm {
class ScopedCaptureClient;
}

namespace ash {
class AshWindowTreeHost;
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
class WmRootWindowControllerAura;
class WmShelfAura;
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

  WorkspaceController* workspace_controller();

  WmShelfAura* wm_shelf_aura() const { return wm_shelf_aura_.get(); }

  WmRootWindowControllerAura* wm_root_window_controller() {
    return wm_root_window_controller_;
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
  explicit RootWindowController(AshWindowTreeHost* host);
  enum RootWindowType { PRIMARY, SECONDARY };

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

  // Owned by the root window.
  WmRootWindowControllerAura* wm_root_window_controller_ = nullptr;

  std::unique_ptr<StackingController> stacking_controller_;

  // The shelf controller for this root window. Exists for the entire lifetime
  // of the RootWindowController so that it is safe for observers to be added
  // to it during construction of the shelf widget and status tray.
  std::unique_ptr<WmShelfAura> wm_shelf_aura_;

  std::unique_ptr<SystemWallpaperController> system_wallpaper_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<BootSplashScreen> boot_splash_screen_;
  // Responsible for initializing TouchExplorationController when spoken
  // feedback is on.
  std::unique_ptr<AshTouchExplorationManager> touch_exploration_manager_;
#endif

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
