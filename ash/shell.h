// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_H_
#define ASH_SHELL_H_
#pragma once

#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/cursor_delegate.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/size.h"

class CommandLine;

namespace aura {
class EventFilter;
class FocusManager;
class RootWindow;
class Window;
namespace client {
class UserActionClient;
}
namespace shared {
class CompoundEventFilter;
class InputMethodEventFilter;
}
}
namespace chromeos {
class OutputConfigurator;
}
namespace content {
class BrowserContext;
}

namespace gfx {
class ImageSkia;
class Point;
class Rect;
}
namespace ui {
class Layer;
}
namespace views {
class NonClientFrameView;
class Widget;
}

namespace ash {

class AcceleratorController;
class DesktopBackgroundController;
class HighContrastController;
class Launcher;
class NestedDispatcherController;
class PowerButtonController;
class ScreenAsh;
class ShellDelegate;
class ShellObserver;
class SystemTrayDelegate;
class SystemTray;
class UserActivityDetector;
class UserWallpaperDelegate;
class VideoDetector;
class WindowCycleController;

namespace internal {
class AcceleratorFilter;
class ActivationController;
class AppListController;
class CaptureController;
class DragDropController;
class EventClientImpl;
class FocusCycler;
class KeyRewriterEventFilter;
class MagnificationController;
class MonitorController;
class PanelLayoutManager;
class PartialScreenshotEventFilter;
class ResizeShadowController;
class RootWindowLayoutManager;
class ScreenDimmer;
class ShadowController;
class ShelfLayoutManager;
class ShellContextMenu;
class SlowAnimationEventFilter;
class SystemGestureEventFilter;
class StackingController;
class StatusAreaWidget;
class TooltipController;
class TouchObserverHUD;
class VisibilityController;
class WindowModalityController;
class WorkspaceController;
}

// Shell is a singleton object that presents the Shell API and implements the
// RootWindow's delegate interface.
//
// Upon creation, the Shell sets itself as the RootWindow's delegate, which
// takes ownership of the Shell.
class ASH_EXPORT Shell : aura::CursorDelegate {
 public:
  enum Direction {
    FORWARD,
    BACKWARD
  };

  // Accesses private data from a Shell for testing.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(Shell* shell);

    internal::RootWindowLayoutManager* root_window_layout();
    aura::shared::InputMethodEventFilter* input_method_event_filter();
    internal::SystemGestureEventFilter* system_gesture_event_filter();
    internal::WorkspaceController* workspace_controller();

   private:
    Shell* shell_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // A shell must be explicitly created so that it can call |Init()| with the
  // delegate set. |delegate| can be NULL (if not required for initialization).
  static Shell* CreateInstance(ShellDelegate* delegate);

  // Should never be called before |CreateInstance()|.
  static Shell* GetInstance();

  // Returns true if the ash shell has been instantiated.
  static bool HasInstance();

  static void DeleteInstance();

  // Gets the primary RootWindow. The primary RootWindow is the one
  // that has a launcher.
  static aura::RootWindow* GetPrimaryRootWindow();

  // Gets the active RootWindow. The active RootWindow is the one that
  // contains the current active window as a decendant child. The active
  // RootWindow remains the same even when the active window becomes NULL,
  // until the another window who has a different root window becomes active.
  static aura::RootWindow* GetActiveRootWindow();

  // Gets the RootWindow at |point| in the virtual screen coordinates.
  // Returns NULL if the root window does not exist at the given
  // point.
  static aura::RootWindow* GetRootWindowAt(const gfx::Point& point);

  static aura::Window* GetContainer(aura::RootWindow* root_window,
                                    int container_id);

  // Returns the list of containers that match |container_id| in
  // all root windows.
  static std::vector<aura::Window*> GetAllContainers(int container_id);

  void set_active_root_window(aura::RootWindow* active_root_window) {
    active_root_window_ = active_root_window;
  }

  internal::RootWindowLayoutManager* root_window_layout() const {
    return root_window_layout_;
  }

  // Adds or removes |filter| from the aura::Env's CompoundEventFilter.
  void AddEnvEventFilter(aura::EventFilter* filter);
  void RemoveEnvEventFilter(aura::EventFilter* filter);
  size_t GetEnvEventFilterCount() const;

  // Shows the background menu over |widget|.
  void ShowBackgroundMenu(views::Widget* widget, const gfx::Point& location);

  // Toggles app list.
  void ToggleAppList();

  // Returns app list target visibility.
  bool GetAppListTargetVisibility() const;

  // Returns app list window or NULL if it is not visible.
  aura::Window* GetAppListWindow();

  // Returns true if the screen is locked.
  bool IsScreenLocked() const;

  // Returns true if a modal dialog window is currently open.
  bool IsModalWindowOpen() const;

  // Creates a default views::NonClientFrameView for use by windows in the
  // Ash environment.
  views::NonClientFrameView* CreateDefaultNonClientFrameView(
      views::Widget* widget);

