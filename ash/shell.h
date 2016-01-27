// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_H_
#define ASH_SHELL_H_

#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shelf/shelf_types.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/cursor_manager_chromeos.h"
#include "ash/wm/system_modal_container_event_filter_delegate.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event_target.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/public/activation_change_observer.h"

namespace app_list {
class AppListView;
}
namespace aura {
class EventFilter;
class RootWindow;
class Window;
namespace client {
class ActivationClient;
class FocusClient;
}
}

namespace base {
class SequencedWorkerPool;
}

namespace gfx {
class ImageSkia;
class Point;
class Rect;
}

namespace ui {
class DisplayConfigurator;
class Layer;
class UserActivityDetector;
class UserActivityPowerManagerNotifier;
}
namespace views {
class NonClientFrameView;
class Widget;
namespace corewm {
class TooltipController;
}
}

namespace wm {
class AcceleratorFilter;
class CompoundEventFilter;
class NestedAcceleratorController;
class ShadowController;
class VisibilityController;
class WindowModalityController;
}

namespace ash {

class AcceleratorController;
class AccessibilityDelegate;
class AppListController;
class AshNativeCursorManager;
class AutoclickController;
class BluetoothNotificationController;
class CaptureController;
class DesktopBackgroundController;
class DisplayChangeObserver;
class DisplayColorManager;
class DisplayConfigurationController;
class WindowTreeHostManager;
class DisplayErrorObserver;
class DisplayManager;
class DragDropController;
class EventClientImpl;
class EventRewriterEventFilter;
class EventTransformationHandler;
class FirstRunHelper;
class FocusCycler;
class GPUSupport;
class HighContrastController;
class KeyboardUMAEventFilter;
class LastWindowClosedLogoutReminder;
class LocaleNotificationController;
class LockStateController;
class LogoutConfirmationController;
class MagnificationController;
class MaximizeModeController;
class MaximizeModeWindowManager;
class MediaDelegate;
class MouseCursorEventFilter;
class MruWindowTracker;
class NewWindowDelegate;
class OverlayEventFilter;
class PartialMagnificationController;
class PartialScreenshotController;
class PowerButtonController;
class PowerEventObserver;
class ProjectingObserver;
class ResizeShadowController;
class ResolutionNotificationController;
class RootWindowController;
class ScopedTargetRootWindow;
class ScreenAsh;
class ScreenOrientationController;
class ScreenPositionController;
class SessionStateDelegate;
class Shelf;
class ShelfDelegate;
class ShelfItemDelegateManager;
class ShelfModel;
class ShelfWindowWatcher;
class ShellDelegate;
struct ShellInitParams;
class ShellObserver;
class SlowAnimationEventFilter;
class StatusAreaWidget;
class StickyKeysController;
class SystemGestureEventFilter;
class SystemModalContainerEventFilter;
class SystemTray;
class SystemTrayDelegate;
class SystemTrayNotifier;
class ToplevelWindowEventHandler;
class TouchTransformerController;
class TouchObserverHUD;
class UserWallpaperDelegate;
class VirtualKeyboardController;
class VideoActivityNotifier;
class VideoDetector;
class WebNotificationTray;
class WindowCycleController;
class WindowPositioner;
class WindowSelectorController;

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
class ASH_EXPORT Shell : public SystemModalContainerEventFilterDelegate,
                         public ui::EventTarget,
                         public aura::client::ActivationChangeObserver {
 public:
  typedef std::vector<RootWindowController*> RootWindowControllerList;

  enum Direction {
    FORWARD,
    BACKWARD
  };

  // A shell must be explicitly created so that it can call |Init()| with the
  // delegate set. |delegate| can be NULL (if not required for initialization).
  // Takes ownership of |delegate|.
  static Shell* CreateInstance(const ShellInitParams& init_params);

  // Should never be called before |CreateInstance()|.
  static Shell* GetInstance();

  // Returns true if the ash shell has been instantiated.
  static bool HasInstance();

  static void DeleteInstance();

  // Returns the root window controller for the primary root window.
  // TODO(oshima): move this to |RootWindowController|
  static RootWindowController* GetPrimaryRootWindowController();

  // Returns all root window controllers.
  // TODO(oshima): move this to |RootWindowController|
  static RootWindowControllerList GetAllRootWindowControllers();

  // Returns the primary root Window. The primary root Window is the one that
  // has a launcher.
  static aura::Window* GetPrimaryRootWindow();

  // Returns a root Window when used as a target when creating a new window.
  // The root window of the active window is used in most cases, but can
  // be overridden by using ScopedTargetRootWindow().
  // If you want to get the root Window of the active window, just use
  // |wm::GetActiveWindow()->GetRootWindow()|.
  static aura::Window* GetTargetRootWindow();

  // Returns all root windows.
  static aura::Window::Windows GetAllRootWindows();

  static aura::Window* GetContainer(aura::Window* root_window,
                                    int container_id);
  static const aura::Window* GetContainer(const aura::Window* root_window,
                                          int container_id);

  // Returns the list of containers that match |container_id| in
  // all root windows. If |priority_root| is given, the container
  // in the |priority_root| will be inserted at the top of the list.
  static std::vector<aura::Window*> GetContainersFromAllRootWindows(
      int container_id,
      aura::Window* priority_root);

  void set_target_root_window(aura::Window* target_root_window) {
    target_root_window_ = target_root_window;
  }

  // Shows the context menu for the background and launcher at
  // |location_in_screen| (in screen coordinates).
  void ShowContextMenu(const gfx::Point& location_in_screen,
                       ui::MenuSourceType source_type);

  // Shows the app list. |window| specifies in which display the app
  // list should be shown. If this is NULL, the active root window
  // will be used.
  void ShowAppList(aura::Window* anchor);

  // Dismisses the app list.
  void DismissAppList();

  // Shows the app list if it's not visible. Dismisses it otherwise.
  void ToggleAppList(aura::Window* anchor);

  // Returns app list target visibility.
  bool GetAppListTargetVisibility() const;

  // Returns app list window or NULL if it is not visible.
  aura::Window* GetAppListWindow();

  // Returns app list view or NULL if it is not visible.
  app_list::AppListView* GetAppListView();

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

  // Called after the logged-in user's profile is ready.
  void OnLoginUserProfilePrepared();

  // Called when the login status changes.
  // TODO(oshima): Investigate if we can merge this and |OnLoginStateChanged|.
  void UpdateAfterLoginStatusChange(user::LoginStatus status);

  // Called when the application is exiting.
  void OnAppTerminating();

  // Called when the screen is locked (after the lock window is visible) or
  // unlocked.
  void OnLockStateChanged(bool locked);

  // Called when a casting session is started or stopped.
  void OnCastingSessionStartedOrStopped(bool started);

  // Called when the overview mode is about to be started (before the windows
  // get re-arranged).
  void OnOverviewModeStarting();

  // Called after overview mode has ended.
  void OnOverviewModeEnded();

  // Called after maximize mode has started, windows might still animate though.
  void OnMaximizeModeStarted();

  // Called after maximize mode has ended, windows might still be returning to
  // their original position.
  void OnMaximizeModeEnded();

  // Called when a root window is created.
  void OnRootWindowAdded(aura::Window* root_window);

  // Initializes |shelf_|.  Does nothing if it's already initialized.
  void CreateShelf();

  // Called when the shelf is created for |root_window|.
  void OnShelfCreatedForRootWindow(aura::Window* root_window);

  // Creates a virtual keyboard. Deletes the old virtual keyboard if it already
  // exists.
  void CreateKeyboard();

  // Deactivates the virtual keyboard.
  void DeactivateKeyboard();

  // Show shelf view if it was created hidden (before session has started).
  void ShowShelf();

  // Adds/removes observer.
  void AddShellObserver(ShellObserver* observer);
  void RemoveShellObserver(ShellObserver* observer);

#if defined(OS_CHROMEOS)
  // Test if MaximizeModeWindowManager is not enabled, and if
  // MaximizeModeController is not currently setting a display rotation. Or if
  // the |resolution_notification_controller_| is not showing its confirmation
  // dialog. If true then changes to display settings can be saved.
  bool ShouldSaveDisplaySettings();
#endif

  AcceleratorController* accelerator_controller() {
    return accelerator_controller_.get();
  }

  DisplayManager* display_manager() { return display_manager_.get(); }
  DisplayConfigurationController* display_configuration_controller() {
    return display_configuration_controller_.get();
  }
  ::wm::CompoundEventFilter* env_filter() {
    return env_filter_.get();
  }
  views::corewm::TooltipController* tooltip_controller() {
    return tooltip_controller_.get();
  }
  OverlayEventFilter* overlay_filter() { return overlay_filter_.get(); }
  DesktopBackgroundController* desktop_background_controller() {
    return desktop_background_controller_.get();
  }
  PowerButtonController* power_button_controller() {
    return power_button_controller_.get();
  }
  LockStateController* lock_state_controller() {
    return lock_state_controller_.get();
  }
  MruWindowTracker* mru_window_tracker() {
    return mru_window_tracker_.get();
  }
  VideoDetector* video_detector() {
    return video_detector_.get();
  }
  WindowCycleController* window_cycle_controller() {
    return window_cycle_controller_.get();
  }
  WindowSelectorController* window_selector_controller() {
    return window_selector_controller_.get();
  }
  FocusCycler* focus_cycler() { return focus_cycler_.get(); }
  WindowTreeHostManager* window_tree_host_manager() {
    return window_tree_host_manager_.get();
  }
#if defined(OS_CHROMEOS)
  PowerEventObserver* power_event_observer() {
    return power_event_observer_.get();
  }
  TouchTransformerController* touch_transformer_controller() {
    return touch_transformer_controller_.get();
  }
#endif  // defined(OS_CHROMEOS)
  PartialScreenshotController* partial_screenshot_controller() {
    return partial_screenshot_controller_.get();
  }
  MouseCursorEventFilter* mouse_cursor_filter() {
    return mouse_cursor_filter_.get();
  }
  EventTransformationHandler* event_transformation_handler() {
    return event_transformation_handler_.get();
  }
  ::wm::CursorManager* cursor_manager() { return &cursor_manager_; }

  ShellDelegate* delegate() { return delegate_.get(); }

  UserWallpaperDelegate* user_wallpaper_delegate() {
    return user_wallpaper_delegate_.get();
  }

  SessionStateDelegate* session_state_delegate() {
    return session_state_delegate_.get();
  }

  AccessibilityDelegate* accessibility_delegate() {
    return accessibility_delegate_.get();
  }

  NewWindowDelegate* new_window_delegate() {
    return new_window_delegate_.get();
  }

  MediaDelegate* media_delegate() {
    return media_delegate_.get();
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

  AutoclickController* autoclick_controller() {
    return autoclick_controller_.get();
  }

  aura::client::ActivationClient* activation_client() {
    return activation_client_;
  }

  ShelfItemDelegateManager* shelf_item_delegate_manager() {
    return shelf_item_delegate_manager_.get();
  }

  base::SequencedWorkerPool* blocking_pool() {
    return blocking_pool_;
  }

  // Force the shelf to query for it's current visibility state.
  void UpdateShelfVisibility();

  // TODO(oshima): Define an interface to access shelf/launcher
  // state, or just use Launcher.

  // Sets/gets the shelf auto-hide behavior on |root_window|.
  void SetShelfAutoHideBehavior(ShelfAutoHideBehavior behavior,
                                aura::Window* root_window);
  ShelfAutoHideBehavior GetShelfAutoHideBehavior(
      aura::Window* root_window) const;

  // Sets/gets shelf's alignment on |root_window|.
  void SetShelfAlignment(ShelfAlignment alignment,
                         aura::Window* root_window);
  ShelfAlignment GetShelfAlignment(const aura::Window* root_window);

  // Notifies |observers_| when entering or exiting fullscreen mode in
  // |root_window|.
  void NotifyFullscreenStateChange(bool is_fullscreen,
                                   aura::Window* root_window);

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

  ResizeShadowController* resize_shadow_controller() {
    return resize_shadow_controller_.get();
  }

  // Made available for tests.
  ::wm::ShadowController* shadow_controller() {
    return shadow_controller_.get();
  }

  // Starts the animation that occurs on first login.
  void DoInitialWorkspaceAnimation();

  MaximizeModeController* maximize_mode_controller() {
    return maximize_mode_controller_.get();
  }

#if defined(OS_CHROMEOS)
  // TODO(oshima): Move these objects to WindowTreeHostManager.
  ui::DisplayConfigurator* display_configurator() {
    return display_configurator_.get();
  }
  DisplayErrorObserver* display_error_observer() {
    return display_error_observer_.get();
  }

  ResolutionNotificationController* resolution_notification_controller() {
    return resolution_notification_controller_.get();
  }

  LogoutConfirmationController* logout_confirmation_controller() {
    return logout_confirmation_controller_.get();
  }

  ScreenOrientationController* screen_orientation_controller() {
    return screen_orientation_controller_.get();
  }

  VirtualKeyboardController* virtual_keyboard_controller() {
    return virtual_keyboard_controller_.get();
  }
#endif  // defined(OS_CHROMEOS)

  ShelfModel* shelf_model() {
    return shelf_model_.get();
  }

  WindowPositioner* window_positioner() {
    return window_positioner_.get();
  }

  // Returns the launcher delegate, creating if necesary.
  ShelfDelegate* GetShelfDelegate();

  UserMetricsRecorder* metrics() {
    return user_metrics_recorder_.get();
  }

  void SetTouchHudProjectionEnabled(bool enabled);

  bool is_touch_hud_projection_enabled() const {
    return is_touch_hud_projection_enabled_;
  }

#if defined(OS_CHROMEOS)
  // Creates instance of FirstRunHelper. Caller is responsible for deleting
  // returned object.
  ash::FirstRunHelper* CreateFirstRunHelper();

  // Toggles cursor compositing on/off. Native cursor is disabled when cursor
  // compositing is enabled, and vice versa.
  void SetCursorCompositingEnabled(bool enabled);

  StickyKeysController* sticky_keys_controller() {
    return sticky_keys_controller_.get();
  }
#endif  // defined(OS_CHROMEOS)

  GPUSupport* gpu_support() { return gpu_support_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtendedDesktopTest, TestCursor);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, MouseEventCursors);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, TransformActivate);
  friend class RootWindowController;
  friend class ScopedTargetRootWindow;
  friend class test::ShellTestApi;
  friend class shell::WindowWatcher;

  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  // Takes ownership of |delegate|.
  Shell(ShellDelegate* delegate, base::SequencedWorkerPool* blocking_pool);
  ~Shell() override;

