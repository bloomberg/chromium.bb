// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_controller.h"

#include <limits>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/shutdown_reason.h"
#include "ash/system/power/power_button_display_controller.h"
#include "ash/system/power/power_button_menu_screen_view.h"
#include "ash/system/power/power_button_screenshot_controller.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/session_state_animator.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/time/default_tick_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager/backlight.pb.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Amount of time power button must be held to start the power menu animation
// for convertible/slate/detachable devices. This differs depending on whether
// the screen is on or off when the power button is initially pressed.
constexpr base::TimeDelta kShowMenuWhenScreenOnTimeout =
    base::TimeDelta::FromMilliseconds(500);
constexpr base::TimeDelta kShowMenuWhenScreenOffTimeout =
    base::TimeDelta::FromMilliseconds(2000);

// Time that power button should be pressed before starting to shutdown.
constexpr base::TimeDelta kStartShutdownTimeout =
    base::TimeDelta::FromSeconds(3);

// Creates a fullscreen widget responsible for showing the power button menu.
std::unique_ptr<views::Widget> CreateMenuWidget() {
  auto menu_widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.keep_on_top = true;
  params.accept_events = true;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "PowerButtonMenuWindow";
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_OverlayContainer);
  menu_widget->Init(params);

  gfx::Rect widget_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  menu_widget->SetBounds(widget_bounds);
  return menu_widget;
}

}  // namespace

constexpr base::TimeDelta PowerButtonController::kScreenStateChangeDelay;

constexpr base::TimeDelta PowerButtonController::kIgnoreRepeatedButtonUpDelay;

constexpr base::TimeDelta
    PowerButtonController::kIgnorePowerButtonAfterResumeDelay;

constexpr const char* PowerButtonController::kPositionField;
constexpr const char* PowerButtonController::kXField;
constexpr const char* PowerButtonController::kYField;
constexpr const char* PowerButtonController::kLeftPosition;
constexpr const char* PowerButtonController::kRightPosition;
constexpr const char* PowerButtonController::kTopPosition;
constexpr const char* PowerButtonController::kBottomPosition;

PowerButtonController::PowerButtonController(
    BacklightsForcedOffSetter* backlights_forced_off_setter)
    : backlights_forced_off_setter_(backlights_forced_off_setter),
      lock_state_controller_(Shell::Get()->lock_state_controller()),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      backlights_forced_off_observer_(this),
      weak_factory_(this) {
  ProcessCommandLine();
  display_controller_ = std::make_unique<PowerButtonDisplayController>(
      backlights_forced_off_setter_, tick_clock_);
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  power_manager_client->AddObserver(this);
  power_manager_client->GetSwitchStates(base::BindOnce(
      &PowerButtonController::OnGetSwitchStates, weak_factory_.GetWeakPtr()));
  chromeos::AccelerometerReader::GetInstance()->AddObserver(this);
  Shell::Get()->display_configurator()->AddObserver(this);
  backlights_forced_off_observer_.Add(backlights_forced_off_setter);
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
  ShellPort::Get()->AddLockStateObserver(this);
}

