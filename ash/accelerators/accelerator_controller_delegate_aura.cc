// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller_delegate_aura.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_commands_aura.h"
#include "ash/common/accelerators/debug_commands.h"
#include "ash/common/accessibility_types.h"
#include "ash/common/ash_switches.h"
#include "ash/common/focus_cycler.h"
#include "ash/common/gpu_support.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/system_notifier.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/web_notification/web_notification_tray.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_event.h"
#include "ash/common/wm_shell.h"
#include "ash/debug.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_manager.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/rotator/window_rotation.h"
#include "ash/screen_util.h"
#include "ash/screenshot_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/utility/screenshot_controller.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notifier_settings.h"

#if defined(OS_CHROMEOS)
#include "ash/display/display_configuration_controller.h"
#include "base/sys_info.h"
#endif  // defined(OS_CHROMEOS)

namespace ash {
namespace {

using base::UserMetricsAction;

// The notification delegate that will be used to open the keyboard shortcut
// help page when the notification is clicked.
class DeprecatedAcceleratorNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  DeprecatedAcceleratorNotificationDelegate() {}

  // message_center::NotificationDelegate:
  bool HasClickedListener() override { return true; }

  void Click() override {
    if (!WmShell::Get()->GetSessionStateDelegate()->IsUserSessionBlocked())
      WmShell::Get()->delegate()->OpenKeyboardShortcutHelpPage();
  }

 private:
  // Private destructor since NotificationDelegate is ref-counted.
  ~DeprecatedAcceleratorNotificationDelegate() override {}

