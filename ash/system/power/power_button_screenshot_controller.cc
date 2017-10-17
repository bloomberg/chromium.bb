// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_button_screenshot_controller.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/system/power/power_button_controller.h"
#include "ash/system/power/tablet_power_button_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/time/tick_clock.h"
#include "ui/events/event.h"

namespace ash {

namespace {

bool IsTabletMode() {
  return Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

}  // namespace

constexpr base::TimeDelta
    PowerButtonScreenshotController::kScreenshotChordDelay;

PowerButtonScreenshotController::PowerButtonScreenshotController(
    TabletPowerButtonController* tablet_controller,
    base::TickClock* tick_clock,
    bool force_clamshell_power_button)
    : tablet_controller_(tablet_controller),
      tick_clock_(tick_clock),
      force_clamshell_power_button_(force_clamshell_power_button) {
  DCHECK(tick_clock_);
  // Using prepend to make sure this event handler is put in front of
  // AcceleratorFilter. See Shell::Init().
  Shell::Get()->PrependPreTargetHandler(this);
}

PowerButtonScreenshotController::~PowerButtonScreenshotController() {
  Shell::Get()->RemovePreTargetHandler(this);
}

bool PowerButtonScreenshotController::OnPowerButtonEvent(
    bool down,
    const base::TimeTicks& timestamp) {
  if (!IsTabletMode())
    return false;

  power_button_pressed_ = down;
  if (power_button_pressed_) {
    volume_down_timer_.Stop();
    power_button_pressed_time_ = tick_clock_->NowTicks();
    if (InterceptScreenshotChord())
      return true;
  }

  // If forced clamshell power button releases when
  // |clamshell_power_button_timer_| is still running, stop the timer to
  // invalidate this delayed power button behavior.
  if (!down) {
    clamshell_power_button_timer_.Stop();
    return false;
  }

  // If forced clamshell power button, start a timer waiting volume down key
  // pressed. When timing out, perform this delayed power button behavior.
  if (force_clamshell_power_button_ && !volume_down_key_pressed_ && down) {
    clamshell_power_button_timer_.Start(
        FROM_HERE, kScreenshotChordDelay, this,
        &PowerButtonScreenshotController::OnClamshellPowerButtonTimeout);
    return true;
  }

  // If volume key is pressed, mark power button as consumed. This invalidates
  // other power button's behavior when user tries to operate screenshot.
  return volume_down_key_pressed_ || volume_up_key_pressed_;
}

void PowerButtonScreenshotController::OnKeyEvent(ui::KeyEvent* event) {
  if (!IsTabletMode())
    return;

  ui::KeyboardCode key_code = event->key_code();
  if (key_code != ui::VKEY_VOLUME_DOWN && key_code != ui::VKEY_VOLUME_UP)
    return;
  clamshell_power_button_timer_.Stop();

  if (key_code == ui::VKEY_VOLUME_DOWN) {
    if (event->type() == ui::ET_KEY_PRESSED) {
      if (!volume_down_key_pressed_) {
        volume_down_key_pressed_ = true;
        volume_down_key_pressed_time_ = tick_clock_->NowTicks();
        consume_volume_down_ = false;

        InterceptScreenshotChord();
      }

      // Do not propagate volume down key pressed event if the first
      // one is consumed by screenshot.
      if (consume_volume_down_)
        event->StopPropagation();
    } else {
      volume_down_key_pressed_ = false;
    }
  }

  if (key_code == ui::VKEY_VOLUME_UP)
    volume_up_key_pressed_ = event->type() == ui::ET_KEY_PRESSED;

  // When volume key is pressed, cancel the ongoing tablet power button
  // behavior.
  if ((volume_down_key_pressed_ || volume_up_key_pressed_) &&
      tablet_controller_) {
    tablet_controller_->CancelTabletPowerButton();
  }

  // On volume down key pressed while power button not pressed yet state, do not
  // propagate volume down key pressed event for chord delay time. Start the
  // timer to wait power button pressed for screenshot operation, and on timeout
  // perform the delayed volume down operation.
  if (volume_down_key_pressed_ && !power_button_pressed_) {
    base::TimeTicks now = tick_clock_->NowTicks();
    if (now <= volume_down_key_pressed_time_ + kScreenshotChordDelay) {
      event->StopPropagation();

      if (!volume_down_timer_.IsRunning()) {
        volume_down_timer_.Start(
            FROM_HERE, kScreenshotChordDelay, this,
            &PowerButtonScreenshotController::OnVolumeDownTimeout);
      }
    }
  }
}

bool PowerButtonScreenshotController::InterceptScreenshotChord() {
  if (volume_down_key_pressed_ && power_button_pressed_) {
    base::TimeTicks now = tick_clock_->NowTicks();
    if (now <= volume_down_key_pressed_time_ + kScreenshotChordDelay &&
        now <= power_button_pressed_time_ + kScreenshotChordDelay) {
      Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
          TAKE_SCREENSHOT);
      consume_volume_down_ = true;
      return true;
    }
  }
  return false;
}

void PowerButtonScreenshotController::OnVolumeDownTimeout() {
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(VOLUME_DOWN);
}

void PowerButtonScreenshotController::OnClamshellPowerButtonTimeout() {
  Shell::Get()->power_button_controller()->OnPowerButtonEvent(
      true, tick_clock_->NowTicks());
}

}  // namespace ash