  void Init(const ShellInitParams& init_params);

  // Initializes virtual keyboard controller.
  void InitKeyboard();

  // Initializes the root window so that it can host browser windows.
  void InitRootWindow(aura::Window* root_window);

  // Hides the shelf view if any are visible.
  void HideShelf();

  // ash::SystemModalContainerEventFilterDelegate overrides:
  bool CanWindowReceiveEvents(aura::Window* window) override;

  // Overridden from ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override;
  EventTarget* GetParentTarget() override;
  scoped_ptr<ui::EventTargetIterator> GetChildIterator() const override;
  ui::EventTargeter* GetEventTargeter() override;

  // Overridden from aura::client::ActivationChangeObserver:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  static Shell* instance_;

  // If set before the Shell is initialized, the mouse cursor will be hidden
  // when the screen is initially created.
  static bool initially_hide_cursor_;

  // When no explicit target display/RootWindow is given, new windows are
  // created on |scoped_target_root_window_| , unless NULL in
  // which case they are created on |target_root_window_|.
  // |target_root_window_| never becomes NULL during the session.
  aura::Window* target_root_window_;
  aura::Window* scoped_target_root_window_;

  // The CompoundEventFilter owned by aura::Env object.
  scoped_ptr< ::wm::CompoundEventFilter> env_filter_;

