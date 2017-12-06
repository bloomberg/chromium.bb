// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/backlights_forced_off_setter.h"

#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/power/scoped_backlights_forced_off.h"
#include "ash/touch/touch_devices_controller.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace ash {

BacklightsForcedOffSetter::BacklightsForcedOffSetter()
    : power_manager_observer_(this), weak_ptr_factory_(this) {
  InitDisableTouchscreenWhileScreenOff();

  power_manager_observer_.Add(
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient());
  GetInitialBacklightsForcedOff();
}

BacklightsForcedOffSetter::~BacklightsForcedOffSetter() {
  if (active_backlights_forced_off_count_ > 0) {
    chromeos::DBusThreadManager::Get()
        ->GetPowerManagerClient()
        ->SetBacklightsForcedOff(false);
  }
}

void BacklightsForcedOffSetter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BacklightsForcedOffSetter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::unique_ptr<ScopedBacklightsForcedOff>
BacklightsForcedOffSetter::ForceBacklightsOff() {
  auto scoped_backlights_forced_off =
      std::make_unique<ScopedBacklightsForcedOff>(base::BindOnce(
          &BacklightsForcedOffSetter::OnScopedBacklightsForcedOffDestroyed,
          weak_ptr_factory_.GetWeakPtr()));
  ++active_backlights_forced_off_count_;
  SetBacklightsForcedOff(true);
  return scoped_backlights_forced_off;
}

void BacklightsForcedOffSetter::BrightnessChanged(int level,
                                                  bool user_initiated) {
  const ScreenState old_state = screen_state_;
  if (level != 0)
    screen_state_ = ScreenState::ON;
  else
    screen_state_ = user_initiated ? ScreenState::OFF : ScreenState::OFF_AUTO;

  if (screen_state_ != old_state) {
    for (auto& observer : observers_)
      observer.OnScreenStateChanged(screen_state_);
  }

  // Disable the touchscreen when the screen is turned off due to inactivity:
  // https://crbug.com/743291
  if ((screen_state_ == ScreenState::OFF_AUTO) !=
          (old_state == ScreenState::OFF_AUTO) &&
      disable_touchscreen_while_screen_off_) {
    UpdateTouchscreenStatus();
  }
}

void BacklightsForcedOffSetter::PowerManagerRestarted() {
  if (backlights_forced_off_.has_value()) {
    chromeos::DBusThreadManager::Get()
        ->GetPowerManagerClient()
        ->SetBacklightsForcedOff(backlights_forced_off_.value());
  } else {
    GetInitialBacklightsForcedOff();
  }
}

void BacklightsForcedOffSetter::ResetForTest() {
  if (active_backlights_forced_off_count_ > 0) {
    chromeos::DBusThreadManager::Get()
        ->GetPowerManagerClient()
        ->SetBacklightsForcedOff(false);
  }

  // Cancel all backlights forced off requests.
  weak_ptr_factory_.InvalidateWeakPtrs();
  active_backlights_forced_off_count_ = 0;

  InitDisableTouchscreenWhileScreenOff();

  backlights_forced_off_.reset();
  GetInitialBacklightsForcedOff();
}

void BacklightsForcedOffSetter::InitDisableTouchscreenWhileScreenOff() {
  disable_touchscreen_while_screen_off_ =
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTouchscreenUsableWhileScreenOff);
}

void BacklightsForcedOffSetter::GetInitialBacklightsForcedOff() {
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->GetBacklightsForcedOff(base::BindOnce(
          &BacklightsForcedOffSetter::OnGotInitialBacklightsForcedOff,
          weak_ptr_factory_.GetWeakPtr()));
}

void BacklightsForcedOffSetter::OnGotInitialBacklightsForcedOff(
    base::Optional<bool> is_forced_off) {
  if (backlights_forced_off_.has_value() || !is_forced_off.has_value())
    return;

  backlights_forced_off_ = is_forced_off;

  UpdateTouchscreenStatus();

  for (auto& observer : observers_)
    observer.OnBacklightsForcedOffChanged(backlights_forced_off_.value());
}

void BacklightsForcedOffSetter::OnScopedBacklightsForcedOffDestroyed() {
  DCHECK_GT(active_backlights_forced_off_count_, 0);

  --active_backlights_forced_off_count_;
  SetBacklightsForcedOff(active_backlights_forced_off_count_ > 0);
}

void BacklightsForcedOffSetter::SetBacklightsForcedOff(bool forced_off) {
  if (backlights_forced_off_.has_value() &&
      backlights_forced_off_.value() == forced_off) {
    return;
  }

  backlights_forced_off_ = forced_off;
  chromeos::DBusThreadManager::Get()
      ->GetPowerManagerClient()
      ->SetBacklightsForcedOff(forced_off);

  UpdateTouchscreenStatus();

  for (auto& observer : observers_)
    observer.OnBacklightsForcedOffChanged(backlights_forced_off_.value());
}

void BacklightsForcedOffSetter::UpdateTouchscreenStatus() {
  const bool disable_touchscreen = backlights_forced_off_.value_or(false) ||
                                   (screen_state_ == ScreenState::OFF_AUTO &&
                                    disable_touchscreen_while_screen_off_);
  Shell::Get()->touch_devices_controller()->SetTouchscreenEnabled(
      !disable_touchscreen, TouchscreenEnabledSource::GLOBAL);
}

}  // namespace ash
