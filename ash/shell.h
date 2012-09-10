// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_H_
#define ASH_SHELL_H_

#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/cursor_delegate.h"
#include "ash/wm/cursor_manager.h"
#include "ash/wm/shelf_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
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
class CapsLockDelegate;
class DesktopBackgroundController;
class DisplayController;
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
class EventRewriterEventFilter;
class FocusCycler;
class MagnificationController;
class MouseCursorEventFilter;
class OutputConfiguratorAnimation;
class OverlayEventFilter;
class PanelLayoutManager;
class ResizeShadowController;
class RootWindowController;
class RootWindowLayoutManager;
class ScreenPositionController;
class ShadowController;
class ShelfLayoutManager;
class ShellContextMenu;
class SlowAnimationEventFilter;
class StackingController;
class StatusAreaWidget;
class SystemGestureEventFilter;
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
class ASH_EXPORT Shell : ash::CursorDelegate {
 public:
  typedef std::vector<aura::RootWindow*> RootWindowList;
  typedef std::vector<internal::RootWindowController*> RootWindowControllerList;

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

  // Returns the root window controller for the primary root window.
  static internal::RootWindowController* GetPrimaryRootWindowController();

  // Returns all root window controllers.
  static RootWindowControllerList GetAllRootWindowControllers();

  // Returns the primary RootWindow. The primary RootWindow is the one
  // that has a launcher.
  static aura::RootWindow* GetPrimaryRootWindow();

  // Returns the active RootWindow. The active RootWindow is the one that
  // contains the current active window as a decendant child. The active
  // RootWindow remains the same even when the active window becomes NULL,
  // until the another window who has a different root window becomes active.
  static aura::RootWindow* GetActiveRootWindow();

  // Returns all root windows.
  static RootWindowList GetAllRootWindows();

  static aura::Window* GetContainer(aura::RootWindow* root_window,
                                    int container_id);

  // Returns the list of containers that match |container_id| in
  // all root windows.
  static std::vector<aura::Window*> GetAllContainers(int container_id);