  std::vector<WindowAndBoundsPair> to_restore_;

  scoped_ptr<UserMetricsRecorder> user_metrics_recorder_;
  scoped_ptr< ::wm::NestedAcceleratorController> nested_accelerator_controller_;
  scoped_ptr<AcceleratorController> accelerator_controller_;
  scoped_ptr<ShellDelegate> delegate_;
  scoped_ptr<SystemTrayDelegate> system_tray_delegate_;
  scoped_ptr<SystemTrayNotifier> system_tray_notifier_;
  scoped_ptr<UserWallpaperDelegate> user_wallpaper_delegate_;
  scoped_ptr<SessionStateDelegate> session_state_delegate_;
  scoped_ptr<AccessibilityDelegate> accessibility_delegate_;
  scoped_ptr<NewWindowDelegate> new_window_delegate_;
  scoped_ptr<MediaDelegate> media_delegate_;
  scoped_ptr<ShelfDelegate> shelf_delegate_;
  scoped_ptr<ShelfItemDelegateManager> shelf_item_delegate_manager_;
  scoped_ptr<ShelfWindowWatcher> shelf_window_watcher_;

  scoped_ptr<ShelfModel> shelf_model_;
  scoped_ptr<WindowPositioner> window_positioner_;

  scoped_ptr<AppListController> app_list_controller_;

