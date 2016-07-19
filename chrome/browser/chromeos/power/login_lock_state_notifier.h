// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_LOGIN_LOCK_STATE_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_LOGIN_LOCK_STATE_NOTIFIER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {

// Listens for login and screen lock events and passes them to
// the Aura shell's PowerButtonController class.
class LoginLockStateNotifier : public content::NotificationObserver {
 public:
  // This class registers/unregisters itself as an observer in ctor/dtor.
  LoginLockStateNotifier();
  ~LoginLockStateNotifier() override;

 private:
  // content::NotificationObserver implementation:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(LoginLockStateNotifier);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_LOGIN_LOCK_STATE_NOTIFIER_H_
