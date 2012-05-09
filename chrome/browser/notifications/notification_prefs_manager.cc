// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_prefs_manager.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

// static
void NotificationPrefsManager::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kDesktopNotificationPosition,
                             BalloonCollection::DEFAULT_POSITION);
#if defined(OS_CHROMEOS)
  // Option menu for changing desktop notification position on ChromeOS is
  // disabled. Force preference to default.
  prefs->SetInteger(prefs::kDesktopNotificationPosition,
                    BalloonCollection::DEFAULT_POSITION);
#endif
}
