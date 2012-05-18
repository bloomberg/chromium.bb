// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "grit/theme_resources.h"

// How long to wait between checks for whether the user has been idle.
static const int kIdleRepeatingTimerWait = 10;  // Minutes (seconds if testing).

// How much idle time (since last input even was detected) must have passed
// until we notify that a critical update has occurred.
static const int kIdleAmount = 2;  // Hours (or seconds, if testing).

bool UseTestingIntervals() {
  // If a command line parameter specifying how long the upgrade check should
  // be, we assume it is for testing and switch to using seconds instead of
  // hours.
  const CommandLine& cmd_line = *CommandLine::ForCurrentProcess();
  return !cmd_line.GetSwitchValueASCII(
      switches::kCheckForUpdateIntervalSec).empty();
}

// static
void UpgradeDetector::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kRestartLastSessionOnShutdown, false);
  prefs->RegisterBooleanPref(prefs::kWasRestarted, false);
}

int UpgradeDetector::GetIconResourceID(UpgradeNotificationIconType type) {
  bool badge = type == UPGRADE_ICON_TYPE_BADGE;
  switch (upgrade_notification_stage_) {
    case UPGRADE_ANNOYANCE_CRITICAL:
      // The critical annoyance state, somewhat ironically, re-purposes the
      // icon for the second highest severity state, since that state has the
      // icon most closely resembling the one requested of this feature and the
      // critical annoyance is never part of the sliding scale of severity
      // anyway (always shown on its own).
      return badge ? IDR_UPDATE_BADGE3 : IDR_UPDATE_MENU3;
    case UPGRADE_ANNOYANCE_SEVERE:
      return badge ? IDR_UPDATE_BADGE4 : IDR_UPDATE_MENU4;
    case UPGRADE_ANNOYANCE_HIGH:
      return badge ? IDR_UPDATE_BADGE3 : IDR_UPDATE_MENU3;
    case UPGRADE_ANNOYANCE_ELEVATED:
      return badge ? IDR_UPDATE_BADGE2 : IDR_UPDATE_MENU2;
    case UPGRADE_ANNOYANCE_LOW:
      return badge ? IDR_UPDATE_BADGE : IDR_UPDATE_MENU;
    default:
      return 0;
  }
}

UpgradeDetector::UpgradeDetector()
    : is_critical_upgrade_(false),
      critical_update_acknowledged_(false),
      upgrade_notification_stage_(UPGRADE_ANNOYANCE_NONE),
      notify_upgrade_(false) {
}

UpgradeDetector::~UpgradeDetector() {
}

void UpgradeDetector::NotifyUpgradeDetected() {
  upgrade_detected_time_ = base::Time::Now();
  critical_update_acknowledged_ = false;
}

void UpgradeDetector::NotifyUpgradeRecommended() {
  notify_upgrade_ = true;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
      content::Source<UpgradeDetector>(this),
      content::NotificationService::NoDetails());

  if (is_critical_upgrade_) {
    int idle_timer = UseTestingIntervals() ?
        kIdleRepeatingTimerWait :
        kIdleRepeatingTimerWait * 60;  // To minutes.
    idle_check_timer_.Start(FROM_HERE,
        base::TimeDelta::FromSeconds(idle_timer),
        this, &UpgradeDetector::CheckIdle);
  }
}

void UpgradeDetector::CheckIdle() {
  // CalculateIdleState expects an interval in seconds.
  int idle_time_allowed = UseTestingIntervals() ? kIdleAmount :
                                                  kIdleAmount * 60 * 60;

  CalculateIdleState(
      idle_time_allowed, base::Bind(&UpgradeDetector::IdleCallback,
                                    base::Unretained(this)));
}

void UpgradeDetector::IdleCallback(IdleState state) {
  switch (state) {
    case IDLE_STATE_LOCKED:
      // Computer is locked, auto-restart.
      idle_check_timer_.Stop();
      browser::AttemptRestart();
      break;
    case IDLE_STATE_IDLE:
      // Computer has been idle for long enough, show warning.
      idle_check_timer_.Stop();
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_CRITICAL_UPGRADE_INSTALLED,
          content::Source<UpgradeDetector>(this),
          content::NotificationService::NoDetails());
      break;
    case IDLE_STATE_ACTIVE:
    case IDLE_STATE_UNKNOWN:
      break;
    default:
      NOTREACHED();  // Need to add any new value above (either providing
                     // automatic restart or show notification to user).
      break;
  }
}
