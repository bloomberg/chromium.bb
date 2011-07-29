// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"
#include "grit/theme_resources.h"

// static
void UpgradeDetector::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kRestartLastSessionOnShutdown, false);
}

int UpgradeDetector::GetIconResourceID(UpgradeNotificationIconType type) {
  bool badge = type == UPGRADE_ICON_TYPE_BADGE;
  switch (upgrade_notification_stage_) {
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
    : upgrade_notification_stage_(UPGRADE_ANNOYANCE_NONE),
      notify_upgrade_(false) {
}

UpgradeDetector::~UpgradeDetector() {
}

void UpgradeDetector::NotifyUpgradeDetected() {
  upgrade_detected_time_ = base::Time::Now();
}

void UpgradeDetector::NotifyUpgradeRecommended() {
  notify_upgrade_ = true;

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_UPGRADE_RECOMMENDED,
      Source<UpgradeDetector>(this),
      NotificationService::NoDetails());
}