  // Rotates focus through containers that can receive focus.
  void RotateFocus(Direction direction);

  // Sets the work area insets of the monitor that contains |window|,
  // this notifies observers too.
  // TODO(sky): this no longer really replicates what happens and is unreliable.
  // Remove this.
  void SetMonitorWorkAreaInsets(aura::Window* window,
                                const gfx::Insets& insets);

  // Called when the user logs in.
  void OnLoginStateChanged(user::LoginStatus status);

  // Called when the application is exiting.
  void OnAppTerminating();

  // Called when the screen is locked (after the lock window is visible) or
  // unlocked.
  void OnLockStateChanged(bool locked);

  // Initializes |launcher_|.  Does nothing if it's already initialized.
  void CreateLauncher();

  // Adds/removes observer.
  void AddShellObserver(ShellObserver* observer);
  void RemoveShellObserver(ShellObserver* observer);

#if !defined(OS_MACOSX)
  AcceleratorController* accelerator_controller() {
    return accelerator_controller_.get();
  }
#endif  // !defined(OS_MACOSX)

  aura::shared::CompoundEventFilter* env_filter() {
    return env_filter_;
  }
  internal::TooltipController* tooltip_controller() {
    return tooltip_controller_.get();
  }
  internal::KeyRewriterEventFilter* key_rewriter_filter() {
    return key_rewriter_filter_.get();
  }
  internal::PartialScreenshotEventFilter* partial_screenshot_filter() {
    return partial_screenshot_filter_.get();
  }
  DesktopBackgroundController* desktop_background_controller() {
    return desktop_background_controller_.get();
  }
  PowerButtonController* power_button_controller() {
    return power_button_controller_.get();
  }
  UserActivityDetector* user_activity_detector() {
    return user_activity_detector_.get();
  }
  VideoDetector* video_detector() {
    return video_detector_.get();
  }
  WindowCycleController* window_cycle_controller() {
    return window_cycle_controller_.get();
  }
  internal::FocusCycler* focus_cycler() {
    return focus_cycler_.get();
  }
  internal::MonitorController* monitor_controller() {
    return monitor_controller_.get();
  }

  ShellDelegate* delegate() { return delegate_.get(); }
  SystemTrayDelegate* tray_delegate() { return tray_delegate_.get(); }
  UserWallpaperDelegate* user_wallpaper_delegate() {
    return user_wallpaper_delegate_.get();
  }

  HighContrastController* high_contrast_controller() {
    return high_contrast_controller_.get();
  }

  internal::MagnificationController* magnification_controller() {
    return magnification_controller_.get();
  }

  internal::ScreenDimmer* screen_dimmer() {
    return screen_dimmer_.get();
  }

  Launcher* launcher() { return launcher_.get(); }

  const ScreenAsh* screen() { return screen_; }

  // Force the shelf to query for it's current visibility state.
  void UpdateShelfVisibility();

  // Sets/gets the shelf auto-hide behavior.
  void SetShelfAutoHideBehavior(ShelfAutoHideBehavior behavior);
  ShelfAutoHideBehavior GetShelfAutoHideBehavior() const;

  void SetShelfAlignment(ShelfAlignment alignment);
  ShelfAlignment GetShelfAlignment();

  // TODO(sky): don't expose this!
  internal::ShelfLayoutManager* shelf() const { return shelf_; }

  internal::StatusAreaWidget* status_area_widget() const {
    return status_area_widget_;
  }

  SystemTray* system_tray() const { return system_tray_.get(); }

  // Returns the size of the grid.
  int GetGridSize() const;

  // Returns true if in maximized or fullscreen mode.
  bool IsInMaximizedMode() const;

  static void set_initially_hide_cursor(bool hide) {
    initially_hide_cursor_ = hide;
  }

  internal::ResizeShadowController* resize_shadow_controller() {
    return resize_shadow_controller_.get();
  }

  // Made available for tests.
  internal::ShadowController* shadow_controller() {
    return shadow_controller_.get();
  }

  content::BrowserContext* browser_context() { return browser_context_; }
  void set_browser_context(content::BrowserContext* browser_context) {
    browser_context_ = browser_context;
  }

  // Initialize the root window to be used for a secondary monitor.
  void InitRootWindowForSecondaryMonitor(aura::RootWindow* root);

#if defined(OS_CHROMEOS)
  chromeos::OutputConfigurator* output_configurator() {
    return output_configurator_.get();
  }
#endif  // defined(OS_CHROMEOS)

