// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_controller.h"

#include <utility>

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_port.h"
#include "ash/wm/maximize_mode/maximize_mode_window_manager.h"
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/chromeos/accelerometer/accelerometer_util.h"
#include "ui/display/display.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/vector3d_f.h"

#if defined(USE_X11)
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_x11.h"
#elif defined(USE_OZONE)
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_ozone.h"
#endif

namespace ash {

namespace {

// The hinge angle at which to enter maximize mode.
const float kEnterMaximizeModeAngle = 200.0f;

// The angle at which to exit maximize mode, this is specifically less than the
// angle to enter maximize mode to prevent rapid toggling when near the angle.
const float kExitMaximizeModeAngle = 160.0f;

// Defines a range for which accelerometer readings are considered accurate.
// When the lid is near open (or near closed) the accelerometer readings may be
// inaccurate and a lid that is fully open may appear to be near closed (and
// vice versa).
const float kMinStableAngle = 20.0f;
const float kMaxStableAngle = 340.0f;

// The time duration to consider the lid to be recently opened.
// This is used to prevent entering maximize mode if an erroneous accelerometer
// reading makes the lid appear to be fully open when the user is opening the
// lid from a closed position.
const int kLidRecentlyOpenedDurationSeconds = 2;

// When the device approaches vertical orientation (i.e. portrait orientation)
// the accelerometers for the base and lid approach the same values (i.e.
// gravity pointing in the direction of the hinge). When this happens abrupt
// small acceleration perpendicular to the hinge can lead to incorrect hinge
// angle calculations. To prevent this the accelerometer updates will be
// smoothed over time in order to reduce this noise.
// This is the minimum acceleration parallel to the hinge under which to begin
// smoothing in m/s^2.
const float kHingeVerticalSmoothingStart = 7.0f;
// This is the maximum acceleration parallel to the hinge under which smoothing
// will incorporate new acceleration values, in m/s^2.
const float kHingeVerticalSmoothingMaximum = 8.7f;

// The maximum deviation between the magnitude of the two accelerometers under
// which to detect hinge angle in m/s^2. These accelerometers are attached to
// the same physical device and so should be under the same acceleration.
const float kNoisyMagnitudeDeviation = 1.0f;

// The angle between chromeos::AccelerometerReadings are considered stable if
// their magnitudes do not differ greatly. This returns false if the deviation
// between the screen and keyboard accelerometers is too high.
bool IsAngleBetweenAccelerometerReadingsStable(
    const chromeos::AccelerometerUpdate& update) {
  return std::abs(
             ui::ConvertAccelerometerReadingToVector3dF(
                 update.get(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD))
                 .Length() -
             ui::ConvertAccelerometerReadingToVector3dF(
                 update.get(chromeos::ACCELEROMETER_SOURCE_SCREEN))
                 .Length()) <= kNoisyMagnitudeDeviation;
}

bool IsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableTouchView);
}

// Checks the command line to see which force maximize mode is turned on, if
// any.
MaximizeModeController::ForceTabletMode GetMaximizeMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshForceTabletMode)) {
    std::string switch_value =
        command_line->GetSwitchValueASCII(switches::kAshForceTabletMode);
    if (switch_value == switches::kAshForceTabletModeClamshell)
      return MaximizeModeController::ForceTabletMode::CLAMSHELL;

    if (switch_value == switches::kAshForceTabletModeTouchView)
      return MaximizeModeController::ForceTabletMode::TOUCHVIEW;
  }
  return MaximizeModeController::ForceTabletMode::NONE;
}

std::unique_ptr<ScopedDisableInternalMouseAndKeyboard>
CreateScopedDisableInternalMouseAndKeyboard() {
#if defined(USE_X11)
  return base::MakeUnique<ScopedDisableInternalMouseAndKeyboardX11>();
#elif defined(USE_OZONE)
  return base::MakeUnique<ScopedDisableInternalMouseAndKeyboardOzone>();
#endif
  return nullptr;
}

}  // namespace