  scoped_ptr<DragDropController> drag_drop_controller_;
  scoped_ptr<ResizeShadowController> resize_shadow_controller_;
  scoped_ptr< ::wm::ShadowController> shadow_controller_;
  scoped_ptr< ::wm::VisibilityController> visibility_controller_;
  scoped_ptr< ::wm::WindowModalityController> window_modality_controller_;
  scoped_ptr<views::corewm::TooltipController> tooltip_controller_;
  scoped_ptr<DesktopBackgroundController> desktop_background_controller_;
  scoped_ptr<PowerButtonController> power_button_controller_;
  scoped_ptr<LockStateController> lock_state_controller_;
  scoped_ptr<MruWindowTracker> mru_window_tracker_;
  scoped_ptr<ui::UserActivityDetector> user_activity_detector_;
  scoped_ptr<VideoDetector> video_detector_;
  scoped_ptr<WindowCycleController> window_cycle_controller_;
  scoped_ptr<WindowSelectorController> window_selector_controller_;
  scoped_ptr<FocusCycler> focus_cycler_;
  scoped_ptr<WindowTreeHostManager> window_tree_host_manager_;
  scoped_ptr<HighContrastController> high_contrast_controller_;
  scoped_ptr<MagnificationController> magnification_controller_;
  scoped_ptr<PartialMagnificationController> partial_magnification_controller_;
  scoped_ptr<AutoclickController> autoclick_controller_;
  scoped_ptr<aura::client::FocusClient> focus_client_;
  aura::client::ActivationClient* activation_client_;
  scoped_ptr<PartialScreenshotController> partial_screenshot_controller_;

