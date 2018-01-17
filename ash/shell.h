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
#include "ash/session/session_observer.h"
#include "ash/wm/cursor_manager_chromeos.h"
#include "ash/wm/system_modal_container_event_filter_delegate.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/event_target.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/public/activation_change_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace aura {
class RootWindow;
class UserActivityForwarder;
class Window;
class WindowManagerClient;
class WindowTreeClient;
}  // namespace aura

namespace app_list {
class AppList;
}  // namespace app_list

namespace display {
class DisplayChangeObserver;
class DisplayConfigurator;
class DisplayManager;
}  // namespace display

namespace gfx {
class Insets;
}

namespace ui {
class ContextFactory;
class ContextFactoryPrivate;
class UserActivityDetector;
class UserActivityPowerManagerNotifier;
}  // namespace ui

namespace views {
class NonClientFrameView;
class Widget;
namespace corewm {
class TooltipController;
}
}  // namespace views

namespace wm {
class AcceleratorFilter;
class ActivationClient;
class CompoundEventFilter;
class FocusController;
class ShadowController;
class VisibilityController;
class WindowModalityController;
}  // namespace wm

namespace ash {

class AcceleratorController;
class AccessibilityController;
class AccessibilityDelegate;
class AshDisplayController;
class AppListDelegateImpl;
class NativeCursorManagerAsh;
class AshTouchTransformController;
class AutoclickController;
class BacklightsForcedOffSetter;
class BluetoothNotificationController;
class BluetoothPowerController;
class BrightnessControlDelegate;
class CastConfigController;
class DisplayColorManager;
class DisplayConfigurationController;
class DisplayErrorObserver;
class DisplayShutdownObserver;
class DragDropController;
class EventClientImpl;
class EventTransformationHandler;
class FirstRunHelper;
class FocusCycler;
class HighContrastController;
class HighlighterController;
class ImeController;
class ImmersiveContextAsh;
class ImmersiveHandlerFactoryAsh;
class KeyboardBrightnessControlDelegate;
class KeyboardUI;
class LaserPointerController;
class LocaleNotificationController;
class LockStateController;
class LogoutConfirmationController;
class LoginScreenController;
class MagnificationController;
class TabletModeController;
class MediaController;
class MessageCenterController;
class MouseCursorEventFilter;
class MruWindowTracker;
class NewWindowController;
class NightLightController;
class NoteTakingController;
class OverlayEventFilter;
class PartialMagnificationController;
class PeripheralBatteryNotifier;
class PowerButtonController;
class PowerEventObserver;
class ProjectingObserver;
class ResizeShadowController;
class ResolutionNotificationController;
class RootWindowController;
class ShellPort;
class ScreenLayoutObserver;
class ScreenOrientationController;
class ScreenshotController;
class ScreenPinningController;
class ScreenPositionController;
class SessionController;
class ShelfController;
class ShelfModel;
class ShelfWindowWatcher;
class ShellDelegate;
struct ShellInitParams;
class ShellObserver;
class ShutdownController;
class SmsObserver;
class SplitViewController;
class StickyKeysController;
class SystemGestureEventFilter;
class SystemModalContainerEventFilter;
class SystemTray;
class SystemTrayController;
class SystemTrayNotifier;
class TimeToFirstPresentRecorder;
class ToplevelWindowEventHandler;
class ToastManager;
class TouchDevicesController;
class TrayAction;
class TrayBluetoothHelper;
class VirtualKeyboardController;
class VideoActivityNotifier;
class VideoDetector;
class VoiceInteractionController;
class VpnList;
class WallpaperController;
class WallpaperDelegate;
class WebNotificationTray;
class WindowCycleController;
class WindowPositioner;
class WindowSelectorController;
class WindowTreeHostManager;

enum class Config;
enum class LoginStatus;

// Shell is a singleton object that presents the Shell API and implements the
// RootWindow's delegate interface.
//
// Upon creation, the Shell sets itself as the RootWindow's delegate, which
// takes ownership of the Shell.
class ASH_EXPORT Shell : public SessionObserver,
                         public SystemModalContainerEventFilterDelegate,
                         public ui::EventTarget,
                         public ::wm::ActivationChangeObserver {
 public:
  typedef std::vector<RootWindowController*> RootWindowControllerList;

  // Creates the single Shell instance.
  static Shell* CreateInstance(ShellInitParams init_params);

  // Should never be called before |CreateInstance()|.
  static Shell* Get();

  // Returns true if the ash shell has been instantiated.
  static bool HasInstance();

  static void DeleteInstance();

  // Returns the root window controller for the primary root window.
  // TODO(oshima): move this to |RootWindowController|
  static RootWindowController* GetPrimaryRootWindowController();

  // Returns the RootWindowController for the given display id. If there
  // is no display for |display_id|, null is returned.
  static RootWindowController* GetRootWindowControllerWithDisplayId(
      int64_t display_id);

  // Returns the root Window for the given display id. If there is no display
  // for |display_id| null is returned.
  static aura::Window* GetRootWindowForDisplayId(int64_t display_id);

  // Returns all root window controllers.
  // TODO(oshima): move this to |RootWindowController|
  static RootWindowControllerList GetAllRootWindowControllers();

  // Returns the primary root Window. The primary root Window is the one that
  // has a launcher.
  static aura::Window* GetPrimaryRootWindow();

  // Returns the root window that newly created windows should be added to.
  // Value can be temporarily overridden using ScopedRootWindowForNewWindows.
  // NOTE: this returns the root, newly created window should be added to the
  // appropriate container in the returned window.
  static aura::Window* GetRootWindowForNewWindows();

  // Returns all root windows.
  static aura::Window::Windows GetAllRootWindows();

  static aura::Window* GetContainer(aura::Window* root_window,
                                    int container_id);
  static const aura::Window* GetContainer(const aura::Window* root_window,
                                          int container_id);

  // If a system-modal dialog window is currently open, returns the ID of the
  // system modal window container that contains the window.
  // If no system-modal dialogs are open it returns -1.
  static int GetOpenSystemModalWindowContainerId();

  // Returns true if a system-modal dialog window is currently open.
  static bool IsSystemModalWindowOpen();

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

  static Config GetAshConfig();
  static bool ShouldUseIMEService();

  // Registers all ash related local state prefs to the given |registry|.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Registers all ash related user profile prefs to the given |registry|.
  // Can be called before Shell is initialized. When |for_test| is true this
  // registers foreign user profile prefs (e.g. chrome prefs) as if they are
  // owned by ash. This allows test code to read the pref values.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry,
                                   bool for_test = false);

