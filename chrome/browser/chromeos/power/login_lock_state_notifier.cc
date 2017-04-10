// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/login_lock_state_notifier.h"

#include "ash/shell.h"
#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

namespace ash {
class LockStateControllerDelegate;
}

namespace chromeos {

LoginLockStateNotifier::LoginLockStateNotifier() {
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
}

LoginLockStateNotifier::~LoginLockStateNotifier() {}

void LoginLockStateNotifier::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_APP_TERMINATING:
      ash::Shell::Get()->OnAppTerminating();
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

}  // namespace chromeos