  void set_active_root_window(aura::RootWindow* active_root_window) {
    active_root_window_ = active_root_window;
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

  // Sets the work area insets of the display that contains |window|,
  // this notifies observers too.
  // TODO(sky): this no longer really replicates what happens and is unreliable.
  // Remove this.
  void SetDisplayWorkAreaInsets(aura::Window* window,
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

  // Show launcher view if it was created hidden (before session has started).
  void ShowLauncher();

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
  internal::EventRewriterEventFilter* event_rewriter_filter() {
    return event_rewriter_filter_.get();
  }
  internal::OverlayEventFilter* overlay_filter() {
    return overlay_filter_.get();
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
  DisplayController* display_controller() {
    return display_controller_.get();
  }
  internal::MouseCursorEventFilter* mouse_cursor_filter() {
    return mouse_cursor_filter_.get();
  }
  CursorManager* cursor_manager() { return &cursor_manager_; }

  ShellDelegate* delegate() { return delegate_.get(); }

  UserWallpaperDelegate* user_wallpaper_delegate() {
    return user_wallpaper_delegate_.get();
  }

  CapsLockDelegate* caps_lock_delegate() {
    return caps_lock_delegate_.get();
  }

  HighContrastController* high_contrast_controller() {
    return high_contrast_controller_.get();
  }

  internal::MagnificationController* magnification_controller() {
    return magnification_controller_.get();
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

  // Dims or undims the screen.
  void SetDimming(bool should_dim);

  // TODO(sky): don't expose this!
  internal::ShelfLayoutManager* shelf() const { return shelf_; }

  internal::StatusAreaWidget* status_area_widget() const {
    return status_area_widget_;
  }

  // Convenience accessor for members of StatusAreaWidget.
  SystemTrayDelegate* tray_delegate();
  SystemTray* system_tray();

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

  // Initializes the root window to be used for a secondary display.
  void InitRootWindowForSecondaryDisplay(aura::RootWindow* root);

#if defined(OS_CHROMEOS)
  chromeos::OutputConfigurator* output_configurator() {
    return output_configurator_.get();
  }
  internal::OutputConfiguratorAnimation* output_configurator_animation() {
    return output_configurator_animation_.get();
  }
#endif  // defined(OS_CHROMEOS)

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtendedDesktopTest, TestCursor);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, MouseEventCursors);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, TransformActivate);
  friend class internal::RootWindowController;

  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  explicit Shell(ShellDelegate* delegate);
  virtual ~Shell();

  void Init();

  // Initializes the root window and root window controller so that it
  // can host browser windows.
  void InitRootWindowController(internal::RootWindowController* root);

  // Initializes the layout managers and event filters specific for
  // primary display.
  void InitLayoutManagersForPrimaryDisplay(
      internal::RootWindowController* root_window_controller);

  // aura::CursorManager::Delegate overrides:
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual void ShowCursor(bool visible) OVERRIDE;

  static Shell* instance_;

  // If set before the Shell is initialized, the mouse cursor will be hidden
  // when the screen is initially created.
  static bool initially_hide_cursor_;

  ScreenAsh* screen_;

  // Active root window. Never becomes NULL during the session.
  aura::RootWindow* active_root_window_;

  // The CompoundEventFilter owned by aura::Env object.
  aura::shared::CompoundEventFilter* env_filter_;

  std::vector<WindowAndBoundsPair> to_restore_;

#if !defined(OS_MACOSX)
  scoped_ptr<NestedDispatcherController> nested_dispatcher_controller_;

  scoped_ptr<AcceleratorController> accelerator_controller_;
#endif  // !defined(OS_MACOSX)

  scoped_ptr<ShellDelegate> delegate_;
  scoped_ptr<UserWallpaperDelegate> user_wallpaper_delegate_;
  scoped_ptr<CapsLockDelegate> caps_lock_delegate_;

  scoped_ptr<Launcher> launcher_;

  scoped_ptr<internal::AppListController> app_list_controller_;

  scoped_ptr<internal::ShellContextMenu> shell_context_menu_;
  scoped_ptr<internal::StackingController> stacking_controller_;
  scoped_ptr<internal::ActivationController> activation_controller_;
  scoped_ptr<internal::CaptureController> capture_controller_;
  scoped_ptr<internal::WindowModalityController> window_modality_controller_;
  scoped_ptr<internal::DragDropController> drag_drop_controller_;
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
  scoped_ptr<DisplayController> display_controller_;
  scoped_ptr<HighContrastController> high_contrast_controller_;
  scoped_ptr<internal::MagnificationController> magnification_controller_;
  scoped_ptr<aura::FocusManager> focus_manager_;
  scoped_ptr<aura::client::UserActionClient> user_action_client_;
  scoped_ptr<internal::MouseCursorEventFilter> mouse_cursor_filter_;
  scoped_ptr<internal::ScreenPositionController> screen_position_controller_;

  // An event filter that rewrites or drops an event.
  scoped_ptr<internal::EventRewriterEventFilter> event_rewriter_filter_;

  // An event filter that pre-handles key events while the partial
  // screenshot UI or the keyboard overlay is active.
  scoped_ptr<internal::OverlayEventFilter> overlay_filter_;

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

#if defined(OS_CHROMEOS)
  // Controls video output device state.
  scoped_ptr<chromeos::OutputConfigurator> output_configurator_;
  scoped_ptr<internal::OutputConfiguratorAnimation>
      output_configurator_animation_;
#endif  // defined(OS_CHROMEOS)

  CursorManager cursor_manager_;

  // The shelf for managing the launcher and the status widget in non-compact
  // mode. Shell does not own the shelf. Instead, it is owned by container of
  // the status area.
  internal::ShelfLayoutManager* shelf_;

  // Manages layout of panels. Owned by PanelContainer.
  internal::PanelLayoutManager* panel_layout_manager_;

  ObserverList<ShellObserver> observers_;

  // Widget containing system tray.
  internal::StatusAreaWidget* status_area_widget_;

  // Used by ash/shell.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace ash

#endif  // ASH_SHELL_H_
