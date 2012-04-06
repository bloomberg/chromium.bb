// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_IDLE_LOGOUT_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_IDLE_LOGOUT_H_
#pragma once

#include "base/basictypes.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace browser {

// Shows or closes the logout dialog for Kiosk Mode.
void ShowIdleLogoutDialog();
void CloseIdleLogoutDialog();

}  // namespace browser

namespace chromeos {

class KioskModeIdleLogout : public PowerManagerClient::Observer,
                            public content::NotificationObserver {
 public:
  KioskModeIdleLogout();

  // Really initialize idle logout when KioskModeHelper is initialized.
  void Setup();

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // PowerManagerClient::Observer overrides:
  virtual void IdleNotify(int64 threshold) OVERRIDE;
  virtual void ActiveNotify() OVERRIDE;

 private:
  friend class KioskModeIdleLogoutTest;
  content::NotificationRegistrar registrar_;

  void SetupIdleNotifications();
  void RequestNextActiveNotification();
  void RequestNextIdleNotification();

  DISALLOW_COPY_AND_ASSIGN(KioskModeIdleLogout);
};

void InitializeKioskModeIdleLogout();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_IDLE_LOGOUT_H_