PowerButtonController::~PowerButtonController() {
  ShellPort::Get()->RemoveLockStateObserver(this);
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  Shell::Get()->display_configurator()->RemoveObserver(this);
  chromeos::AccelerometerReader::GetInstance()->RemoveObserver(this);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void PowerButtonController::OnPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  power_button_down_ = down;

  // Ignore power button if lock button is being pressed.
  if (lock_button_down_)
    return;

  if (button_type_ == ButtonType::LEGACY) {
    // Avoid starting the lock/shutdown sequence if the power button is pressed
    // while the screen is off (http://crbug.com/128451), unless an external
    // display is still on (http://crosbug.com/p/24912).
    if (brightness_is_zero_ && !internal_display_off_and_external_display_on_)
      return;

    // If power button releases won't get reported correctly because we're not
    // running on official hardware, just lock the screen or shut down
    // immediately.
    if (down) {
      const SessionController* const session_controller =
          Shell::Get()->session_controller();
      if (session_controller->CanLockScreen() &&
          !session_controller->IsUserSessionBlocked() &&
          !lock_state_controller_->LockRequested()) {
        lock_state_controller_->StartLockAnimationAndLockImmediately();
      } else {
        lock_state_controller_->RequestShutdown(ShutdownReason::POWER_BUTTON);
      }
    }
    return;
  }

  if (down) {
    show_menu_animation_done_ = false;
    if (turn_screen_off_for_tap_) {
      force_off_on_button_up_ = true;

      // When the system resumes in response to the power button being pressed,
      // Chrome receives powerd's SuspendDone signal and notification that the
      // backlight has been turned back on before seeing the power button events
      // that woke the system. Avoid forcing off display just after resuming to
      // ensure that we don't turn the display off in response to the events.
      if (timestamp - last_resume_time_ <= kIgnorePowerButtonAfterResumeDelay)
        force_off_on_button_up_ = false;

      // The actual display may remain off for a short period after powerd asks
      // Chrome to turn it on. If the user presses the power button again during
      // this time, they probably intend to turn the display on. Avoid forcing
      // off in this case.
      if (timestamp - display_controller_->screen_state_last_changed() <=
          kScreenStateChangeDelay) {
        force_off_on_button_up_ = false;
      }
    }

    screen_off_when_power_button_down_ = !display_controller_->IsScreenOn();
    display_controller_->SetBacklightsForcedOff(false);

    if (!turn_screen_off_for_tap_) {
      StartPowerMenuAnimation();
    } else {
      base::TimeDelta timeout = screen_off_when_power_button_down_
                                    ? kShowMenuWhenScreenOffTimeout
                                    : kShowMenuWhenScreenOnTimeout;

      power_button_menu_timer_.Start(
          FROM_HERE, timeout, this,
          &PowerButtonController::StartPowerMenuAnimation);
    }

    shutdown_timer_.Start(FROM_HERE, kStartShutdownTimeout, this,
                          &PowerButtonController::OnShutdownTimeout);
  } else {
    const base::TimeTicks previous_up_time = last_button_up_time_;
    last_button_up_time_ = timestamp;

    const bool menu_timer_was_running = power_button_menu_timer_.IsRunning();
    power_button_menu_timer_.Stop();
    shutdown_timer_.Stop();

    // Ignore the event if it comes too soon after the last one.
    if (timestamp - previous_up_time <= kIgnoreRepeatedButtonUpDelay)
      return;

    if (menu_timer_was_running && !screen_off_when_power_button_down_ &&
        force_off_on_button_up_) {
      display_controller_->SetBacklightsForcedOff(true);
      LockScreenIfRequired();
    }

    // Cancel the menu animation if it's still ongoing when the button is
    // released on a clamshell device.
    if (!turn_screen_off_for_tap_ && !show_menu_animation_done_) {
      static_cast<PowerButtonMenuScreenView*>(menu_widget_->GetContentsView())
          ->ScheduleShowHideAnimation(false);
    }
  }
}

void PowerButtonController::OnLockButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  lock_button_down_ = down;

  // Ignore the lock button behavior if power button is being pressed.
  if (power_button_down_)
    return;

  const SessionController* const session_controller =
      Shell::Get()->session_controller();
  if (!session_controller->CanLockScreen() ||
      session_controller->IsScreenLocked() ||
      lock_state_controller_->LockRequested() ||
      lock_state_controller_->ShutdownRequested()) {
    return;
  }

  DismissMenu();
  if (down)
    lock_state_controller_->StartLockAnimation();
  else
    lock_state_controller_->CancelLockAnimation();
}

void PowerButtonController::CancelPowerButtonEvent() {
  force_off_on_button_up_ = false;
  StopTimersAndDismissMenu();
}

bool PowerButtonController::IsMenuOpened() const {
  return menu_widget_ && menu_widget_->GetLayer()->GetTargetVisibility();
}

void PowerButtonController::DismissMenu() {
  if (IsMenuOpened())
    menu_widget_->Hide();
}

