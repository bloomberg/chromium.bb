// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_controller.h"

#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/ash_switches.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/wm/maximize_mode/maximize_mode_window_manager.h"
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/display/display.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/vector3d_f.h"

#if defined(USE_X11)
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_x11.h"
#endif

#if defined(USE_OZONE)
#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_ozone.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/chromeos/accelerometer/accelerometer_util.h"
#endif  // OS_CHROMEOS

namespace ash {

namespace {

#if defined(OS_CHROMEOS)
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
#endif  // OS_CHROMEOS

// The time duration to consider the lid to be recently opened.
// This is used to prevent entering maximize mode if an erroneous accelerometer
// reading makes the lid appear to be fully open when the user is opening the
// lid from a closed position.
const int kLidRecentlyOpenedDurationSeconds = 2;

#if defined(OS_CHROMEOS)
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
const float kHingeVerticalSmoothingMaximum = 9.5f;

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
                 update.get(chromeos::ACCELEROMETER_SOURCE_SCREEN)).Length()) <=
         kNoisyMagnitudeDeviation;
}
#endif  // OS_CHROMEOS

}  // namespace

MaximizeModeController::MaximizeModeController()
    : have_seen_accelerometer_data_(false),
      lid_open_past_180_(false),
      touchview_usage_interval_start_time_(base::Time::Now()),
      tick_clock_(new base::DefaultTickClock()),
      lid_is_closed_(false) {
  Shell* shell = Shell::GetInstance();
  shell->AddShellObserver(this);
  shell->metrics()->RecordUserMetricsAction(
      ash::UMA_MAXIMIZE_MODE_INITIALLY_DISABLED);

#if defined(OS_CHROMEOS)
  // TODO(jonross): Do not create MaximizeModeController if the flag is
  // unavailable. This will require refactoring
  // IsMaximizeModeWindowManagerEnabled to check for the existance of the
  // controller.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTouchView)) {
    chromeos::AccelerometerReader::GetInstance()->AddObserver(this);
    shell->window_tree_host_manager()->AddObserver(this);
  }
  chromeos::DBusThreadManager::Get()->
      GetPowerManagerClient()->AddObserver(this);
#endif  // OS_CHROMEOS
}

MaximizeModeController::~MaximizeModeController() {
  Shell::GetInstance()->RemoveShellObserver(this);
#if defined(OS_CHROMEOS)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTouchView)) {
    chromeos::AccelerometerReader::GetInstance()->RemoveObserver(this);
    Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);
  }
  chromeos::DBusThreadManager::Get()->
      GetPowerManagerClient()->RemoveObserver(this);
#endif  // OS_CHROMEOS
}

bool MaximizeModeController::CanEnterMaximizeMode() {
  // If we have ever seen accelerometer data, then HandleHingeRotation may
  // trigger maximize mode at some point in the future.
  // The --enable-touch-view-testing switch can also mean that we may enter
  // maximize mode.
  // TODO(mgiuca): This can result in false positives, as it returns true for
  // any device with an accelerometer. Have TouchView-enabled devices explicitly
  // set a flag, and change this implementation to simply return true iff the
  // flag is present (http://crbug.com/457445).
  return have_seen_accelerometer_data_ ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshEnableTouchViewTesting);
}

void MaximizeModeController::EnableMaximizeModeWindowManager(
    bool should_enable) {
  bool is_enabled = !!maximize_mode_window_manager_.get();
  if (should_enable == is_enabled)
    return;

  Shell* shell = Shell::GetInstance();

  if (should_enable) {
    maximize_mode_window_manager_.reset(new MaximizeModeWindowManager());
    // TODO(jonross): Move the maximize mode notifications from ShellObserver
    // to MaximizeModeController::Observer
    shell->metrics()->RecordUserMetricsAction(ash::UMA_MAXIMIZE_MODE_ENABLED);
    shell->OnMaximizeModeStarted();
  } else {
    maximize_mode_window_manager_.reset();
    shell->metrics()->RecordUserMetricsAction(ash::UMA_MAXIMIZE_MODE_DISABLED);
    shell->OnMaximizeModeEnded();
  }
}

bool MaximizeModeController::IsMaximizeModeWindowManagerEnabled() const {
  return maximize_mode_window_manager_.get() != NULL;
}

void MaximizeModeController::AddWindow(aura::Window* window) {
  if (IsMaximizeModeWindowManagerEnabled())
    maximize_mode_window_manager_->AddWindow(window);
}

