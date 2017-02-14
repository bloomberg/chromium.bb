// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/power_event_observer.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "ash/wm/power_button_controller.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/compositor/compositor.h"
#include "ui/display/manager/chromeos/display_configurator.h"

namespace ash {

namespace {

// Tells the compositor for each of the displays to finish all pending
// rendering requests and block any new ones.
void StopRenderingRequests() {
  for (aura::Window* window : Shell::GetAllRootWindows()) {
    ui::Compositor* compositor = window->GetHost()->compositor();
    compositor->SetVisible(false);
  }
}

// Tells the compositor for each of the displays to resume sending rendering
// requests to the GPU.
void ResumeRenderingRequests() {
  for (aura::Window* window : Shell::GetAllRootWindows())
    window->GetHost()->compositor()->SetVisible(true);
}

void OnSuspendDisplaysCompleted(const base::Closure& suspend_callback,
                                bool status) {
  suspend_callback.Run();
}

}  // namespace

PowerEventObserver::PowerEventObserver()
    : screen_locked_(false), waiting_for_lock_screen_animations_(false) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->AddObserver(
      this);
}

PowerEventObserver::~PowerEventObserver() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
  chromeos::DBusThreadManager::Get()->GetSessionManagerClient()->RemoveObserver(
      this);
}

void PowerEventObserver::OnLockAnimationsComplete() {
  VLOG(1) << "Screen locker animations have completed.";
  waiting_for_lock_screen_animations_ = false;

  if (!screen_lock_callback_.is_null()) {
    StopRenderingRequests();

    screen_lock_callback_.Run();
    screen_lock_callback_.Reset();
  }
}

void PowerEventObserver::BrightnessChanged(int level, bool user_initiated) {
  Shell::GetInstance()->power_button_controller()->OnScreenBrightnessChanged(
      static_cast<double>(level));
}

void PowerEventObserver::SuspendImminent() {
  SessionStateDelegate* delegate = WmShell::Get()->GetSessionStateDelegate();

  // This class is responsible for disabling all rendering requests at suspend
  // time and then enabling them at resume time.  When the
  // auto-screen-lock pref is not set this is easy to do since
  // StopRenderingRequests() is just called directly from this function.  If the
  // auto-screen-lock pref _is_ set, then the suspend needs to be delayed
  // until the lock screen is fully visible.  While it is sufficient from a
  // security perspective to block only until the lock screen is ready, which
  // guarantees that the contents of the user's screen are no longer visible,
  // this leads to poor UX on the first resume since neither the user pod nor
  // the header bar will be visible for a few hundred milliseconds until the GPU
  // process starts rendering again.  To deal with this, the suspend is delayed
  // until all the lock screen animations have completed and the suspend request
  // is unblocked from OnLockAnimationsComplete().
  if (!screen_locked_ && delegate->ShouldLockScreenAutomatically() &&
      delegate->CanLockScreen()) {
    screen_lock_callback_ = chromeos::DBusThreadManager::Get()
                                ->GetPowerManagerClient()
                                ->GetSuspendReadinessCallback();
    VLOG(1) << "Requesting screen lock from PowerEventObserver";
    chromeos::DBusThreadManager::Get()
        ->GetSessionManagerClient()
        ->RequestLockScreen();
  } else if (waiting_for_lock_screen_animations_) {
    // The lock-before-suspending pref has been set and the lock screen is ready
    // but the animations have not completed yet.  This can happen if a suspend
    // request is canceled after the lock screen is ready but before the
    // animations have completed and then another suspend request is immediately
    // started.  In practice, it is highly unlikely that this will ever happen
    // but it's better to be safe since the cost of not dealing with it properly
    // is a memory leak in the GPU and weird artifacts on the screen.
    screen_lock_callback_ = chromeos::DBusThreadManager::Get()
                                ->GetPowerManagerClient()
                                ->GetSuspendReadinessCallback();
  } else {
    // The lock-before-suspending pref is not set or the screen has already been
    // locked and the animations have completed.  Rendering can be stopped now.
    StopRenderingRequests();
  }

  ui::UserActivityDetector::Get()->OnDisplayPowerChanging();

  // TODO(derat): After mus exposes a method for suspending displays, call it
  // here: http://crbug.com/692193
  if (!WmShell::Get()->IsRunningInMash()) {
    Shell::GetInstance()->display_configurator()->SuspendDisplays(base::Bind(
        &OnSuspendDisplaysCompleted, chromeos::DBusThreadManager::Get()
                                         ->GetPowerManagerClient()
                                         ->GetSuspendReadinessCallback()));
  }
}

void PowerEventObserver::SuspendDone(const base::TimeDelta& sleep_duration) {
  // TODO(derat): After mus exposes a method for resuming displays, call it
  // here: http://crbug.com/692193
  if (!WmShell::Get()->IsRunningInMash())
    Shell::GetInstance()->display_configurator()->ResumeDisplays();
  WmShell::Get()->system_tray_notifier()->NotifyRefreshClock();

  // If the suspend request was being blocked while waiting for the lock
  // animation to complete, clear the blocker since the suspend has already
  // completed.  This prevents rendering requests from being blocked after a
  // resume if the lock screen took too long to show.
  screen_lock_callback_.Reset();

  ResumeRenderingRequests();
}

void PowerEventObserver::ScreenIsLocked() {
  screen_locked_ = true;
  waiting_for_lock_screen_animations_ = true;

  // The screen is now locked but the pending suspend, if any, will be blocked
  // until all the animations have completed.
  if (!screen_lock_callback_.is_null()) {
    VLOG(1) << "Screen locked due to suspend";
  } else {
    VLOG(1) << "Screen locked without suspend";
  }
}

void PowerEventObserver::ScreenIsUnlocked() {
  screen_locked_ = false;
}

}  // namespace ash
