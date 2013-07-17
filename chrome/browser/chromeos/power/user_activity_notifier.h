// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_USER_ACTIVITY_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_USER_ACTIVITY_NOTIFIER_H_

#include "ash/wm/user_activity_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"

namespace chromeos {

// Notifies the power manager when the user is active.
class UserActivityNotifier : public ash::UserActivityObserver {
 public:
  UserActivityNotifier();
  virtual ~UserActivityNotifier();

  // ash::UserActivityObserver implementation.
  virtual void OnUserActivity(const ui::Event* event) OVERRIDE;

 private:
  // Last time that the power manager was notified.
  base::TimeTicks last_notify_time_;

  DISALLOW_COPY_AND_ASSIGN(UserActivityNotifier);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_USER_ACTIVITY_NOTIFIER_H_
