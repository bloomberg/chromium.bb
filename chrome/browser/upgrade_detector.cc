// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

// How long to wait between checks for whether the user has been idle.
static const int kIdleRepeatingTimerWait = 10;  // Minutes (seconds if testing).

// How much idle time (since last input even was detected) must have passed
// until we notify that a critical update has occurred.
static const int kIdleAmount = 2;  // Hours (or seconds, if testing).

bool UseTestingIntervals() {
  // If a command line parameter specifying how long the upgrade check should
  // be, we assume it is for testing and switch to using seconds instead of
  // hours.
  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  return !cmd_line.GetSwitchValueASCII(
      switches::kCheckForUpdateIntervalSec).empty();
}

// static
void UpgradeDetector::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kAttemptedToEnableAutoupdate, false);
}

gfx::Image UpgradeDetector::GetIcon() {
  SkColor color = gfx::kPlaceholderColor;
  switch (upgrade_notification_stage_) {
    case UPGRADE_ANNOYANCE_NONE:
      return gfx::Image();
    case UPGRADE_ANNOYANCE_LOW:
      color = gfx::kGoogleGreen700;
      break;
    case UPGRADE_ANNOYANCE_ELEVATED:
      color = gfx::kGoogleYellow700;
      break;
    case UPGRADE_ANNOYANCE_HIGH:
    case UPGRADE_ANNOYANCE_SEVERE:
    case UPGRADE_ANNOYANCE_CRITICAL:
      color = gfx::kGoogleRed700;
      break;
  }
  DCHECK_NE(gfx::kPlaceholderColor, color);

  return gfx::Image(gfx::CreateVectorIcon(kBrowserToolsUpdateIcon, color));
}

UpgradeDetector::UpgradeDetector()
    : upgrade_available_(UPGRADE_AVAILABLE_NONE),
      best_effort_experiment_updates_available_(false),
      critical_experiment_updates_available_(false),
      critical_update_acknowledged_(false),
      is_factory_reset_required_(false),
      upgrade_notification_stage_(UPGRADE_ANNOYANCE_NONE),
      notify_upgrade_(false) {
}

UpgradeDetector::~UpgradeDetector() {
}

void UpgradeDetector::NotifyOutdatedInstall() {
  for (auto& observer : observer_list_)
    observer.OnOutdatedInstall();
}

void UpgradeDetector::NotifyOutdatedInstallNoAutoUpdate() {
  for (auto& observer : observer_list_)
    observer.OnOutdatedInstallNoAutoUpdate();
}

void UpgradeDetector::NotifyUpgrade() {
  notify_upgrade_ = true;

  NotifyUpgradeRecommended();
  if (upgrade_available_ == UPGRADE_NEEDED_OUTDATED_INSTALL) {
    NotifyOutdatedInstall();
  } else if (upgrade_available_ == UPGRADE_NEEDED_OUTDATED_INSTALL_NO_AU) {
    NotifyOutdatedInstallNoAutoUpdate();
  } else if (upgrade_available_ == UPGRADE_AVAILABLE_CRITICAL ||
             critical_experiment_updates_available_) {
    TriggerCriticalUpdate();
  }
}

void UpgradeDetector::NotifyUpgradeRecommended() {
  for (auto& observer : observer_list_)
    observer.OnUpgradeRecommended();
}

void UpgradeDetector::NotifyCriticalUpgradeInstalled() {
  for (auto& observer : observer_list_)
    observer.OnCriticalUpgradeInstalled();
}

void UpgradeDetector::NotifyUpdateOverCellularAvailable() {
  for (auto& observer : observer_list_)
    observer.OnUpdateOverCellularAvailable();
}

void UpgradeDetector::TriggerCriticalUpdate() {
  const base::TimeDelta idle_timer = UseTestingIntervals() ?
      base::TimeDelta::FromSeconds(kIdleRepeatingTimerWait) :
      base::TimeDelta::FromMinutes(kIdleRepeatingTimerWait);
  idle_check_timer_.Start(FROM_HERE, idle_timer, this,
                          &UpgradeDetector::CheckIdle);
}

void UpgradeDetector::CheckIdle() {
  // CalculateIdleState expects an interval in seconds.
  int idle_time_allowed = UseTestingIntervals() ? kIdleAmount :
                                                  kIdleAmount * 60 * 60;

  CalculateIdleState(
      idle_time_allowed, base::Bind(&UpgradeDetector::IdleCallback,
                                    base::Unretained(this)));
}

void UpgradeDetector::IdleCallback(ui::IdleState state) {
  // Don't proceed while an incognito window is open. The timer will still
  // keep firing, so this function will get a chance to re-evaluate this.
  if (chrome::IsIncognitoSessionActive())
    return;

  switch (state) {
    case ui::IDLE_STATE_LOCKED:
      // Computer is locked, auto-restart.
      idle_check_timer_.Stop();
      chrome::AttemptRestart();
      break;
    case ui::IDLE_STATE_IDLE:
      // Computer has been idle for long enough, show warning.
      idle_check_timer_.Stop();
      NotifyCriticalUpgradeInstalled();
      break;
    case ui::IDLE_STATE_ACTIVE:
    case ui::IDLE_STATE_UNKNOWN:
      break;
    default:
      NOTREACHED();  // Need to add any new value above (either providing
                     // automatic restart or show notification to user).
      break;
  }
}

void UpgradeDetector::AddObserver(UpgradeObserver* observer) {
  observer_list_.AddObserver(observer);
}

void UpgradeDetector::RemoveObserver(UpgradeObserver* observer) {
  observer_list_.RemoveObserver(observer);
}