  DISALLOW_COPY_AND_ASSIGN(DeprecatedAcceleratorNotificationDelegate);
};

// Ensures that there are no word breaks at the "+"s in the shortcut texts such
// as "Ctrl+Shift+Space".
void EnsureNoWordBreaks(base::string16* shortcut_text) {
  std::vector<base::string16> keys =
      base::SplitString(*shortcut_text, base::ASCIIToUTF16("+"),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (keys.size() < 2U)
    return;

  // The plus sign surrounded by the word joiner to guarantee an non-breaking
  // shortcut.
  const base::string16 non_breaking_plus =
      base::UTF8ToUTF16("\xe2\x81\xa0+\xe2\x81\xa0");
  shortcut_text->clear();
  for (size_t i = 0; i < keys.size() - 1; ++i) {
    *shortcut_text += keys[i];
    *shortcut_text += non_breaking_plus;
  }

  *shortcut_text += keys.back();
}

// Gets the notification message after it formats it in such a way that there
// are no line breaks in the middle of the shortcut texts.
base::string16 GetNotificationText(int message_id,
                                   int old_shortcut_id,
                                   int new_shortcut_id) {
  base::string16 old_shortcut = l10n_util::GetStringUTF16(old_shortcut_id);
  base::string16 new_shortcut = l10n_util::GetStringUTF16(new_shortcut_id);
  EnsureNoWordBreaks(&old_shortcut);
  EnsureNoWordBreaks(&new_shortcut);

  return l10n_util::GetStringFUTF16(message_id, new_shortcut, old_shortcut);
}

void HandleFocusShelf() {
  base::RecordAction(UserMetricsAction("Accel_Focus_Shelf"));
  WmShell::Get()->focus_cycler()->FocusWidget(
      Shelf::ForPrimaryDisplay()->shelf_widget());
}

void HandleLaunchAppN(int n) {
  base::RecordAction(UserMetricsAction("Accel_Launch_App"));
  Shelf::ForPrimaryDisplay()->LaunchAppIndexAt(n);
}

void HandleLaunchLastApp() {
  base::RecordAction(UserMetricsAction("Accel_Launch_Last_App"));
  Shelf::ForPrimaryDisplay()->LaunchAppIndexAt(-1);
}

bool CanHandleMagnifyScreen() {
  Shell* shell = Shell::GetInstance();
  return shell->magnification_controller()->IsEnabled() ||
         shell->partial_magnification_controller()->is_enabled();
}

// Magnify the screen
void HandleMagnifyScreen(int delta_index) {
  if (Shell::GetInstance()->magnification_controller()->IsEnabled()) {
    // TODO(yoshiki): Move the following logic to MagnificationController.
    float scale = Shell::GetInstance()->magnification_controller()->GetScale();
    // Calculate rounded logarithm (base kMagnificationScaleFactor) of scale.
    int scale_index =
        std::floor(std::log(scale) / std::log(kMagnificationScaleFactor) + 0.5);

    int new_scale_index = std::max(0, std::min(8, scale_index + delta_index));

    Shell::GetInstance()->magnification_controller()->SetScale(
        std::pow(kMagnificationScaleFactor, new_scale_index), true);
  } else if (Shell::GetInstance()
                 ->partial_magnification_controller()
                 ->is_enabled()) {
    float scale = delta_index > 0 ? kDefaultPartialMagnifiedScale : 1;
    Shell::GetInstance()->partial_magnification_controller()->SetScale(scale);
  }
}

display::Display::Rotation GetNextRotation(display::Display::Rotation current) {
  switch (current) {
    case display::Display::ROTATE_0:
      return display::Display::ROTATE_90;
    case display::Display::ROTATE_90:
      return display::Display::ROTATE_180;
    case display::Display::ROTATE_180:
      return display::Display::ROTATE_270;
    case display::Display::ROTATE_270:
      return display::Display::ROTATE_0;
  }
  NOTREACHED() << "Unknown rotation:" << current;
  return display::Display::ROTATE_0;
}

// Rotates the screen.
void HandleRotateScreen() {
  if (Shell::GetInstance()->display_manager()->IsInUnifiedMode())
    return;

  base::RecordAction(UserMetricsAction("Accel_Rotate_Window"));
  gfx::Point point = display::Screen::GetScreen()->GetCursorScreenPoint();
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point);
  const DisplayInfo& display_info =
      Shell::GetInstance()->display_manager()->GetDisplayInfo(display.id());
  ScreenRotationAnimator(display.id())
      .Rotate(GetNextRotation(display_info.GetActiveRotation()),
              display::Display::ROTATION_SOURCE_USER);
}

// Rotate the active window.
void HandleRotateActiveWindow() {
  base::RecordAction(UserMetricsAction("Accel_Rotate_Window"));
  aura::Window* active_window = wm::GetActiveWindow();
  if (active_window) {
    // The rotation animation bases its target transform on the current
    // rotation and position. Since there could be an animation in progress
    // right now, queue this animation so when it starts it picks up a neutral
    // rotation and position. Use replace so we only enqueue one at a time.
    active_window->layer()->GetAnimator()->set_preemption_strategy(
        ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
    active_window->layer()->GetAnimator()->StartAnimation(
        new ui::LayerAnimationSequence(
            new WindowRotation(360, active_window->layer())));
  }
}

bool CanHandleShowMessageCenterBubble() {
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  StatusAreaWidget* status_area_widget =
      controller->shelf_widget()->status_area_widget();
  return status_area_widget &&
         status_area_widget->web_notification_tray()->visible();
}

void HandleShowMessageCenterBubble() {
  base::RecordAction(UserMetricsAction("Accel_Show_Message_Center_Bubble"));
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  StatusAreaWidget* status_area_widget =
      controller->shelf_widget()->status_area_widget();
  if (status_area_widget) {
    WebNotificationTray* notification_tray =
        status_area_widget->web_notification_tray();
    if (notification_tray->visible())
      notification_tray->ShowMessageCenterBubble();
  }
}

void HandleShowSystemTrayBubble() {
  base::RecordAction(UserMetricsAction("Accel_Show_System_Tray_Bubble"));
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  if (!controller->GetSystemTray()->HasSystemBubble())
    controller->GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);
}

void HandleTakeWindowScreenshot(ScreenshotDelegate* screenshot_delegate) {
  base::RecordAction(UserMetricsAction("Accel_Take_Window_Screenshot"));
  DCHECK(screenshot_delegate);
  Shell::GetInstance()->screenshot_controller()->StartWindowScreenshotSession(
      screenshot_delegate);
}

void HandleTakePartialScreenshot(ScreenshotDelegate* screenshot_delegate) {
  base::RecordAction(UserMetricsAction("Accel_Take_Partial_Screenshot"));
  DCHECK(screenshot_delegate);
  Shell::GetInstance()->screenshot_controller()->StartPartialScreenshotSession(
      screenshot_delegate);
}

void HandleTakeScreenshot(ScreenshotDelegate* screenshot_delegate) {
  base::RecordAction(UserMetricsAction("Accel_Take_Screenshot"));
  DCHECK(screenshot_delegate);
  if (screenshot_delegate->CanTakeScreenshot())
    screenshot_delegate->HandleTakeScreenshotForAllRootWindows();
}

gfx::ImageSkia CreateWallpaperImage(SkColor fill, SkColor rect) {
  // TODO(oshima): Consider adding a command line option to control
  // wallpaper images for testing.
  // The size is randomly picked.
  gfx::Size image_size(1366, 768);
  gfx::Canvas canvas(image_size, 1.0f, true);
  canvas.DrawColor(fill);
  SkPaint paint;
  paint.setColor(rect);
  paint.setStrokeWidth(10);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  canvas.DrawRoundRect(gfx::Rect(image_size), 100, paint);
  return gfx::ImageSkia(canvas.ExtractImageRep());
}

void HandleToggleDesktopBackgroundMode() {
  static int index = 0;
  DesktopBackgroundController* desktop_background_controller =
      Shell::GetInstance()->desktop_background_controller();
  switch (++index % 4) {
    case 0:
      ash::Shell::GetInstance()
          ->user_wallpaper_delegate()
          ->InitializeWallpaper();
      break;
    case 1:
      desktop_background_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorRED, SK_ColorBLUE),
          wallpaper::WALLPAPER_LAYOUT_STRETCH);
      break;
    case 2:
      desktop_background_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorBLUE, SK_ColorGREEN),
          wallpaper::WALLPAPER_LAYOUT_CENTER);
      break;
    case 3:
      desktop_background_controller->SetWallpaperImage(
          CreateWallpaperImage(SK_ColorGREEN, SK_ColorRED),
          wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED);
      break;
  }
}