  // Creates a default views::NonClientFrameView for use by windows in the
  // Ash environment.
  views::NonClientFrameView* CreateDefaultNonClientFrameView(
      views::Widget* widget);

  // Sets work area insets of the display containing |window|, pings observers.
  void SetDisplayWorkAreaInsets(aura::Window* window,
                                const gfx::Insets& insets);

  // Called when a casting session is started or stopped.
  void OnCastingSessionStartedOrStopped(bool started);

  // Called when a root window is created.
  void OnRootWindowAdded(aura::Window* root_window);

  // Creates a keyboard controller and associate it with the primary root window
  // controller. Destroys the old keyboard controller if it already exists.
  void CreateKeyboard();

  // Destroys the virtual keyboard.
  void DestroyKeyboard();

  // Test if TabletModeWindowManager is not enabled, and if
  // TabletModeController is not currently setting a display rotation. Or if
  // the |resolution_notification_controller_| is not showing its confirmation
  // dialog. If true then changes to display settings can be saved.
  bool ShouldSaveDisplaySettings();

  AcceleratorController* accelerator_controller() {
    return accelerator_controller_.get();
  }
  AccessibilityController* accessibility_controller() {
    return accessibility_controller_.get();
  }
  AccessibilityDelegate* accessibility_delegate() {
    return accessibility_delegate_.get();
  }
  ::wm::ActivationClient* activation_client();
  app_list::AppList* app_list() { return app_list_.get(); }
  AshDisplayController* ash_display_controller() {
    return ash_display_controller_.get();
  }
  AutoclickController* autoclick_controller() {
    return autoclick_controller_.get();
  }
  BacklightsForcedOffSetter* backlights_forced_off_setter() {
    return backlights_forced_off_setter_.get();
  }
  BluetoothPowerController* bluetooth_power_controller() {
    return bluetooth_power_controller_.get();
  }
  BrightnessControlDelegate* brightness_control_delegate() {
    return brightness_control_delegate_.get();
  }
  CastConfigController* cast_config() { return cast_config_.get(); }

  // Returns nullptr in mash which has no global cursor manager.
  ::wm::CursorManager* cursor_manager() { return cursor_manager_.get(); }

