// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_IDLE_LOGOUT_H_
#define CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_IDLE_LOGOUT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/timer/timer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/wm/core/user_activity_observer.h"

namespace chromeos {

class KioskModeIdleLogout : public wm::UserActivityObserver,
                            public content::NotificationObserver {
 public:
  static void Initialize();

  KioskModeIdleLogout();
  virtual ~KioskModeIdleLogout();

 private:
  friend class KioskModeIdleLogoutTest;

  // Really initialize idle logout when KioskModeHelper is initialized.
  void Setup();

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // wm::UserActivityObserver overrides:
  virtual void OnUserActivity(const ui::Event* event) OVERRIDE;

  // Begins listening for user activity and calls ResetTimer().
  void Start();

  // Resets |timer_| to fire when the logout dialog should be shown.
  void ResetTimer();

  // Invoked by |timer_| to display the logout dialog.
  void OnTimeout();

  content::NotificationRegistrar registrar_;

  base::OneShotTimer<KioskModeIdleLogout> timer_;

  DISALLOW_COPY_AND_ASSIGN(KioskModeIdleLogout);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_KIOSK_MODE_KIOSK_MODE_IDLE_LOGOUT_H_