  scoped_ptr<MouseCursorEventFilter> mouse_cursor_filter_;
  scoped_ptr<ScreenPositionController> screen_position_controller_;
  scoped_ptr<SystemModalContainerEventFilter> modality_filter_;
  scoped_ptr<EventClientImpl> event_client_;
  scoped_ptr<EventTransformationHandler> event_transformation_handler_;

  // An event filter that pre-handles key events while the partial
  // screenshot UI or the keyboard overlay is active.
  scoped_ptr<OverlayEventFilter> overlay_filter_;

  // An event filter for logging keyboard-related metrics.
  scoped_ptr<KeyboardUMAEventFilter> keyboard_metrics_filter_;

  // An event filter which handles moving and resizing windows.
  scoped_ptr<ToplevelWindowEventHandler> toplevel_window_event_handler_;

  // An event filter which handles system level gestures
  scoped_ptr<SystemGestureEventFilter> system_gesture_filter_;

  // An event filter that pre-handles global accelerators.
  scoped_ptr< ::wm::AcceleratorFilter> accelerator_filter_;

  scoped_ptr<DisplayManager> display_manager_;
  scoped_ptr<DisplayConfigurationController> display_configuration_controller_;

  scoped_ptr<LocaleNotificationController> locale_notification_controller_;

#if defined(OS_CHROMEOS)
  scoped_ptr<PowerEventObserver> power_event_observer_;
  scoped_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;
  scoped_ptr<VideoActivityNotifier> video_activity_notifier_;
  scoped_ptr<StickyKeysController> sticky_keys_controller_;
  scoped_ptr<ResolutionNotificationController>
      resolution_notification_controller_;
  scoped_ptr<BluetoothNotificationController>
      bluetooth_notification_controller_;
  scoped_ptr<LogoutConfirmationController> logout_confirmation_controller_;
  scoped_ptr<LastWindowClosedLogoutReminder>
      last_window_closed_logout_reminder_;
  scoped_ptr<VirtualKeyboardController> virtual_keyboard_controller_;
  // Controls video output device state.
  scoped_ptr<ui::DisplayConfigurator> display_configurator_;
  scoped_ptr<DisplayColorManager> display_color_manager_;
  scoped_ptr<DisplayErrorObserver> display_error_observer_;
  scoped_ptr<ProjectingObserver> projecting_observer_;

  // Listens for output changes and updates the display manager.
  scoped_ptr<DisplayChangeObserver> display_change_observer_;

  // Implements content::ScreenOrientationController for ChromeOS
  scoped_ptr<ScreenOrientationController> screen_orientation_controller_;

  scoped_ptr<TouchTransformerController> touch_transformer_controller_;

  scoped_ptr<ui::EventHandler> magnifier_key_scroll_handler_;
  scoped_ptr<ui::EventHandler> speech_feedback_handler_;
#endif  // defined(OS_CHROMEOS)

  scoped_ptr<MaximizeModeController> maximize_mode_controller_;

  // |native_cursor_manager_| is owned by |cursor_manager_|, but we keep a
  // pointer to vend to test code.
  AshNativeCursorManager* native_cursor_manager_;

// Cursor may be hidden on certain key events in ChromeOS, whereas we never hide
// the cursor on Windows.
#if defined(OS_CHROMEOS)
  CursorManager cursor_manager_;
#else  // !defined(OS_CHROMEOS)
  ::wm::CursorManager cursor_manager_;
#endif  // defined(OS_CHROMEOS)

  base::ObserverList<ShellObserver> observers_;

  // For testing only: simulate that a modal window is open
  bool simulate_modal_window_open_for_testing_;

  bool is_touch_hud_projection_enabled_;

  // Injected content::GPUDataManager support.
  scoped_ptr<GPUSupport> gpu_support_;

  base::SequencedWorkerPool* blocking_pool_;

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace ash

#endif  // ASH_SHELL_H_