  display::DisplayManager* display_manager() { return display_manager_.get(); }
  DisplayConfigurationController* display_configuration_controller() {
    return display_configuration_controller_.get();
  }

  // TODO(oshima): Move these objects to WindowTreeHostManager.
  display::DisplayConfigurator* display_configurator() {
    return display_configurator_.get();
  }
  DisplayErrorObserver* display_error_observer() {
    return display_error_observer_.get();
  }

  ::wm::CompoundEventFilter* env_filter() { return env_filter_.get(); }
  EventClientImpl* event_client() { return event_client_.get(); }
  EventTransformationHandler* event_transformation_handler() {
    return event_transformation_handler_.get();
  }
  FocusCycler* focus_cycler() { return focus_cycler_.get(); }
  HighlighterController* highlighter_controller() {
    return highlighter_controller_.get();
  }
  HighContrastController* high_contrast_controller() {
    return high_contrast_controller_.get();
  }
  ImeController* ime_controller() { return ime_controller_.get(); }
  KeyboardBrightnessControlDelegate* keyboard_brightness_control_delegate() {
    return keyboard_brightness_control_delegate_.get();
  }
  KeyboardUI* keyboard_ui() { return keyboard_ui_.get(); }
  LaserPointerController* laser_pointer_controller() {
    return laser_pointer_controller_.get();
  }
  LocaleNotificationController* locale_notification_controller() {
    return locale_notification_controller_.get();
  }
  LoginScreenController* login_screen_controller() {
    return login_screen_controller_.get();
  }
  LockStateController* lock_state_controller() {
    return lock_state_controller_.get();
  }
  LogoutConfirmationController* logout_confirmation_controller() {
    return logout_confirmation_controller_.get();
  }
  MagnificationController* magnification_controller() {
    return magnification_controller_.get();
  }
  MediaController* media_controller() { return media_controller_.get(); }
  MessageCenterController* message_center_controller() {
    return message_center_controller_.get();
  }
  MouseCursorEventFilter* mouse_cursor_filter() {
    return mouse_cursor_filter_.get();
  }
  MruWindowTracker* mru_window_tracker() { return mru_window_tracker_.get(); }
  NewWindowController* new_window_controller() {
    return new_window_controller_.get();
  }
  NightLightController* night_light_controller();
  NoteTakingController* note_taking_controller() {
    return note_taking_controller_.get();
  }
  OverlayEventFilter* overlay_filter() { return overlay_filter_.get(); }
  PartialMagnificationController* partial_magnification_controller() {
    return partial_magnification_controller_.get();
  }
  PowerButtonController* power_button_controller() {
    return power_button_controller_.get();
  }
  PowerEventObserver* power_event_observer() {
    return power_event_observer_.get();
  }
  ResizeShadowController* resize_shadow_controller() {
    return resize_shadow_controller_.get();
  }
  ResolutionNotificationController* resolution_notification_controller() {
    return resolution_notification_controller_.get();
  }
  ScreenshotController* screenshot_controller() {
    return screenshot_controller_.get();
  }
  ScreenLayoutObserver* screen_layout_observer() {
    return screen_layout_observer_.get();
  }
  ScreenOrientationController* screen_orientation_controller() {
    return screen_orientation_controller_.get();
  }
  ScreenPinningController* screen_pinning_controller() {
    return screen_pinning_controller_.get();
  }
  SessionController* session_controller() { return session_controller_.get(); }
  ShelfController* shelf_controller() { return shelf_controller_.get(); }
  ShelfModel* shelf_model();
  ShellDelegate* shell_delegate() { return shell_delegate_.get(); }
  ShutdownController* shutdown_controller() {
    return shutdown_controller_.get();
  }
  SplitViewController* split_view_controller() {
    return split_view_controller_.get();
  }
  StickyKeysController* sticky_keys_controller() {
    return sticky_keys_controller_.get();
  }
  SystemTrayController* system_tray_controller() {
    return system_tray_controller_.get();
  }
  SystemTrayNotifier* system_tray_notifier() {
    return system_tray_notifier_.get();
  }
  TabletModeController* tablet_mode_controller() {
    return tablet_mode_controller_.get();
  }
  TimeToFirstPresentRecorder* time_to_first_present_recorder() {
    return time_to_first_present_recorder_.get();
  }
  ToastManager* toast_manager() { return toast_manager_.get(); }
  views::corewm::TooltipController* tooltip_controller() {
    return tooltip_controller_.get();
  }
  TouchDevicesController* touch_devices_controller() {
    return touch_devices_controller_.get();
  }
  AshTouchTransformController* touch_transformer_controller() {
    return touch_transformer_controller_.get();
  }
  TrayAction* tray_action() { return tray_action_.get(); }
  TrayBluetoothHelper* tray_bluetooth_helper() {
    return tray_bluetooth_helper_.get();
  }
  UserMetricsRecorder* metrics() { return user_metrics_recorder_.get(); }
  VideoDetector* video_detector() { return video_detector_.get(); }
  VirtualKeyboardController* virtual_keyboard_controller() {
    return virtual_keyboard_controller_.get();
  }
  VoiceInteractionController* voice_interaction_controller() {
    return voice_interaction_controller_.get();
  }
  VpnList* vpn_list() { return vpn_list_.get(); }
  WallpaperController* wallpaper_controller() {
    return wallpaper_controller_.get();
  }
  WallpaperDelegate* wallpaper_delegate() { return wallpaper_delegate_.get(); }
  WindowCycleController* window_cycle_controller() {
    return window_cycle_controller_.get();
  }
  WindowPositioner* window_positioner() { return window_positioner_.get(); }
  WindowSelectorController* window_selector_controller() {
    return window_selector_controller_.get();
  }
  WindowTreeHostManager* window_tree_host_manager() {
    return window_tree_host_manager_.get();
  }