void PowerButtonController::OnDisplayModeChanged(
    const display::DisplayConfigurator::DisplayStateList& display_states) {
  bool internal_display_off = false;
  bool external_display_on = false;
  for (const display::DisplaySnapshot* display : display_states) {
    if (display->type() == display::DISPLAY_CONNECTION_TYPE_INTERNAL) {
      if (!display->current_mode())
        internal_display_off = true;
    } else if (display->current_mode()) {
      external_display_on = true;
    }
  }
  internal_display_off_and_external_display_on_ =
      internal_display_off && external_display_on;
}

void PowerButtonController::ScreenBrightnessChanged(
    const power_manager::BacklightBrightnessChange& change) {
  brightness_is_zero_ =
      change.percent() <= std::numeric_limits<double>::epsilon();
}

void PowerButtonController::PowerButtonEventReceived(
    bool down,
    const base::TimeTicks& timestamp) {
  if (lock_state_controller_->ShutdownRequested())
    return;

  // Handle tablet mode power button screenshot accelerator.
  if (screenshot_controller_ &&
      screenshot_controller_->OnPowerButtonEvent(down, timestamp)) {
    return;
  }

  OnPowerButtonEvent(down, timestamp);
}

void PowerButtonController::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  DismissMenu();
}

void PowerButtonController::SuspendDone(const base::TimeDelta& sleep_duration) {
  last_resume_time_ = tick_clock_->NowTicks();
}

void PowerButtonController::OnGetSwitchStates(
    base::Optional<chromeos::PowerManagerClient::SwitchStates> result) {
  if (!result.has_value())
    return;

  // Turn screen off if tapping the power button (except
  // |force_clamshell_power_button_|) and power button screenshot accelerator
  // are enabled on devices that have a tablet mode switch.
  if (result->tablet_mode ==
      chromeos::PowerManagerClient::TabletMode::UNSUPPORTED) {
    return;
  }

  has_tablet_mode_switch_ = true;
  InitTabletPowerButtonMembers();
}

void PowerButtonController::OnAccelerometerUpdated(
    scoped_refptr<const chromeos::AccelerometerUpdate> update) {
  // Turn screen off if tapping the power button (except
  // |force_clamshell_power_button_|) and power button screenshot accelerator
  // are enabled on devices that can enter tablet mode, which must have a tablet
  // mode switch or report accelerometer data before user actions.
  if (has_tablet_mode_switch_ || !observe_accelerometer_events_)
    return;
  InitTabletPowerButtonMembers();
}

void PowerButtonController::OnBacklightsForcedOffChanged(bool forced_off) {
  DismissMenu();
}

void PowerButtonController::OnScreenStateChanged(
    BacklightsForcedOffSetter::ScreenState screen_state) {
  if (screen_state != BacklightsForcedOffSetter::ScreenState::ON)
    DismissMenu();
}

void PowerButtonController::OnTabletModeStarted() {
  StopTimersAndDismissMenu();
}

void PowerButtonController::OnTabletModeEnded() {
  StopTimersAndDismissMenu();
}

void PowerButtonController::OnLockStateEvent(
    LockStateObserver::EventType event) {
  // Reset |lock_button_down_| when lock animation finished. LOCK_RELEASED is
  // not allowed when screen is locked, which means OnLockButtonEvent will not
  // be called in lock screen. This will lead |lock_button_down_| to stay in a
  // dirty state if press lock button after login but release in lock screen.
  if (event == EVENT_LOCK_ANIMATION_FINISHED)
    lock_button_down_ = false;
}

void PowerButtonController::StopTimersAndDismissMenu() {
  shutdown_timer_.Stop();
  power_button_menu_timer_.Stop();
  DismissMenu();
}

