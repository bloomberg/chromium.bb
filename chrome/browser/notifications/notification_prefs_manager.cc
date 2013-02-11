// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_prefs_manager.h"

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

// static
void NotificationPrefsManager::RegisterPrefs(PrefService* prefs,
                                             PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kDesktopNotificationPosition,
                                BalloonCollection::DEFAULT_POSITION);
#if defined(OS_CHROMEOS)
  // Option menu for changing desktop notification position on ChromeOS is
  // disabled. Force preference to default.
  //
  // TODO(joi): This shouldn't be done during registration;
  // registration should all be done up front, before there even
  // exists a PrefService.
  prefs->SetInteger(prefs::kDesktopNotificationPosition,
                    BalloonCollection::DEFAULT_POSITION);
#endif
}