  ToplevelWindowEventHandler* toplevel_window_event_handler() {
    return toplevel_window_event_handler_.get();
  }

  // Force the shelf to query for it's current visibility state.
  // TODO(jamescook): Move to Shelf.
  void UpdateShelfVisibility();

  // Gets the local state pref service. It can be null in mash if connecting to
  // local state pref service has not completed successfully.
  PrefService* GetLocalStatePrefService() const;

  // Returns WebNotificationTray on the primary root window.
  WebNotificationTray* GetWebNotificationTray();

  // Does the primary display have status area?
  bool HasPrimaryStatusArea();

  // Returns the system tray on primary display.
  SystemTray* GetPrimarySystemTray();

  // Starts the animation that occurs on first login.
  void DoInitialWorkspaceAnimation();

  // NOTE: Prefer ScopedRootWindowForNewWindows when setting temporarily.
  void set_root_window_for_new_windows(aura::Window* root) {
    root_window_for_new_windows_ = root;
  }

  // Creates instance of FirstRunHelper. Caller is responsible for deleting
  // returned object.
  ash::FirstRunHelper* CreateFirstRunHelper();

  void SetLargeCursorSizeInDip(int large_cursor_size_in_dip);

  // Updates cursor compositing on/off. Native cursor is disabled when cursor
  // compositing is enabled, and vice versa.
  void UpdateCursorCompositingEnabled();

  // Force setting compositing on/off without checking dependency.
  void SetCursorCompositingEnabled(bool enabled);

  // Returns true if split view mode is active.
  bool IsSplitViewModeActive() const;

  void AddShellObserver(ShellObserver* observer);
  void RemoveShellObserver(ShellObserver* observer);

  // Called when the login status changes.
  // TODO(oshima): Investigate if we can merge this and |OnLoginStateChanged|.
  void UpdateAfterLoginStatusChange(LoginStatus status);

  // Notifies observers that overview mode is about to be started (before the
  // windows get re-arranged).
  void NotifyOverviewModeStarting();

  // Notifies observers that overview mode has ended.
  void NotifyOverviewModeEnded();

  // Notifies observers that split view mode is about to be started (before the
  // window gets snapped and activated).
  void NotifySplitViewModeStarting();

  // Notifies observers that split view mode has ended.
  void NotifySplitViewModeEnded();

  // Notifies observers that fullscreen mode has changed for |root_window|.
  void NotifyFullscreenStateChanged(bool is_fullscreen,
                                    aura::Window* root_window);

  // Notifies observers that |pinned_window| changed its pinned window state.
  void NotifyPinnedStateChanged(aura::Window* pinned_window);

  // Notifies observers that the virtual keyboard has been
  // activated/deactivated for |root_window|.
  void NotifyVirtualKeyboardActivated(bool activated,
                                      aura::Window* root_window);

  // Notifies observers that the shelf was created for |root_window|.
  // TODO(jamescook): Move to Shelf.
  void NotifyShelfCreatedForRootWindow(aura::Window* root_window);