 private:
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, MouseEventCursors);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, TransformActivate);

  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  explicit Shell(ShellDelegate* delegate);
  virtual ~Shell();

  void Init();

  // Initializes the layout managers and event filters.
  void InitLayoutManagers();

  // Disables the workspace grid layout.
  void DisableWorkspaceGridLayout();

  // aura::CursorManager::Delegate overrides:
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual void ShowCursor(bool visible) OVERRIDE;

  static Shell* instance_;

  // If set before the Shell is initialized, the mouse cursor will be hidden
  // when the screen is initially created.
  static bool initially_hide_cursor_;

  scoped_ptr<aura::RootWindow> root_window_;
  ScreenAsh* screen_;

  // Active root window. Never become NULL.
  aura::RootWindow* active_root_window_;

  // The CompoundEventFilter owned by aura::Env object.
  aura::shared::CompoundEventFilter* env_filter_;

  std::vector<WindowAndBoundsPair> to_restore_;

#if !defined(OS_MACOSX)
  scoped_ptr<NestedDispatcherController> nested_dispatcher_controller_;

  scoped_ptr<AcceleratorController> accelerator_controller_;
#endif  // !defined(OS_MACOSX)

  scoped_ptr<ShellDelegate> delegate_;
  scoped_ptr<SystemTrayDelegate> tray_delegate_;
  scoped_ptr<UserWallpaperDelegate> user_wallpaper_delegate_;

  scoped_ptr<Launcher> launcher_;

  scoped_ptr<internal::AppListController> app_list_controller_;

  scoped_ptr<internal::ShellContextMenu> shell_context_menu_;
  scoped_ptr<internal::StackingController> stacking_controller_;
  scoped_ptr<internal::ActivationController> activation_controller_;
  scoped_ptr<internal::CaptureController> capture_controller_;
  scoped_ptr<internal::WindowModalityController> window_modality_controller_;
  scoped_ptr<internal::DragDropController> drag_drop_controller_;
  scoped_ptr<internal::WorkspaceController> workspace_controller_;
  scoped_ptr<internal::ResizeShadowController> resize_shadow_controller_;
  scoped_ptr<internal::ShadowController> shadow_controller_;
  scoped_ptr<internal::TooltipController> tooltip_controller_;
  scoped_ptr<internal::VisibilityController> visibility_controller_;
  scoped_ptr<DesktopBackgroundController> desktop_background_controller_;
  scoped_ptr<PowerButtonController> power_button_controller_;
  scoped_ptr<UserActivityDetector> user_activity_detector_;
  scoped_ptr<VideoDetector> video_detector_;
  scoped_ptr<WindowCycleController> window_cycle_controller_;
  scoped_ptr<internal::FocusCycler> focus_cycler_;
  scoped_ptr<internal::EventClientImpl> event_client_;
  scoped_ptr<internal::MonitorController> monitor_controller_;
  scoped_ptr<HighContrastController> high_contrast_controller_;
  scoped_ptr<internal::MagnificationController> magnification_controller_;
  scoped_ptr<internal::ScreenDimmer> screen_dimmer_;
  scoped_ptr<aura::FocusManager> focus_manager_;
  scoped_ptr<aura::client::UserActionClient> user_action_client_;

  // An event filter that rewrites or drops a key event.
  scoped_ptr<internal::KeyRewriterEventFilter> key_rewriter_filter_;

  // An event filter that pre-handles key events while the partial
  // screenshot UI is active.
  scoped_ptr<internal::PartialScreenshotEventFilter> partial_screenshot_filter_;

  // An event filter which handles system level gestures
  scoped_ptr<internal::SystemGestureEventFilter> system_gesture_filter_;

#if !defined(OS_MACOSX)
  // An event filter that pre-handles global accelerators.
  scoped_ptr<internal::AcceleratorFilter> accelerator_filter_;
#endif

  // An event filter that pre-handles all key events to send them to an IME.
  scoped_ptr<aura::shared::InputMethodEventFilter> input_method_filter_;

  // An event filter that silently keeps track of all touch events and controls
  // a heads-up display. This is enabled only if --ash-touch-hud flag is used.
  scoped_ptr<internal::TouchObserverHUD> touch_observer_hud_;

  // An event filter that looks for modifier keypresses and triggers a slowdown
  // of layer animations for visual debugging.
  scoped_ptr<internal::SlowAnimationEventFilter> slow_animation_filter_;

#if defined(OS_CHROMEOS)
  // Controls video output device state.
  scoped_ptr<chromeos::OutputConfigurator> output_configurator_;
#endif  // defined(OS_CHROMEOS)

  // The shelf for managing the launcher and the status widget in non-compact
  // mode. Shell does not own the shelf. Instead, it is owned by container of
  // the status area.
  internal::ShelfLayoutManager* shelf_;

  // Manages layout of panels. Owned by PanelContainer.
  internal::PanelLayoutManager* panel_layout_manager_;

  ObserverList<ShellObserver> observers_;

  // Owned by aura::RootWindow, cached here for type safety.
  internal::RootWindowLayoutManager* root_window_layout_;

  // Widget containing system tray.
  internal::StatusAreaWidget* status_area_widget_;

  // System tray with clock, Wi-Fi signal, etc.
  scoped_ptr<SystemTray> system_tray_;

  // Used by ash/shell.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace ash

#endif  // ASH_SHELL_H_