bool CanHandleUnpin() {
  wm::WindowState* window_state = wm::GetActiveWindowState();
  return window_state && window_state->IsPinned();
}

#if defined(OS_CHROMEOS)
void HandleSwapPrimaryDisplay() {
  base::RecordAction(UserMetricsAction("Accel_Swap_Primary_Display"));
  Shell::GetInstance()->display_configuration_controller()->SetPrimaryDisplayId(
      ScreenUtil::GetSecondaryDisplay().id(), true /* user_action */);
}

void HandleToggleMirrorMode() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Mirror_Mode"));
  bool mirror = !Shell::GetInstance()->display_manager()->IsInMirrorMode();
  Shell::GetInstance()->display_configuration_controller()->SetMirrorMode(
      mirror, true /* user_action */);
}

bool CanHandleTouchHud() {
  return RootWindowController::ForTargetRootWindow()->touch_hud_debug();
}

void HandleTouchHudClear() {
  RootWindowController::ForTargetRootWindow()->touch_hud_debug()->Clear();
}

void HandleTouchHudModeChange() {
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  controller->touch_hud_debug()->ChangeToNextMode();
}

#endif  // defined(OS_CHROMEOS)

}  // namespace

AcceleratorControllerDelegateAura::AcceleratorControllerDelegateAura() {}

AcceleratorControllerDelegateAura::~AcceleratorControllerDelegateAura() {}

void AcceleratorControllerDelegateAura::SetScreenshotDelegate(
    std::unique_ptr<ScreenshotDelegate> screenshot_delegate) {
  screenshot_delegate_ = std::move(screenshot_delegate);
}

bool AcceleratorControllerDelegateAura::HandlesAction(
    AcceleratorAction action) {
  switch (action) {
    case DEBUG_TOGGLE_DESKTOP_BACKGROUND_MODE:
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
    case DEBUG_TOGGLE_ROOT_WINDOW_FULL_SCREEN:
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
    case FOCUS_SHELF:
    case LAUNCH_APP_0:
    case LAUNCH_APP_1:
    case LAUNCH_APP_2:
    case LAUNCH_APP_3:
    case LAUNCH_APP_4:
    case LAUNCH_APP_5:
    case LAUNCH_APP_6:
    case LAUNCH_APP_7:
    case LAUNCH_LAST_APP:
    case MAGNIFY_SCREEN_ZOOM_IN:
    case MAGNIFY_SCREEN_ZOOM_OUT:
    case ROTATE_SCREEN:
    case ROTATE_WINDOW:
    case SCALE_UI_DOWN:
    case SCALE_UI_RESET:
    case SCALE_UI_UP:
    case SHOW_MESSAGE_CENTER_BUBBLE:
    case SHOW_SYSTEM_TRAY_BUBBLE:
    case TAKE_PARTIAL_SCREENSHOT:
    case TAKE_SCREENSHOT:
    case TAKE_WINDOW_SCREENSHOT:
    case UNPIN:
      return true;

#if defined(OS_CHROMEOS)
    case DEBUG_ADD_REMOVE_DISPLAY:
    case DEBUG_TOGGLE_UNIFIED_DESKTOP:
    case DISABLE_GPU_WATCHDOG:
    case LOCK_PRESSED:
    case LOCK_RELEASED:
    case POWER_PRESSED:
    case POWER_RELEASED:
    case SWAP_PRIMARY_DISPLAY:
    case TOGGLE_MIRROR_MODE:
    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
    case TOUCH_HUD_PROJECTION_TOGGLE:
      return true;
#endif

    default:
      break;
  }
  return false;
}

