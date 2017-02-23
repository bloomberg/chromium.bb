// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_H_
#define ASH_SHELL_H_

#include <memory>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/wm/cursor_manager_chromeos.h"
#include "ash/wm/system_modal_container_event_filter_delegate.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/event_target.h"
#include "ui/wm/core/cursor_manager.h"

namespace aura {
class RootWindow;
class UserActivityForwarder;
class Window;
class WindowManagerClient;
class WindowTreeClient;
namespace client {
class ActivationClient;
class FocusClient;
}
}

namespace chromeos {
class AudioA11yController;
}

namespace display {
class DisplayChangeObserver;
class DisplayConfigurator;
class DisplayManager;
}

namespace gfx {
class Insets;
}

namespace ui {
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
class ShadowController;
class VisibilityController;
class WindowModalityController;
}

namespace ash {

class AcceleratorControllerDelegateAura;
class AppListDelegateImpl;
class AshNativeCursorManager;
class AutoclickController;
class BluetoothNotificationController;
class DisplayColorManager;
class DisplayConfigurationController;
class DisplayErrorObserver;
class DragDropController;
class EventClientImpl;
class EventTransformationHandler;
class FirstRunHelper;
class GPUSupport;
class HighContrastController;
class ImmersiveHandlerFactoryAsh;
class LaserPointerController;
class LinkHandlerModelFactory;
class LockStateController;
enum class LoginStatus;
class MagnificationController;
class MouseCursorEventFilter;
class OverlayEventFilter;
class PartialMagnificationController;
class PowerButtonController;
class PowerEventObserver;
class ProjectingObserver;
class ResizeShadowController;
class ResolutionNotificationController;
class RootWindowController;
class ScopedOverviewAnimationSettingsFactoryAura;
class ScreenOrientationController;
class ScreenshotController;
class ScreenPinningController;
class ScreenPositionController;
class SessionStateDelegate;
struct ShellInitParams;
class ShutdownObserver;
class SmsObserver;
class StickyKeysController;
class SystemGestureEventFilter;
class SystemModalContainerEventFilter;
class SystemTray;
class ToplevelWindowEventHandler;
class AshTouchTransformController;
class ScreenLayoutObserver;
class VirtualKeyboardController;
class VideoActivityNotifier;
class VideoDetector;
class WebNotificationTray;
class WindowPositioner;
class WindowTreeHostManager;
class WmShell;
class WmWindow;

namespace shell {
class WindowWatcher;
}

namespace test {
class ShellTestApi;
class SmsObserverTest;
}

// Shell is a singleton object that presents the Shell API and implements the
// RootWindow's delegate interface.
//
// Upon creation, the Shell sets itself as the RootWindow's delegate, which
// takes ownership of the Shell.
class ASH_EXPORT Shell : public SystemModalContainerEventFilterDelegate,
                         public ui::EventTarget {
 public:
  typedef std::vector<RootWindowController*> RootWindowControllerList;

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
  // be overridden by using ScopedRootWindowForNewWindows.
  // If you want to get the root Window of the active window, just use
  // |wm::GetActiveWindow()->GetRootWindow()|.
  static aura::Window* GetTargetRootWindow();

  // Returns the id of the display::Display corresponding to the window returned
  // by |GetTargetRootWindow()|
  static int64_t GetTargetDisplayId();

  // Returns all root windows.
  static aura::Window::Windows GetAllRootWindows();

  static aura::Window* GetContainer(aura::Window* root_window,
                                    int container_id);
  static const aura::Window* GetContainer(const aura::Window* root_window,
                                          int container_id);

  // TODO(sky): move this and WindowManagerClient into ShellMash that is owned
  // by Shell. Doing the move is gated on having mash create Shell.
  static void set_window_tree_client(aura::WindowTreeClient* client) {
    window_tree_client_ = client;
  }

  static aura::WindowTreeClient* window_tree_client() {
    return window_tree_client_;
  }

  static void set_window_manager_client(aura::WindowManagerClient* client) {
    window_manager_client_ = client;
  }
  static aura::WindowManagerClient* window_manager_client() {
    return window_manager_client_;
  }

  // Creates a default views::NonClientFrameView for use by windows in the
  // Ash environment.
  views::NonClientFrameView* CreateDefaultNonClientFrameView(
      views::Widget* widget);

  // Sets work area insets of the display containing |window|, pings observers.
  void SetDisplayWorkAreaInsets(aura::Window* window,
                                const gfx::Insets& insets);

  // Called when the user logs in.
  void OnLoginStateChanged(LoginStatus status);

  // Called when the application is exiting.
  void OnAppTerminating();

  // Called when the screen is locked (after the lock window is visible) or
  // unlocked.
  void OnLockStateChanged(bool locked);

  // Called when a casting session is started or stopped.
  void OnCastingSessionStartedOrStopped(bool started);

  // Called when a root window is created.
  void OnRootWindowAdded(WmWindow* root_window);

  // Creates a virtual keyboard. Deletes the old virtual keyboard if it already
  // exists.
  void CreateKeyboard();

  // Deactivates the virtual keyboard.
  void DeactivateKeyboard();

  // Test if MaximizeModeWindowManager is not enabled, and if
  // MaximizeModeController is not currently setting a display rotation. Or if
  // the |resolution_notification_controller_| is not showing its confirmation
  // dialog. If true then changes to display settings can be saved.
  bool ShouldSaveDisplaySettings();

  AcceleratorControllerDelegateAura* accelerator_controller_delegate() {
    return accelerator_controller_delegate_.get();
  }

  display::DisplayManager* display_manager() { return display_manager_.get(); }
  DisplayConfigurationController* display_configuration_controller() {
    return display_configuration_controller_.get();
  }
  ::wm::CompoundEventFilter* env_filter() { return env_filter_.get(); }
  views::corewm::TooltipController* tooltip_controller() {
    return tooltip_controller_.get();
  }
  OverlayEventFilter* overlay_filter() { return overlay_filter_.get(); }
  LinkHandlerModelFactory* link_handler_model_factory() {
    return link_handler_model_factory_;
  }
  void set_link_handler_model_factory(
      LinkHandlerModelFactory* link_handler_model_factory) {
    link_handler_model_factory_ = link_handler_model_factory;
  }
  PowerButtonController* power_button_controller() {
    return power_button_controller_.get();
  }
  LockStateController* lock_state_controller() {
    return lock_state_controller_.get();
  }
  VideoDetector* video_detector() { return video_detector_.get(); }
  WindowTreeHostManager* window_tree_host_manager() {
    return window_tree_host_manager_.get();
  }
  PowerEventObserver* power_event_observer() {
    return power_event_observer_.get();
  }
  AshTouchTransformController* touch_transformer_controller() {
    return touch_transformer_controller_.get();
  }
  LaserPointerController* laser_pointer_controller() {
    return laser_pointer_controller_.get();
  }
  PartialMagnificationController* partial_magnification_controller() {
    return partial_magnification_controller_.get();
  }
  ScreenshotController* screenshot_controller() {
    return screenshot_controller_.get();
  }
  MouseCursorEventFilter* mouse_cursor_filter() {
    return mouse_cursor_filter_.get();
  }
  EventTransformationHandler* event_transformation_handler() {
    return event_transformation_handler_.get();
  }
  ::wm::CursorManager* cursor_manager() { return cursor_manager_.get(); }

  SessionStateDelegate* session_state_delegate() {
    return session_state_delegate_.get();
  }

  HighContrastController* high_contrast_controller() {
    return high_contrast_controller_.get();
  }

  MagnificationController* magnification_controller() {
    return magnification_controller_.get();
  }

  AutoclickController* autoclick_controller() {
    return autoclick_controller_.get();
  }

  aura::client::ActivationClient* activation_client() {
    return activation_client_;
  }

  // Force the shelf to query for it's current visibility state.
  // TODO(jamescook): Move to Shelf.
  void UpdateShelfVisibility();

  // Returns WebNotificationTray on the primary root window.
  WebNotificationTray* GetWebNotificationTray();

  // Does the primary display have status area?
  bool HasPrimaryStatusArea();

  // Returns the system tray on primary display.
  SystemTray* GetPrimarySystemTray();

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

  // TODO(oshima): Move these objects to WindowTreeHostManager.
  display::DisplayConfigurator* display_configurator() {
    return display_configurator_.get();
  }
  DisplayErrorObserver* display_error_observer() {
    return display_error_observer_.get();
  }

  ScreenLayoutObserver* screen_layout_observer() {
    return screen_layout_observer_.get();
  }

  ResolutionNotificationController* resolution_notification_controller() {
    return resolution_notification_controller_.get();
  }

  ScreenOrientationController* screen_orientation_controller() {
    return screen_orientation_controller_.get();
  }

  VirtualKeyboardController* virtual_keyboard_controller() {
    return virtual_keyboard_controller_.get();
  }

  chromeos::AudioA11yController* audio_a11y_controller() {
    return audio_a11y_controller_.get();
  }

  WindowPositioner* window_positioner() { return window_positioner_.get(); }

  UserMetricsRecorder* metrics() { return user_metrics_recorder_.get(); }

  void SetTouchHudProjectionEnabled(bool enabled);

  bool is_touch_hud_projection_enabled() const {
    return is_touch_hud_projection_enabled_;
  }

  // Creates instance of FirstRunHelper. Caller is responsible for deleting
  // returned object.
  ash::FirstRunHelper* CreateFirstRunHelper();

  void SetLargeCursorSizeInDip(int large_cursor_size_in_dip);

  // Toggles cursor compositing on/off. Native cursor is disabled when cursor
  // compositing is enabled, and vice versa.
  void SetCursorCompositingEnabled(bool enabled);

  StickyKeysController* sticky_keys_controller() {
    return sticky_keys_controller_.get();
  }

  ScreenPinningController* screen_pinning_controller() {
    return screen_pinning_controller_.get();
  }

  GPUSupport* gpu_support() { return gpu_support_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtendedDesktopTest, TestCursor);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, MouseEventCursors);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, TransformActivate);
  friend class RootWindowController;
  friend class test::ShellTestApi;
  friend class shell::WindowWatcher;
  friend class SmsObserverTest;

  explicit Shell(std::unique_ptr<WmShell> wm_shell);
  ~Shell() override;

  void Init(const ShellInitParams& init_params);

  // Initializes virtual keyboard controller.
  void InitKeyboard();

  // Initializes the root window so that it can host browser windows.
  void InitRootWindow(aura::Window* root_window);

  // Destroys all child windows including widgets across all roots.
  void CloseAllRootWindowChildWindows();

  // SystemModalContainerEventFilterDelegate:
  bool CanWindowReceiveEvents(aura::Window* window) override;

  // Overridden from ui::EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override;
  EventTarget* GetParentTarget() override;
  std::unique_ptr<ui::EventTargetIterator> GetChildIterator() const override;
  ui::EventTargeter* GetEventTargeter() override;

  static Shell* instance_;

  // Only valid in mash, for classic ash this is null.
  static aura::WindowTreeClient* window_tree_client_;
  static aura::WindowManagerClient* window_manager_client_;

  // If set before the Shell is initialized, the mouse cursor will be hidden
  // when the screen is initially created.
  static bool initially_hide_cursor_;

  std::unique_ptr<ScopedOverviewAnimationSettingsFactoryAura>
      scoped_overview_animation_settings_factory_;
  std::unique_ptr<WmShell> wm_shell_;

  // The CompoundEventFilter owned by aura::Env object.
  std::unique_ptr<::wm::CompoundEventFilter> env_filter_;

  std::unique_ptr<UserMetricsRecorder> user_metrics_recorder_;
  std::unique_ptr<AcceleratorControllerDelegateAura>
      accelerator_controller_delegate_;
  std::unique_ptr<SessionStateDelegate> session_state_delegate_;
  std::unique_ptr<WindowPositioner> window_positioner_;

  std::unique_ptr<DragDropController> drag_drop_controller_;
  std::unique_ptr<ResizeShadowController> resize_shadow_controller_;
  std::unique_ptr<::wm::ShadowController> shadow_controller_;
  std::unique_ptr<::wm::VisibilityController> visibility_controller_;
  std::unique_ptr<::wm::WindowModalityController> window_modality_controller_;
  std::unique_ptr<views::corewm::TooltipController> tooltip_controller_;
  LinkHandlerModelFactory* link_handler_model_factory_;
  std::unique_ptr<PowerButtonController> power_button_controller_;
  std::unique_ptr<LockStateController> lock_state_controller_;
  std::unique_ptr<ui::UserActivityDetector> user_activity_detector_;
  std::unique_ptr<VideoDetector> video_detector_;
  std::unique_ptr<WindowTreeHostManager> window_tree_host_manager_;
  std::unique_ptr<HighContrastController> high_contrast_controller_;
  std::unique_ptr<MagnificationController> magnification_controller_;
  std::unique_ptr<AutoclickController> autoclick_controller_;
  std::unique_ptr<aura::client::FocusClient> focus_client_;

  aura::client::ActivationClient* activation_client_;

  std::unique_ptr<ScreenshotController> screenshot_controller_;

  std::unique_ptr<MouseCursorEventFilter> mouse_cursor_filter_;
  std::unique_ptr<ScreenPositionController> screen_position_controller_;
  std::unique_ptr<SystemModalContainerEventFilter> modality_filter_;
  std::unique_ptr<EventClientImpl> event_client_;
  std::unique_ptr<EventTransformationHandler> event_transformation_handler_;

  // An event filter that pre-handles key events while the partial
  // screenshot UI or the keyboard overlay is active.
  std::unique_ptr<OverlayEventFilter> overlay_filter_;

  // An event filter which handles moving and resizing windows.
  std::unique_ptr<ToplevelWindowEventHandler> toplevel_window_event_handler_;

  // An event filter which handles system level gestures
  std::unique_ptr<SystemGestureEventFilter> system_gesture_filter_;

  // An event filter that pre-handles global accelerators.
  std::unique_ptr<::wm::AcceleratorFilter> accelerator_filter_;

  std::unique_ptr<display::DisplayManager> display_manager_;
  std::unique_ptr<DisplayConfigurationController>
      display_configuration_controller_;

  std::unique_ptr<ScreenPinningController> screen_pinning_controller_;

  // Forwards user activity ui::mojom::UserActivityMonitor to
  // |user_activity_detector_|. Only initialized for mash.
  std::unique_ptr<aura::UserActivityForwarder> user_activity_forwarder_;

  std::unique_ptr<PowerEventObserver> power_event_observer_;
  std::unique_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;
  std::unique_ptr<VideoActivityNotifier> video_activity_notifier_;
  std::unique_ptr<StickyKeysController> sticky_keys_controller_;
  std::unique_ptr<ResolutionNotificationController>
      resolution_notification_controller_;
  std::unique_ptr<BluetoothNotificationController>
      bluetooth_notification_controller_;
  std::unique_ptr<VirtualKeyboardController> virtual_keyboard_controller_;
  std::unique_ptr<chromeos::AudioA11yController> audio_a11y_controller_;
  // Controls video output device state.
  std::unique_ptr<display::DisplayConfigurator> display_configurator_;
  std::unique_ptr<DisplayColorManager> display_color_manager_;
  std::unique_ptr<DisplayErrorObserver> display_error_observer_;
  std::unique_ptr<ProjectingObserver> projecting_observer_;

  // Listens for output changes and updates the display manager.
  std::unique_ptr<display::DisplayChangeObserver> display_change_observer_;

  // Listens for shutdown and updates DisplayConfigurator.
  std::unique_ptr<ShutdownObserver> shutdown_observer_;

  // Listens for new sms messages and shows notifications.
  std::unique_ptr<SmsObserver> sms_observer_;

  // Implements content::ScreenOrientationController for Chrome OS.
  std::unique_ptr<ScreenOrientationController> screen_orientation_controller_;
  std::unique_ptr<ScreenLayoutObserver> screen_layout_observer_;

  std::unique_ptr<AshTouchTransformController> touch_transformer_controller_;

  std::unique_ptr<ui::EventHandler> magnifier_key_scroll_handler_;
  std::unique_ptr<ui::EventHandler> speech_feedback_handler_;
  std::unique_ptr<LaserPointerController> laser_pointer_controller_;
  std::unique_ptr<PartialMagnificationController>
      partial_magnification_controller_;

  // |native_cursor_manager_| is owned by |cursor_manager_|, but we keep a
  // pointer to vend to test code.
  AshNativeCursorManager* native_cursor_manager_;

  // Cursor may be hidden on certain key events in Chrome OS, whereas we never
  // hide the cursor on Windows.
  std::unique_ptr<::wm::CursorManager> cursor_manager_;

  // For testing only: simulate that a modal window is open
  bool simulate_modal_window_open_for_testing_;

  bool is_touch_hud_projection_enabled_;

  // Injected content::GPUDataManager support.
  std::unique_ptr<GPUSupport> gpu_support_;

  std::unique_ptr<ImmersiveHandlerFactoryAsh> immersive_handler_factory_;

  std::unique_ptr<AppListDelegateImpl> app_list_delegate_impl_;

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace ash

#endif  // ASH_SHELL_H_