#if defined(OS_CHROMEOS)
void MaximizeModeController::OnAccelerometerUpdated(
    scoped_refptr<const chromeos::AccelerometerUpdate> update) {
  bool first_accelerometer_update = !have_seen_accelerometer_data_;
  have_seen_accelerometer_data_ = true;

  if (!update->has(chromeos::ACCELEROMETER_SOURCE_SCREEN))
    return;

  if (!display::Display::HasInternalDisplay())
    return;

  if (!Shell::GetInstance()->display_manager()->IsActiveDisplayId(
          display::Display::InternalDisplayId())) {
    return;
  }

  // Whether or not we enter maximize mode affects whether we handle screen
  // rotation, so determine whether to enter maximize mode first.
  if (!update->has(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD)) {
    if (first_accelerometer_update)
      EnterMaximizeMode();
  } else if (ui::IsAccelerometerReadingStable(
                 *update, chromeos::ACCELEROMETER_SOURCE_SCREEN) &&
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

void MaximizeModeController::LidEventReceived(bool open,
                                              const base::TimeTicks& time) {
  if (open)
    last_lid_open_time_ = time;
  lid_is_closed_ = !open;
  LeaveMaximizeMode();
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

  base_smoothed_.Scale(smoothing_ratio);
  base_reading.Scale(1.0f - smoothing_ratio);
  base_smoothed_.Add(base_reading);

  lid_smoothed_.Scale(smoothing_ratio);
  lid_reading.Scale(1.0f - smoothing_ratio);
  lid_smoothed_.Add(lid_reading);

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

  bool is_angle_stable = lid_angle >= kMinStableAngle &&
                         lid_angle <= kMaxStableAngle;

  // Clear the last_lid_open_time_ for a stable reading so that there is less
  // chance of a delay if the lid is moved from the close state to the fully
  // open state very quickly.
  if (is_angle_stable)
    last_lid_open_time_ = base::TimeTicks();

  // Toggle maximize mode on or off when corresponding thresholds are passed.
  if (lid_open_past_180_ && is_angle_stable &&
      lid_angle <= kExitMaximizeModeAngle) {
    lid_open_past_180_ = false;
    LeaveMaximizeMode();
  } else if (!lid_open_past_180_ && !lid_is_closed_ &&
             lid_angle >= kEnterMaximizeModeAngle &&
             (is_angle_stable || !WasLidOpenedRecently())) {
    lid_open_past_180_ = true;
    EnterMaximizeMode();
  }
}
#endif  // OS_CHROMEOS

void MaximizeModeController::EnterMaximizeMode() {
  // Always reset first to avoid creation before destruction of a previous
  // object.
  event_blocker_.reset();
#if defined(USE_X11)
  event_blocker_.reset(new ScopedDisableInternalMouseAndKeyboardX11);
#elif defined(USE_OZONE)
  event_blocker_.reset(new ScopedDisableInternalMouseAndKeyboardOzone);
#endif

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTouchViewTesting)) {
    // We don't let accelerometer updates interfere with the maximize mode
    // status as set by the touch-view-testing keyboard shortcut.
    return;
  }

  if (IsMaximizeModeWindowManagerEnabled())
    return;
  EnableMaximizeModeWindowManager(true);
}

void MaximizeModeController::LeaveMaximizeMode() {
  event_blocker_.reset();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTouchViewTesting)) {
    // We don't let accelerometer updates interfere with the maximize mode
    // status as set by the touch-view-testing keyboard shortcut.
    return;
  }

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

void MaximizeModeController::OnDisplayConfigurationChanged() {
  if (!display::Display::HasInternalDisplay() ||
      !Shell::GetInstance()->display_manager()->IsActiveDisplayId(
          display::Display::InternalDisplayId())) {
    LeaveMaximizeMode();
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

void MaximizeModeController::OnAppTerminating() {
  // The system is about to shut down, so record TouchView usage interval
  // metrics based on whether TouchView mode is currently active.
  RecordTouchViewUsageInterval(CurrentTouchViewIntervalType());

  if (CanEnterMaximizeMode()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchView.TouchViewActiveTotal",
        total_touchview_time_.InMinutes(),
        1, base::TimeDelta::FromDays(7).InMinutes(), 50);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Ash.TouchView.TouchViewInactiveTotal",
        total_non_touchview_time_.InMinutes(),
        1, base::TimeDelta::FromDays(7).InMinutes(), 50);
    base::TimeDelta total_runtime = total_touchview_time_ +
        total_non_touchview_time_;
    if (total_runtime.InSeconds() > 0) {
      UMA_HISTOGRAM_PERCENTAGE("Ash.TouchView.TouchViewActivePercentage",
          100 * total_touchview_time_.InSeconds() / total_runtime.InSeconds());
    }
  }
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