bool AcceleratorControllerDelegateAura::CanPerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator,
    const ui::Accelerator& previous_accelerator) {
  switch (action) {
    case DEBUG_TOGGLE_DESKTOP_BACKGROUND_MODE:
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
    case DEBUG_TOGGLE_ROOT_WINDOW_FULL_SCREEN:
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
      return debug::DebugAcceleratorsEnabled();
    case MAGNIFY_SCREEN_ZOOM_IN:
    case MAGNIFY_SCREEN_ZOOM_OUT:
      return CanHandleMagnifyScreen();
    case SCALE_UI_DOWN:
    case SCALE_UI_RESET:
    case SCALE_UI_UP:
      return accelerators::IsInternalDisplayZoomEnabled();
    case SHOW_MESSAGE_CENTER_BUBBLE:
      return CanHandleShowMessageCenterBubble();
    case UNPIN:
      return CanHandleUnpin();

    // Following are always enabled:
    case FOCUS_SHELF:
    case LAUNCH_APP_0:
    case LAUNCH_APP_1:
    case LAUNCH_APP_2:
    case LAUNCH_APP_3:
    case LAUNCH_APP_4:
    case LAUNCH_APP_5:
    case LAUNCH_APP_6:
    case LAUNCH_APP_7:
    case LAUNCH_LAST_APP:
    case ROTATE_SCREEN:
    case ROTATE_WINDOW:
    case SHOW_SYSTEM_TRAY_BUBBLE:
    case TAKE_PARTIAL_SCREENSHOT:
    case TAKE_SCREENSHOT:
    case TAKE_WINDOW_SCREENSHOT:
      return true;

#if defined(OS_CHROMEOS)
    case DEBUG_ADD_REMOVE_DISPLAY:
    case DEBUG_TOGGLE_UNIFIED_DESKTOP:
      return debug::DebugAcceleratorsEnabled();

    case SWAP_PRIMARY_DISPLAY:
      return display::Screen::GetScreen()->GetNumDisplays() > 1;
    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
      return CanHandleTouchHud();

    // Following are always enabled.
    case DISABLE_GPU_WATCHDOG:
    case LOCK_PRESSED:
    case LOCK_RELEASED:
    case POWER_PRESSED:
    case POWER_RELEASED:
    case TOGGLE_MIRROR_MODE:
    case TOUCH_HUD_PROJECTION_TOGGLE:
      return true;
#endif

    default:
      NOTREACHED();
      break;
  }
  return false;
}

