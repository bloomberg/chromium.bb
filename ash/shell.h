// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_H_
#define ASH_SHELL_H_

#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/shelf/shelf_types.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/system_modal_container_event_filter_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/base/events/event_target.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/views/corewm/cursor_manager.h"

class CommandLine;

namespace aura {
class EventFilter;
class RootWindow;
class Window;
namespace client {
class ActivationClient;
class FocusClient;
class UserActionClient;
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
namespace corewm {
class CompoundEventFilter;
class InputMethodEventFilter;
class ShadowController;
class TooltipController;
class VisibilityController;
class WindowModalityController;
}
}

namespace ash {

class AcceleratorController;
class AshNativeCursorManager;
class CapsLockDelegate;
class DesktopBackgroundController;
class DisplayController;
class HighContrastController;
class Launcher;
class LauncherDelegate;
class LauncherModel;
class MagnificationController;
class NestedDispatcherController;
class PartialMagnificationController;
class PowerButtonController;
class RootWindowHostFactory;
class ScreenAsh;
class SessionStateController;
class ShellDelegate;
class ShellObserver;
class SystemTray;
class SystemTrayDelegate;
class SystemTrayNotifier;
class UserActivityDetector;
class UserWallpaperDelegate;
class VideoDetector;
class WebNotificationTray;
class WindowCycleController;

namespace internal {
class AcceleratorFilter;
class ActivationController;
class AppListController;
class CaptureController;
class DisplayChangeObserverX11;
class DisplayErrorObserver;
class DisplayManager;
class DragDropController;
class EventClientImpl;
class EventRewriterEventFilter;
class EventTransformationHandler;
class FocusCycler;
class MouseCursorEventFilter;
class OutputConfiguratorAnimation;
class OverlayEventFilter;
class ResizeShadowController;
class RootWindowController;
class RootWindowLayoutManager;
class ScreenPositionController;
class SlowAnimationEventFilter;
class StatusAreaWidget;
class SystemGestureEventFilter;
class SystemModalContainerEventFilter;
class TouchObserverHUD;
class WorkspaceController;
}

namespace shell {
class WindowWatcher;
}

namespace test {
class ShellTestApi;
}

// Shell is a singleton object that presents the Shell API and implements the
// RootWindow's delegate interface.
//
// Upon creation, the Shell sets itself as the RootWindow's delegate, which
// takes ownership of the Shell.
class ASH_EXPORT Shell
    : public internal::SystemModalContainerEventFilterDelegate,
      public ui::EventTarget,
      public aura::client::ActivationChangeObserver {
 public:
  typedef std::vector<aura::RootWindow*> RootWindowList;
  typedef std::vector<internal::RootWindowController*> RootWindowControllerList;

  enum Direction {
    FORWARD,
    BACKWARD
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
  // TODO(oshima): move this to |RootWindowController|
  static internal::RootWindowController* GetPrimaryRootWindowController();

  // Returns all root window controllers.
  // TODO(oshima): move this to |RootWindowController|
  static RootWindowControllerList GetAllRootWindowControllers();

  // Returns the primary RootWindow. The primary RootWindow is the one
  // that has a launcher.
  static aura::RootWindow* GetPrimaryRootWindow();

  // Returns the active RootWindow. The active RootWindow is the one that
  // contains the current active window as a decendant child. The active
  // RootWindow remains the same even when the active window becomes NULL,
  // until the another window who has a different root window becomes active.
  static aura::RootWindow* GetActiveRootWindow();

  // Returns the global Screen object that's always active in ash.
  static gfx::Screen* GetScreen();

  // Returns all root windows.
  static RootWindowList GetAllRootWindows();

  static aura::Window* GetContainer(aura::RootWindow* root_window,
                                    int container_id);
  static const aura::Window* GetContainer(const aura::RootWindow* root_window,
                                          int container_id);

  // Returns the list of containers that match |container_id| in
  // all root windows. If |priority_root| is given, the container
  // in the |priority_root| will be inserted at the top of the list.
  static std::vector<aura::Window*> GetContainersFromAllRootWindows(
      int container_id,
      aura::RootWindow* priority_root);

  // True if "launcher per display" feature  is enabled.
  static bool IsLauncherPerDisplayEnabled();

  void set_active_root_window(aura::RootWindow* active_root_window) {
    active_root_window_ = active_root_window;
  }

  // Shows the context menu for the background and launcher at
  // |location_in_screen| (in screen coordinates).
  void ShowContextMenu(const gfx::Point& location_in_screen);

  // Toggles the app list. |window| specifies in which display the app
  // list should be shown. If this is NULL, the active root window
  // will be used.
  void ToggleAppList(aura::Window* anchor);

  // Returns app list target visibility.
  bool GetAppListTargetVisibility() const;

  // Returns app list window or NULL if it is not visible.
  aura::Window* GetAppListWindow();

  // Returns true if a user is logged in whose session can be locked (i.e. the
  // user has a password with which to unlock the session).
  bool CanLockScreen();

  // Returns true if the screen is locked.
  bool IsScreenLocked() const;

  // Returns true if a system-modal dialog window is currently open.
  bool IsSystemModalWindowOpen() const;

  // For testing only: set simulation that a modal window is open
  void SimulateModalWindowOpenForTesting(bool modal_window_open) {
    simulate_modal_window_open_for_testing_ = modal_window_open;
  }

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

  // Called when the login status changes.
  // TODO(oshima): Investigate if we can merge this and |OnLoginStateChanged|.
  void UpdateAfterLoginStatusChange(user::LoginStatus status);

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

  internal::DisplayManager* display_manager() {
    return display_manager_.get();
  }
  views::corewm::CompoundEventFilter* env_filter() {
    return env_filter_.get();
  }
  views::corewm::TooltipController* tooltip_controller() {
    return tooltip_controller_.get();
  }
  internal::TouchObserverHUD* touch_observer_hud() {
    return touch_observer_hud_.get();
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
  SessionStateController* session_state_controller() {
    return session_state_controller_.get();
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
  internal::EventTransformationHandler* event_transformation_handler() {
    return event_transformation_handler_.get();
  }
  views::corewm::CursorManager* cursor_manager() { return &cursor_manager_; }

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

  MagnificationController* magnification_controller() {
    return magnification_controller_.get();
  }

  PartialMagnificationController* partial_magnification_controller() {
    return partial_magnification_controller_.get();
  }
  aura::client::ActivationClient* activation_client() {
    return activation_client_;
  }

  ScreenAsh* screen() { return screen_; }

  // Force the shelf to query for it's current visibility state.
  void UpdateShelfVisibility();

  // TODO(oshima): Define an interface to access shelf/launcher
  // state, or just use Launcher.

  // Sets/gets the shelf auto-hide behavior on |root_window|.
  void SetShelfAutoHideBehavior(ShelfAutoHideBehavior behavior,
                                aura::RootWindow* root_window);
  ShelfAutoHideBehavior GetShelfAutoHideBehavior(
      aura::RootWindow* root_window) const;

  // Sets/gets shelf's alignment on |root_window|.
  void SetShelfAlignment(ShelfAlignment alignment,
                         aura::RootWindow* root_window);
  ShelfAlignment GetShelfAlignment(aura::RootWindow* root_window);

  // Dims or undims the screen.
  void SetDimming(bool should_dim);

  // Creates a modal background (a partially-opaque fullscreen window)
  // on all displays for |window|.
  void CreateModalBackground(aura::Window* window);

  // Called when a modal window is removed. It will activate
  // another modal window if any, or remove modal screens
  // on all displays.
  void OnModalWindowRemoved(aura::Window* removed);

  // Returns WebNotificationTray on the primary root window.
  WebNotificationTray* GetWebNotificationTray();

  // Does the primary display have status area?
  bool HasPrimaryStatusArea();

  // Returns the system tray on primary display.
  SystemTray* GetPrimarySystemTray();

  SystemTrayDelegate* system_tray_delegate() {
    return system_tray_delegate_.get();
  }

  SystemTrayNotifier* system_tray_notifier() {
    return system_tray_notifier_.get();
  }

  static void set_initially_hide_cursor(bool hide) {
    initially_hide_cursor_ = hide;
  }

  internal::ResizeShadowController* resize_shadow_controller() {
    return resize_shadow_controller_.get();
  }

  // Made available for tests.
  views::corewm::ShadowController* shadow_controller() {
    return shadow_controller_.get();
  }

  content::BrowserContext* browser_context() { return browser_context_; }
  void set_browser_context(content::BrowserContext* browser_context) {
    browser_context_ = browser_context;
  }

  // Initializes the root window to be used for a secondary display.
  void InitRootWindowForSecondaryDisplay(aura::RootWindow* root);

  // Starts the animation that occurs on first login.
  void DoInitialWorkspaceAnimation();

#if defined(OS_CHROMEOS)
  // TODO(oshima): Move these objects to DisplayController.
  chromeos::OutputConfigurator* output_configurator() {
    return output_configurator_.get();
  }
  internal::OutputConfiguratorAnimation* output_configurator_animation() {
    return output_configurator_animation_.get();
  }
  internal::DisplayErrorObserver* display_error_observer() {
    return display_error_observer_.get();
  }
#endif  // defined(OS_CHROMEOS)

  RootWindowHostFactory* root_window_host_factory() {
    return root_window_host_factory_.get();
  }

  LauncherModel* launcher_model() {
    return launcher_model_.get();
  }

  // Returns the launcher delegate, creating if necesary.
  LauncherDelegate* GetLauncherDelegate();

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtendedDesktopTest, TestCursor);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, MouseEventCursors);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, TransformActivate);
  friend class internal::RootWindowController;
  friend class test::ShellTestApi;
  friend class shell::WindowWatcher;

  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  explicit Shell(ShellDelegate* delegate);
  virtual ~Shell();

  void Init();

  // Initializes the root window and root window controller so that it
  // can host browser windows.
  void InitRootWindowController(internal::RootWindowController* root);

  // ash::internal::SystemModalContainerEventFilterDelegate overrides:
  virtual bool CanWindowReceiveEvents(aura::Window* window) OVERRIDE;

  // Overridden from ui::EventTarget:
  virtual bool CanAcceptEvent(const ui::Event& event) OVERRIDE;
  virtual EventTarget* GetParentTarget() OVERRIDE;
  virtual void OnEvent(ui::Event* event) OVERRIDE;

  // Overridden from aura::client::ActivationChangeObserver:
  virtual void OnWindowActivated(aura::Window* gained_active,
                                 aura::Window* lost_active) OVERRIDE;

  static Shell* instance_;

  // If set before the Shell is initialized, the mouse cursor will be hidden
  // when the screen is initially created.
  static bool initially_hide_cursor_;

  ScreenAsh* screen_;

  // Active root window. Never becomes NULL during the session.
  aura::RootWindow* active_root_window_;

  // The CompoundEventFilter owned by aura::Env object.
  scoped_ptr<views::corewm::CompoundEventFilter> env_filter_;

  std::vector<WindowAndBoundsPair> to_restore_;

#if !defined(OS_MACOSX)
  scoped_ptr<NestedDispatcherController> nested_dispatcher_controller_;

  scoped_ptr<AcceleratorController> accelerator_controller_;
#endif  // !defined(OS_MACOSX)

  scoped_ptr<ShellDelegate> delegate_;
  scoped_ptr<SystemTrayDelegate> system_tray_delegate_;
  scoped_ptr<SystemTrayNotifier> system_tray_notifier_;
  scoped_ptr<UserWallpaperDelegate> user_wallpaper_delegate_;
  scoped_ptr<CapsLockDelegate> caps_lock_delegate_;
  scoped_ptr<LauncherDelegate> launcher_delegate_;

  scoped_ptr<LauncherModel> launcher_model_;

  scoped_ptr<internal::AppListController> app_list_controller_;

  scoped_ptr<internal::ActivationController> activation_controller_;
  scoped_ptr<internal::CaptureController> capture_controller_;
  scoped_ptr<internal::DragDropController> drag_drop_controller_;
  scoped_ptr<internal::ResizeShadowController> resize_shadow_controller_;
  scoped_ptr<views::corewm::ShadowController> shadow_controller_;
  scoped_ptr<views::corewm::VisibilityController> visibility_controller_;
  scoped_ptr<views::corewm::WindowModalityController>
      window_modality_controller_;
  scoped_ptr<views::corewm::TooltipController> tooltip_controller_;
  scoped_ptr<DesktopBackgroundController> desktop_background_controller_;
  scoped_ptr<PowerButtonController> power_button_controller_;
  scoped_ptr<SessionStateController> session_state_controller_;
  scoped_ptr<UserActivityDetector> user_activity_detector_;
  scoped_ptr<VideoDetector> video_detector_;
  scoped_ptr<WindowCycleController> window_cycle_controller_;
  scoped_ptr<internal::FocusCycler> focus_cycler_;
  scoped_ptr<DisplayController> display_controller_;
  scoped_ptr<HighContrastController> high_contrast_controller_;
  scoped_ptr<MagnificationController> magnification_controller_;
  scoped_ptr<PartialMagnificationController> partial_magnification_controller_;
  scoped_ptr<aura::client::FocusClient> focus_client_;
  scoped_ptr<aura::client::UserActionClient> user_action_client_;
  aura::client::ActivationClient* activation_client_;
  scoped_ptr<internal::MouseCursorEventFilter> mouse_cursor_filter_;
  scoped_ptr<internal::ScreenPositionController> screen_position_controller_;
  scoped_ptr<internal::SystemModalContainerEventFilter> modality_filter_;
  scoped_ptr<internal::EventClientImpl> event_client_;
  scoped_ptr<internal::EventTransformationHandler>
      event_transformation_handler_;
  scoped_ptr<RootWindowHostFactory> root_window_host_factory_;

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
  scoped_ptr<views::corewm::InputMethodEventFilter> input_method_filter_;

  // An event filter that silently keeps track of all touch events and controls
  // a heads-up display. This is enabled only if --ash-touch-hud flag is used.
  scoped_ptr<internal::TouchObserverHUD> touch_observer_hud_;

  scoped_ptr<internal::DisplayManager> display_manager_;

#if defined(OS_CHROMEOS)
  // Controls video output device state.
  scoped_ptr<chromeos::OutputConfigurator> output_configurator_;
  scoped_ptr<internal::OutputConfiguratorAnimation>
      output_configurator_animation_;
  scoped_ptr<internal::DisplayErrorObserver> display_error_observer_;

  // Receives output change events and udpates the display manager.
  scoped_ptr<internal::DisplayChangeObserverX11> display_change_observer_;
#endif  // defined(OS_CHROMEOS)

  // |native_cursor_manager_| is owned by |cursor_manager_|, but we keep a
  // pointer to vend to test code.
  AshNativeCursorManager* native_cursor_manager_;
  views::corewm::CursorManager cursor_manager_;

  ObserverList<ShellObserver> observers_;

  // Used by ash/shell.
  content::BrowserContext* browser_context_;

  // For testing only: simulate that a modal window is open
  bool simulate_modal_window_open_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace ash

#endif  // ASH_SHELL_H_
