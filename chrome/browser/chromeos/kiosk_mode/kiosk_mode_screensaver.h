// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_SCREENSAVER_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_SCREENSAVER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {

class KioskModeScreensaver : public PowerManagerClient::Observer,
                             public content::NotificationObserver {
 public:
  KioskModeScreensaver();
  virtual ~KioskModeScreensaver();

  // Really initialize screensaver when KioskModeHelper is initialized.
  void Setup();

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // PowerManagerClient::Observer overrides:
  virtual void ActiveNotify() OVERRIDE;

 private:
  friend class KioskModeScreensaverTest;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(KioskModeScreensaver);
};

void InitializeKioskModeScreensaver();
void ShutdownKioskModeScreensaver();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_SCREENSAVER_H_
