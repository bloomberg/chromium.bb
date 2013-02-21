// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_prefs_manager.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

NotificationPrefsManager::NotificationPrefsManager(PrefService* prefs) {
#if defined(OS_CHROMEOS)
  static bool have_cleared = false;

  if (!have_cleared) {
    // Option menu for changing desktop notification position on ChromeOS is
    // disabled. Force preference to default.
    prefs->ClearPref(prefs::kDesktopNotificationPosition);
    have_cleared = true;
  }
#endif
}

// static
void NotificationPrefsManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kDesktopNotificationPosition,
                                BalloonCollection::DEFAULT_POSITION);
}