void AcceleratorControllerDelegateAura::PerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  switch (action) {
    case DEBUG_TOGGLE_DESKTOP_BACKGROUND_MODE:
      HandleToggleDesktopBackgroundMode();
      break;
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
      Shell::GetInstance()->display_manager()->ToggleDisplayScaleFactor();
      break;
    case DEBUG_TOGGLE_ROOT_WINDOW_FULL_SCREEN:
      Shell::GetPrimaryRootWindowController()->ash_host()->ToggleFullScreen();
      break;
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
      debug::ToggleShowDebugBorders();
      break;
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
      debug::ToggleShowFpsCounter();
      break;
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
      debug::ToggleShowPaintRects();
      break;
    case FOCUS_SHELF:
      HandleFocusShelf();
      break;
    case LAUNCH_APP_0:
      HandleLaunchAppN(0);
      break;
    case LAUNCH_APP_1:
      HandleLaunchAppN(1);
      break;
    case LAUNCH_APP_2:
      HandleLaunchAppN(2);
      break;
    case LAUNCH_APP_3:
      HandleLaunchAppN(3);
      break;
    case LAUNCH_APP_4:
      HandleLaunchAppN(4);
      break;
    case LAUNCH_APP_5:
      HandleLaunchAppN(5);
      break;
    case LAUNCH_APP_6:
      HandleLaunchAppN(6);
      break;
    case LAUNCH_APP_7:
      HandleLaunchAppN(7);
      break;
    case LAUNCH_LAST_APP:
      HandleLaunchLastApp();
      break;
    case MAGNIFY_SCREEN_ZOOM_IN:
      HandleMagnifyScreen(1);
      break;
    case MAGNIFY_SCREEN_ZOOM_OUT:
      HandleMagnifyScreen(-1);
      break;
    case ROTATE_SCREEN:
      HandleRotateScreen();
      break;
    case ROTATE_WINDOW:
      HandleRotateActiveWindow();
      break;
    case SCALE_UI_DOWN:
      accelerators::ZoomInternalDisplay(false /* down */);
      break;
    case SCALE_UI_RESET:
      accelerators::ResetInternalDisplayZoom();
      break;
    case SCALE_UI_UP:
      accelerators::ZoomInternalDisplay(true /* up */);
      break;
    case SHOW_MESSAGE_CENTER_BUBBLE:
      HandleShowMessageCenterBubble();
      break;
    case SHOW_SYSTEM_TRAY_BUBBLE:
      HandleShowSystemTrayBubble();
      break;
    case TAKE_PARTIAL_SCREENSHOT:
      HandleTakePartialScreenshot(screenshot_delegate_.get());
      break;
    case TAKE_SCREENSHOT:
      HandleTakeScreenshot(screenshot_delegate_.get());
      break;
    case TAKE_WINDOW_SCREENSHOT:
      HandleTakeWindowScreenshot(screenshot_delegate_.get());
      break;
    case UNPIN:
      accelerators::Unpin();
      break;
#if defined(OS_CHROMEOS)
    case DEBUG_ADD_REMOVE_DISPLAY:
      Shell::GetInstance()->display_manager()->AddRemoveDisplay();
      break;
    case DEBUG_TOGGLE_UNIFIED_DESKTOP:
      Shell::GetInstance()->display_manager()->SetUnifiedDesktopEnabled(
          !Shell::GetInstance()->display_manager()->unified_desktop_enabled());
      break;
    case DISABLE_GPU_WATCHDOG:
      Shell::GetInstance()->gpu_support()->DisableGpuWatchdog();
      break;
    case LOCK_PRESSED:
    case LOCK_RELEASED:
      Shell::GetInstance()->power_button_controller()->OnLockButtonEvent(
          action == LOCK_PRESSED, base::TimeTicks());
      break;
    case POWER_PRESSED:  // fallthrough
    case POWER_RELEASED:
      if (!base::SysInfo::IsRunningOnChromeOS()) {
        // There is no powerd, the Chrome OS power manager, in linux desktop,
        // so call the PowerButtonController here.
        Shell::GetInstance()->power_button_controller()->OnPowerButtonEvent(
            action == POWER_PRESSED, base::TimeTicks());
      }
      // We don't do anything with these at present on the device,
      // (power button events are reported to us from powerm via
      // D-BUS), but we consume them to prevent them from getting
      // passed to apps -- see http://crbug.com/146609.
      break;
    case SWAP_PRIMARY_DISPLAY:
      HandleSwapPrimaryDisplay();
      break;
    case TOGGLE_MIRROR_MODE:
      HandleToggleMirrorMode();
      break;
    case TOUCH_HUD_CLEAR:
      HandleTouchHudClear();
      break;
    case TOUCH_HUD_MODE_CHANGE:
      HandleTouchHudModeChange();
      break;
    case TOUCH_HUD_PROJECTION_TOGGLE:
      accelerators::ToggleTouchHudProjection();
      break;
#endif
    default:
      break;
  }
}

void AcceleratorControllerDelegateAura::ShowDeprecatedAcceleratorNotification(
    const char* const notification_id,
    int message_id,
    int old_shortcut_id,
    int new_shortcut_id) {
  const base::string16 message =
      GetNotificationText(message_id, old_shortcut_id, new_shortcut_id);
  std::unique_ptr<message_center::Notification> notification(
      new message_center::Notification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          base::string16(), message,
          WmShell::Get()->delegate()->GetDeprecatedAcceleratorImage(),
          base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              system_notifier::kNotifierDeprecatedAccelerator),
          message_center::RichNotificationData(),
          new DeprecatedAcceleratorNotificationDelegate));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

}  // namespace ash