void PowerButtonController::StartPowerMenuAnimation() {
  if (!menu_widget_)
    menu_widget_ = CreateMenuWidget();
  menu_widget_->SetContentsView(new PowerButtonMenuScreenView(
      power_button_position_, power_button_offset_percentage_,
      base::BindRepeating(&PowerButtonController::SetShowMenuAnimationDone,
                          base::Unretained(this))));
  menu_widget_->Show();

  // Hide cursor, but let it reappear if the mouse moves.
  Shell* shell = Shell::Get();
  if (shell->cursor_manager())
    shell->cursor_manager()->HideCursor();

  static_cast<PowerButtonMenuScreenView*>(menu_widget_->GetContentsView())
      ->ScheduleShowHideAnimation(true);
}

void PowerButtonController::OnShutdownTimeout() {
  display_controller_->SetBacklightsForcedOff(true);
  lock_state_controller_->RequestShutdown(ShutdownReason::POWER_BUTTON);
}

void PowerButtonController::ProcessCommandLine() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  button_type_ = cl->HasSwitch(switches::kAuraLegacyPowerButton)
                     ? ButtonType::LEGACY
                     : ButtonType::NORMAL;
  observe_accelerometer_events_ = cl->HasSwitch(switches::kAshEnableTabletMode);
  force_clamshell_power_button_ =
      cl->HasSwitch(switches::kForceClamshellPowerButton);

  ParsePowerButtonPositionSwitch();
}

void PowerButtonController::InitTabletPowerButtonMembers() {
  if (!force_clamshell_power_button_)
    turn_screen_off_for_tap_ = true;

  if (!screenshot_controller_) {
    screenshot_controller_ =
        std::make_unique<PowerButtonScreenshotController>(tick_clock_);
  }
}

void PowerButtonController::LockScreenIfRequired() {
  const SessionController* session_controller =
      Shell::Get()->session_controller();
  if (session_controller->ShouldLockScreenAutomatically() &&
      session_controller->CanLockScreen() &&
      !session_controller->IsUserSessionBlocked() &&
      !lock_state_controller_->LockRequested()) {
    lock_state_controller_->LockWithoutAnimation();
  }
}

void PowerButtonController::SetShowMenuAnimationDone() {
  show_menu_animation_done_ = true;
}

void PowerButtonController::ParsePowerButtonPositionSwitch() {
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (!cl->HasSwitch(switches::kAshPowerButtonPosition))
    return;

  std::unique_ptr<base::DictionaryValue> position_info =
      base::DictionaryValue::From(base::JSONReader::Read(
          cl->GetSwitchValueASCII(switches::kAshPowerButtonPosition)));
  if (!position_info) {
    LOG(ERROR) << switches::kAshPowerButtonPosition << " flag has no value";
    return;
  }

  std::string str_power_button_position;
  if (!position_info->GetString(kPositionField, &str_power_button_position)) {
    LOG(ERROR) << kPositionField << " field is always needed if "
               << switches::kAshPowerButtonPosition << " is set";
    return;
  }

  if (str_power_button_position == kLeftPosition) {
    power_button_position_ = PowerButtonPosition::LEFT;
  } else if (str_power_button_position == kRightPosition) {
    power_button_position_ = PowerButtonPosition::RIGHT;
  } else if (str_power_button_position == kTopPosition) {
    power_button_position_ = PowerButtonPosition::TOP;
  } else if (str_power_button_position == kBottomPosition) {
    power_button_position_ = PowerButtonPosition::BOTTOM;
  } else {
    LOG(ERROR) << "Invalid " << kPositionField << " field in "
               << switches::kAshPowerButtonPosition;
    return;
  }

  if (power_button_position_ == PowerButtonPosition::LEFT ||
      power_button_position_ == PowerButtonPosition::RIGHT) {
    if (!position_info->GetDouble(kYField, &power_button_offset_percentage_)) {
      LOG(ERROR) << kYField << " not set in "
                 << switches::kAshPowerButtonPosition;
      power_button_position_ = PowerButtonPosition::NONE;
      return;
    }
  } else {
    if (!position_info->GetDouble(kXField, &power_button_offset_percentage_)) {
      LOG(ERROR) << kXField << " not set in "
                 << switches::kAshPowerButtonPosition;
      power_button_position_ = PowerButtonPosition::NONE;
      return;
    }
  }
}

}  // namespace ash
