// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/preferences.h"

#include "base/string_util.h"
#include "chrome/browser/chromeos/touchpad.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "unicode/timezone.h"

namespace chromeos {

// static
void Preferences::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterStringPref(prefs::kTimeZone, L"US/Pacific");
  prefs->RegisterBooleanPref(prefs::kTapToClickEnabled, false);
  prefs->RegisterBooleanPref(prefs::kVertEdgeScrollEnabled, false);
  prefs->RegisterIntegerPref(prefs::kTouchpadSpeedFactor, 5);
  prefs->RegisterIntegerPref(prefs::kTouchpadSensitivity, 5);
}

void Preferences::Init(PrefService* prefs) {
  timezone_.Init(prefs::kTimeZone, prefs, this);
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled, prefs, this);
  vert_edge_scroll_enabled_.Init(prefs::kVertEdgeScrollEnabled, prefs, this);
  speed_factor_.Init(prefs::kTouchpadSpeedFactor, prefs, this);
  sensitivity_.Init(prefs::kTouchpadSensitivity, prefs, this);

  // Initialize touchpad settings to what's saved in user preferences.
  NotifyPrefChanged(NULL);
}

void Preferences::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED)
    NotifyPrefChanged(Details<std::wstring>(details).ptr());
}

void Preferences::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kTimeZone)
    SetTimeZone(timezone_.GetValue());
  if (!pref_name || *pref_name == prefs::kTapToClickEnabled)
    Touchpad::SetTapToClick(tap_to_click_enabled_.GetValue());
  if (!pref_name || *pref_name == prefs::kVertEdgeScrollEnabled)
    Touchpad::SetVertEdgeScroll(vert_edge_scroll_enabled_.GetValue());
  if (!pref_name || *pref_name == prefs::kTouchpadSpeedFactor)
    Touchpad::SetSpeedFactor(speed_factor_.GetValue());
  if (!pref_name || *pref_name == prefs::kTouchpadSensitivity)
    Touchpad::SetSensitivity(sensitivity_.GetValue());
}

void Preferences::SetTimeZone(const std::wstring& id) {
  icu::TimeZone* timezone = icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8(WideToASCII(id)));
  icu::TimeZone::adoptDefault(timezone);
}

}  // namespace chromeos