MaximizeModeController::MaximizeModeController()
    : have_seen_accelerometer_data_(false),
      can_detect_lid_angle_(false),
      touchview_usage_interval_start_time_(base::Time::Now()),
      tick_clock_(new base::DefaultTickClock()),
      tablet_mode_switch_is_on_(false),
      lid_is_closed_(false),
      scoped_session_observer_(this),
      weak_factory_(this) {
  Shell::Get()->AddShellObserver(this);
  ShellPort::Get()->RecordUserMetricsAction(
      UMA_MAXIMIZE_MODE_INITIALLY_DISABLED);

  // TODO(jonross): Do not create MaximizeModeController if the flag is
  // unavailable. This will require refactoring
  // IsMaximizeModeWindowManagerEnabled to check for the existence of the
  // controller.
  if (IsEnabled()) {
    ShellPort::Get()->AddDisplayObserver(this);
    chromeos::AccelerometerReader::GetInstance()->AddObserver(this);
  }
  chromeos::PowerManagerClient* power_manager_client =
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
  power_manager_client->AddObserver(this);
  power_manager_client->GetSwitchStates(base::Bind(
      &MaximizeModeController::OnGetSwitchStates, weak_factory_.GetWeakPtr()));
}

MaximizeModeController::~MaximizeModeController() {
  Shell::Get()->RemoveShellObserver(this);

  if (IsEnabled()) {
    ShellPort::Get()->RemoveDisplayObserver(this);
    chromeos::AccelerometerReader::GetInstance()->RemoveObserver(this);
  }
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

bool MaximizeModeController::CanEnterMaximizeMode() {
  // If we have ever seen accelerometer data, then HandleHingeRotation may
  // trigger maximize mode at some point in the future.
  // All TouchView-enabled devices can enter maximized mode.
  return have_seen_accelerometer_data_ || IsEnabled();
}

// TODO(jcliang): Hide or remove EnableMaximizeModeWindowManager
// (http://crbug.com/620241).
void MaximizeModeController::EnableMaximizeModeWindowManager(
    bool should_enable) {
  bool is_enabled = !!maximize_mode_window_manager_.get();
  if (should_enable == is_enabled)
    return;

  if (should_enable) {
    maximize_mode_window_manager_.reset(new MaximizeModeWindowManager());
    // TODO(jonross): Move the maximize mode notifications from ShellObserver
    // to MaximizeModeController::Observer
    ShellPort::Get()->RecordUserMetricsAction(UMA_MAXIMIZE_MODE_ENABLED);
    Shell::Get()->NotifyMaximizeModeStarted();

    observers_.ForAllPtrs([](mojom::TouchViewObserver* observer) {
      observer->OnTouchViewToggled(true);
    });

  } else {
    maximize_mode_window_manager_->SetIgnoreWmEventsForExit();
    Shell::Get()->NotifyMaximizeModeEnding();
    maximize_mode_window_manager_.reset();
    ShellPort::Get()->RecordUserMetricsAction(UMA_MAXIMIZE_MODE_DISABLED);
    Shell::Get()->NotifyMaximizeModeEnded();

    observers_.ForAllPtrs([](mojom::TouchViewObserver* observer) {
      observer->OnTouchViewToggled(false);
    });
  }
}

bool MaximizeModeController::IsMaximizeModeWindowManagerEnabled() const {
  return maximize_mode_window_manager_.get() != NULL;
}

void MaximizeModeController::AddWindow(aura::Window* window) {
  if (IsMaximizeModeWindowManagerEnabled())
    maximize_mode_window_manager_->AddWindow(window);
}

void MaximizeModeController::BindRequest(
    mojom::TouchViewManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void MaximizeModeController::OnAccelerometerUpdated(
    scoped_refptr<const chromeos::AccelerometerUpdate> update) {
  if (!AllowEnterExitMaximizeMode())
    return;

  have_seen_accelerometer_data_ = true;
  can_detect_lid_angle_ =
      update->has(chromeos::ACCELEROMETER_SOURCE_SCREEN) &&
      update->has(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD);

  if (!can_detect_lid_angle_)
    return;

  if (!display::Display::HasInternalDisplay())
    return;

  if (!ShellPort::Get()->IsActiveDisplayId(
          display::Display::InternalDisplayId())) {
    return;
  }

  // Whether or not we enter maximize mode affects whether we handle screen
  // rotation, so determine whether to enter maximize mode first.
  if (ui::IsAccelerometerReadingStable(*update,
                                       chromeos::ACCELEROMETER_SOURCE_SCREEN) &&
      ui::IsAccelerometerReadingStable(
          *update, chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD) &&
      IsAngleBetweenAccelerometerReadingsStable(*update)) {
    // update.has(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD)
    // Ignore the reading if it appears unstable. The reading is considered
    // unstable if it deviates too much from gravity and/or the magnitude of the
    // reading from the lid differs too much from the reading from the base.
    HandleHingeRotation(update);
  }
}

void MaximizeModeController::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    const base::TimeTicks& time) {
  if (!AllowEnterExitMaximizeMode())
    return;

  const bool open = state == chromeos::PowerManagerClient::LidState::OPEN;
  if (open)
    last_lid_open_time_ = time;
  lid_is_closed_ = !open;
  LeaveMaximizeMode();
}

void MaximizeModeController::TabletModeEventReceived(
    chromeos::PowerManagerClient::TabletMode mode,
    const base::TimeTicks& time) {
  if (!AllowEnterExitMaximizeMode())
    return;

  const bool on = mode == chromeos::PowerManagerClient::TabletMode::ON;
  tablet_mode_switch_is_on_ = on;
  // Do not change if docked.
  if (!display::Display::HasInternalDisplay() ||
      !ShellPort::Get()->IsActiveDisplayId(
          display::Display::InternalDisplayId())) {
    return;
  }
  // The tablet mode switch activates at 300 degrees, so it is always reliable
  // when |on|. However we wish to exit maximize mode at a smaller angle, so
  // when |on| is false we ignore if it is possible to calculate the lid angle.
  if (on && !IsMaximizeModeWindowManagerEnabled()) {
    EnterMaximizeMode();
  } else if (!on && IsMaximizeModeWindowManagerEnabled() &&
             !can_detect_lid_angle_) {
    LeaveMaximizeMode();
  }
}

void MaximizeModeController::SuspendImminent() {
  // The system is about to suspend, so record TouchView usage interval metrics
  // based on whether TouchView mode is currently active.
  RecordTouchViewUsageInterval(CurrentTouchViewIntervalType());
}

void MaximizeModeController::SuspendDone(
    const base::TimeDelta& sleep_duration) {
  // We do not want TouchView usage metrics to include time spent in suspend.
  touchview_usage_interval_start_time_ = base::Time::Now();
}

void MaximizeModeController::HandleHingeRotation(
    scoped_refptr<const chromeos::AccelerometerUpdate> update) {
  static const gfx::Vector3dF hinge_vector(1.0f, 0.0f, 0.0f);
  gfx::Vector3dF base_reading(ui::ConvertAccelerometerReadingToVector3dF(
      update->get(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD)));
  gfx::Vector3dF lid_reading(ui::ConvertAccelerometerReadingToVector3dF(
      update->get(chromeos::ACCELEROMETER_SOURCE_SCREEN)));

  // As the hinge approaches a vertical angle, the base and lid accelerometers
  // approach the same values making any angle calculations highly inaccurate.
  // Smooth out instantaneous acceleration when nearly vertical to increase
  // accuracy.
  float largest_hinge_acceleration =
      std::max(std::abs(base_reading.x()), std::abs(lid_reading.x()));
  float smoothing_ratio =
      std::max(0.0f, std::min(1.0f, (largest_hinge_acceleration -
                                     kHingeVerticalSmoothingStart) /
                                        (kHingeVerticalSmoothingMaximum -
                                         kHingeVerticalSmoothingStart)));

  // We cannot trust the computed lid angle when the device is held vertically.
  bool is_angle_reliable =
      largest_hinge_acceleration <= kHingeVerticalSmoothingMaximum;

  base_smoothed_.Scale(smoothing_ratio);
  base_reading.Scale(1.0f - smoothing_ratio);
  base_smoothed_.Add(base_reading);

  lid_smoothed_.Scale(smoothing_ratio);
  lid_reading.Scale(1.0f - smoothing_ratio);
  lid_smoothed_.Add(lid_reading);

  if (tablet_mode_switch_is_on_)
    return;

  // Ignore the component of acceleration parallel to the hinge for the purposes
  // of hinge angle calculation.
  gfx::Vector3dF base_flattened(base_smoothed_);
  gfx::Vector3dF lid_flattened(lid_smoothed_);
  base_flattened.set_x(0.0f);
  lid_flattened.set_x(0.0f);

  // Compute the angle between the base and the lid.
  float lid_angle = 180.0f - gfx::ClockwiseAngleBetweenVectorsInDegrees(
                                 base_flattened, lid_flattened, hinge_vector);
  if (lid_angle < 0.0f)
    lid_angle += 360.0f;

  bool is_angle_stable = is_angle_reliable && lid_angle >= kMinStableAngle &&
                         lid_angle <= kMaxStableAngle;

  // Clear the last_lid_open_time_ for a stable reading so that there is less
  // chance of a delay if the lid is moved from the close state to the fully
  // open state very quickly.
  if (is_angle_stable)
    last_lid_open_time_ = base::TimeTicks();

  // Toggle maximize mode on or off when corresponding thresholds are passed.
  if (IsMaximizeModeWindowManagerEnabled() && is_angle_stable &&
      lid_angle <= kExitMaximizeModeAngle) {
    LeaveMaximizeMode();
  } else if (!IsMaximizeModeWindowManagerEnabled() && !lid_is_closed_ &&
             lid_angle >= kEnterMaximizeModeAngle &&
             (is_angle_stable || !WasLidOpenedRecently())) {
    EnterMaximizeMode();
  }
}

void MaximizeModeController::EnterMaximizeMode() {
  // Always reset first to avoid creation before destruction of a previous
  // object.
  event_blocker_ = CreateScopedDisableInternalMouseAndKeyboard();

  if (IsMaximizeModeWindowManagerEnabled())
    return;
  EnableMaximizeModeWindowManager(true);
}

void MaximizeModeController::LeaveMaximizeMode() {
  event_blocker_.reset();

  if (!IsMaximizeModeWindowManagerEnabled())
    return;
  EnableMaximizeModeWindowManager(false);
}

// Called after maximize mode has started, windows might still animate though.
void MaximizeModeController::OnMaximizeModeStarted() {
  RecordTouchViewUsageInterval(TOUCH_VIEW_INTERVAL_INACTIVE);
}

// Called after maximize mode has ended, windows might still be returning to
// their original position.
void MaximizeModeController::OnMaximizeModeEnded() {
  RecordTouchViewUsageInterval(TOUCH_VIEW_INTERVAL_ACTIVE);
}

void MaximizeModeController::OnShellInitialized() {
  force_tablet_mode_ = GetMaximizeMode();
  if (force_tablet_mode_ == ForceTabletMode::TOUCHVIEW)
    EnterMaximizeMode();
}

void MaximizeModeController::OnDisplayConfigurationChanged() {
  if (!display::Display::HasInternalDisplay() ||
      !ShellPort::Get()->IsActiveDisplayId(
          display::Display::InternalDisplayId())) {
    LeaveMaximizeMode();
  } else if (tablet_mode_switch_is_on_ &&
             !IsMaximizeModeWindowManagerEnabled()) {
    // The internal display has returned, as we are exiting docked mode.
    // The device is still in tablet mode, so trigger maximize mode, as this
    // switch leads to the ignoring of accelerometer events. When the switch is
    // not set the next stable accelerometer readings will trigger maximize
    // mode.
    EnterMaximizeMode();
  }
}

void MaximizeModeController::RecordTouchViewUsageInterval(
    TouchViewIntervalType type) {
  if (!CanEnterMaximizeMode())
    return;

  base::Time current_time = base::Time::Now();
  base::TimeDelta delta = current_time - touchview_usage_interval_start_time_;
  switch (type) {
    case TOUCH_VIEW_INTERVAL_INACTIVE:
      UMA_HISTOGRAM_LONG_TIMES("Ash.TouchView.TouchViewInactive", delta);
      total_non_touchview_time_ += delta;
      break;
    case TOUCH_VIEW_INTERVAL_ACTIVE:
      UMA_HISTOGRAM_LONG_TIMES("Ash.TouchView.TouchViewActive", delta);
      total_touchview_time_ += delta;
      break;
  }

  touchview_usage_interval_start_time_ = current_time;
}

MaximizeModeController::TouchViewIntervalType
MaximizeModeController::CurrentTouchViewIntervalType() {
  if (IsMaximizeModeWindowManagerEnabled())
    return TOUCH_VIEW_INTERVAL_ACTIVE;
  return TOUCH_VIEW_INTERVAL_INACTIVE;
}

void MaximizeModeController::AddObserver(mojom::TouchViewObserverPtr observer) {
  observer->OnTouchViewToggled(IsMaximizeModeWindowManagerEnabled());
  observers_.AddPtr(std::move(observer));
}

bool MaximizeModeController::AllowEnterExitMaximizeMode() const {
  return force_tablet_mode_ == ForceTabletMode::NONE;
}

void MaximizeModeController::OnChromeTerminating() {
  // The system is about to shut down, so record TouchView usage interval
  // metrics based on whether TouchView mode is currently active.
  RecordTouchViewUsageInterval(CurrentTouchViewIntervalType());

  if (CanEnterMaximizeMode()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchView.TouchViewActiveTotal",
                                total_touchview_time_.InMinutes(), 1,
                                base::TimeDelta::FromDays(7).InMinutes(), 50);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchView.TouchViewInactiveTotal",
                                total_non_touchview_time_.InMinutes(), 1,
                                base::TimeDelta::FromDays(7).InMinutes(), 50);
    base::TimeDelta total_runtime =
        total_touchview_time_ + total_non_touchview_time_;
    if (total_runtime.InSeconds() > 0) {
      UMA_HISTOGRAM_PERCENTAGE(
          "Ash.TouchView.TouchViewActivePercentage",
          100 * total_touchview_time_.InSeconds() / total_runtime.InSeconds());
    }
  }
}

void MaximizeModeController::OnGetSwitchStates(
    chromeos::PowerManagerClient::LidState lid_state,
    chromeos::PowerManagerClient::TabletMode tablet_mode) {
  LidEventReceived(lid_state, base::TimeTicks::Now());
  TabletModeEventReceived(tablet_mode, base::TimeTicks::Now());
}

bool MaximizeModeController::WasLidOpenedRecently() const {
  if (last_lid_open_time_.is_null())
    return false;

  base::TimeTicks now = tick_clock_->NowTicks();
  DCHECK(now >= last_lid_open_time_);
  base::TimeDelta elapsed_time = now - last_lid_open_time_;
  return elapsed_time.InSeconds() <= kLidRecentlyOpenedDurationSeconds;
}

void MaximizeModeController::SetTickClockForTest(
    std::unique_ptr<base::TickClock> tick_clock) {
  DCHECK(tick_clock_);
  tick_clock_ = std::move(tick_clock);
}

}  // namespace ash