  // Notifies observers that |root_window|'s shelf changed alignment.
  // TODO(jamescook): Move to Shelf.
  void NotifyShelfAlignmentChanged(aura::Window* root_window);

  // Notifies observers that |root_window|'s shelf changed auto-hide behavior.
  // TODO(jamescook): Move to Shelf.
  void NotifyShelfAutoHideBehaviorChanged(aura::Window* root_window);

  // Used to provide better error messages for Shell::Get() under mash.
  static void SetIsBrowserProcessWithMash();

  void NotifyAppListVisibilityChanged(bool visible, aura::Window* root_window);

 private:
  FRIEND_TEST_ALL_PREFIXES(ExtendedDesktopTest, TestCursor);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, MouseEventCursors);
  FRIEND_TEST_ALL_PREFIXES(WindowManagerTest, TransformActivate);
  friend class AcceleratorControllerTest;
  friend class AshTestHelper;
  friend class RootWindowController;
  friend class ScopedRootWindowForNewWindows;
  friend class ShellTestApi;
  friend class SmsObserverTest;

  Shell(std::unique_ptr<ShellDelegate> shell_delegate,
        std::unique_ptr<ShellPort> shell_port);
  ~Shell() override;

  void Init(ui::ContextFactory* context_factory,
            ui::ContextFactoryPrivate* context_factory_private);

  // Initializes the display manager and related components.
  void InitializeDisplayManager();

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

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnLoginStatusChanged(LoginStatus login_status) override;
  void OnLockStateChanged(bool locked) override;

  // Callback for prefs::ConnectToPrefService.
  void OnLocalStatePrefServiceInitialized(
      std::unique_ptr<::PrefService> pref_service);

  static Shell* instance_;

  // Only valid in mash, for classic ash this is null.
  static aura::WindowTreeClient* window_tree_client_;
  static aura::WindowManagerClient* window_manager_client_;

  std::unique_ptr<ShellPort> shell_port_;

  // The CompoundEventFilter owned by aura::Env object.
  std::unique_ptr<::wm::CompoundEventFilter> env_filter_;

  std::unique_ptr<UserMetricsRecorder> user_metrics_recorder_;
  std::unique_ptr<WindowPositioner> window_positioner_;

  std::unique_ptr<AcceleratorController> accelerator_controller_;
  std::unique_ptr<AccessibilityController> accessibility_controller_;
  std::unique_ptr<AccessibilityDelegate> accessibility_delegate_;
  std::unique_ptr<AshDisplayController> ash_display_controller_;
  std::unique_ptr<BacklightsForcedOffSetter> backlights_forced_off_setter_;
  std::unique_ptr<BrightnessControlDelegate> brightness_control_delegate_;
  std::unique_ptr<CastConfigController> cast_config_;
  std::unique_ptr<DragDropController> drag_drop_controller_;
  std::unique_ptr<FocusCycler> focus_cycler_;
  std::unique_ptr<ImeController> ime_controller_;
  std::unique_ptr<ImmersiveContextAsh> immersive_context_;
  std::unique_ptr<KeyboardBrightnessControlDelegate>
      keyboard_brightness_control_delegate_;
  std::unique_ptr<KeyboardUI> keyboard_ui_;
  std::unique_ptr<LocaleNotificationController> locale_notification_controller_;
  std::unique_ptr<LoginScreenController> login_screen_controller_;
  std::unique_ptr<LogoutConfirmationController> logout_confirmation_controller_;
  std::unique_ptr<TabletModeController> tablet_mode_controller_;
  std::unique_ptr<MediaController> media_controller_;
  std::unique_ptr<MruWindowTracker> mru_window_tracker_;
  std::unique_ptr<NewWindowController> new_window_controller_;
  std::unique_ptr<ResizeShadowController> resize_shadow_controller_;
  std::unique_ptr<SessionController> session_controller_;
  std::unique_ptr<NightLightController> night_light_controller_;
  std::unique_ptr<NoteTakingController> note_taking_controller_;
  std::unique_ptr<ShelfController> shelf_controller_;
  std::unique_ptr<ShelfWindowWatcher> shelf_window_watcher_;
  std::unique_ptr<ShellDelegate> shell_delegate_;
  std::unique_ptr<ShutdownController> shutdown_controller_;
  std::unique_ptr<SystemTrayController> system_tray_controller_;
  std::unique_ptr<SystemTrayNotifier> system_tray_notifier_;
  std::unique_ptr<ToastManager> toast_manager_;
  std::unique_ptr<TouchDevicesController> touch_devices_controller_;
  std::unique_ptr<TimeToFirstPresentRecorder> time_to_first_present_recorder_;
  std::unique_ptr<TrayAction> tray_action_;
  std::unique_ptr<VoiceInteractionController> voice_interaction_controller_;
  std::unique_ptr<VpnList> vpn_list_;
  std::unique_ptr<WallpaperController> wallpaper_controller_;
  std::unique_ptr<WallpaperDelegate> wallpaper_delegate_;
  std::unique_ptr<WindowCycleController> window_cycle_controller_;
  std::unique_ptr<WindowSelectorController> window_selector_controller_;
  std::unique_ptr<::wm::ShadowController> shadow_controller_;
  std::unique_ptr<::wm::VisibilityController> visibility_controller_;
  std::unique_ptr<::wm::WindowModalityController> window_modality_controller_;
  std::unique_ptr<app_list::AppList> app_list_;
  std::unique_ptr<PrefService> local_state_;
  std::unique_ptr<views::corewm::TooltipController> tooltip_controller_;
  std::unique_ptr<PowerButtonController> power_button_controller_;
  std::unique_ptr<LockStateController> lock_state_controller_;
  std::unique_ptr<ui::UserActivityDetector> user_activity_detector_;
  std::unique_ptr<VideoDetector> video_detector_;
  std::unique_ptr<WindowTreeHostManager> window_tree_host_manager_;
  std::unique_ptr<HighContrastController> high_contrast_controller_;
  std::unique_ptr<MagnificationController> magnification_controller_;
  std::unique_ptr<AutoclickController> autoclick_controller_;
  std::unique_ptr<::wm::FocusController> focus_controller_;

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

  std::unique_ptr<PeripheralBatteryNotifier> peripheral_battery_notifier_;
  std::unique_ptr<PowerEventObserver> power_event_observer_;
  std::unique_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;
  std::unique_ptr<VideoActivityNotifier> video_activity_notifier_;
  std::unique_ptr<StickyKeysController> sticky_keys_controller_;
  std::unique_ptr<ResolutionNotificationController>
      resolution_notification_controller_;
  std::unique_ptr<BluetoothNotificationController>
      bluetooth_notification_controller_;
  std::unique_ptr<BluetoothPowerController> bluetooth_power_controller_;
  std::unique_ptr<TrayBluetoothHelper> tray_bluetooth_helper_;
  std::unique_ptr<VirtualKeyboardController> virtual_keyboard_controller_;
  // Controls video output device state.
  std::unique_ptr<display::DisplayConfigurator> display_configurator_;
  std::unique_ptr<DisplayColorManager> display_color_manager_;
  std::unique_ptr<DisplayErrorObserver> display_error_observer_;
  std::unique_ptr<ProjectingObserver> projecting_observer_;

  // Listens for output changes and updates the display manager.
  std::unique_ptr<display::DisplayChangeObserver> display_change_observer_;

  // Listens for shutdown and updates DisplayConfigurator.
  std::unique_ptr<DisplayShutdownObserver> display_shutdown_observer_;

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
  std::unique_ptr<HighlighterController> highlighter_controller_;

  // The split view controller for Chrome OS in tablet mode.
  std::unique_ptr<SplitViewController> split_view_controller_;

  // |native_cursor_manager_| is owned by |cursor_manager_|, but we keep a
  // pointer to vend to test code.
  NativeCursorManagerAsh* native_cursor_manager_;

  // Cursor may be hidden on certain key events in Chrome OS, whereas we never
  // hide the cursor on Windows.
  std::unique_ptr<::wm::CursorManager> cursor_manager_;

  // For testing only: simulate that a modal window is open
  bool simulate_modal_window_open_for_test_ = false;

  // See comment for GetRootWindowForNewWindows().
  aura::Window* root_window_for_new_windows_ = nullptr;
  aura::Window* scoped_root_window_for_new_windows_ = nullptr;

  std::unique_ptr<ImmersiveHandlerFactoryAsh> immersive_handler_factory_;

  std::unique_ptr<AppListDelegateImpl> app_list_delegate_impl_;

  std::unique_ptr<MessageCenterController> message_center_controller_;

  base::ObserverList<ShellObserver> shell_observers_;

  base::WeakPtrFactory<Shell> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace ash

#endif  // ASH_SHELL_H_
